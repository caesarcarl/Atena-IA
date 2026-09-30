#include "atena/runtime.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace {
constexpr uint64_t MiB = 1024ULL * 1024ULL;

uint64_t parse_kib_line(const std::unordered_map<std::string, uint64_t>& m, const char* key) {
    const auto it = m.find(key);
    return it == m.end() ? 0ULL : it->second * 1024ULL;
}

#ifndef _WIN32
std::unordered_map<std::string, uint64_t> read_meminfo() {
    std::unordered_map<std::string, uint64_t> out;
    std::ifstream in("/proc/meminfo");
    std::string key, unit;
    uint64_t value = 0;
    while (in >> key >> value >> unit) {
        if (!key.empty() && key.back() == ':') key.pop_back();
        out[key] = value;
    }
    return out;
}
#endif

AtenaMemoryPressure classify_pressure(uint64_t available_bytes,
                                      uint64_t total_bytes,
                                      uint64_t swap_total_bytes,
                                      uint64_t swap_free_bytes) {
    const uint64_t avail_mib = available_bytes / MiB;
    const double avail_ratio = total_bytes
        ? static_cast<double>(available_bytes) / static_cast<double>(total_bytes)
        : 0.0;
    const uint64_t swap_used = swap_total_bytes > swap_free_bytes
        ? swap_total_bytes - swap_free_bytes : 0ULL;
    const double swap_ratio = swap_total_bytes
        ? static_cast<double>(swap_used) / static_cast<double>(swap_total_bytes)
        : 0.0;

    if (avail_mib < 650 || (avail_ratio > 0.0 && avail_ratio < 0.12) ||
        (swap_ratio > 0.75 && avail_mib < 1000)) return ATENA_MEMORY_CRITICAL;
    if (avail_mib < 1200 || (avail_ratio > 0.0 && avail_ratio < 0.22) ||
        (swap_ratio > 0.50 && avail_mib < 1600)) return ATENA_MEMORY_TIGHT;
    if (avail_mib < 2200 || (avail_ratio > 0.0 && avail_ratio < 0.35) ||
        (swap_ratio > 0.25 && avail_mib < 2500)) return ATENA_MEMORY_MODERATE;
    return ATENA_MEMORY_COMFORTABLE;
}

uint32_t clamp_threads(uint32_t logical, uint32_t cap) {
    if (logical == 0) logical = 1;
    return std::max<uint32_t>(1, std::min(logical, cap));
}

uint32_t reserve_host_threads(uint32_t logical) {
    if (logical >= 8) return 2;
    if (logical >= 4) return 1;
    return 0;
}

uint32_t apply_load_headroom(uint32_t threads, const AtenaResourceSnapshot *s) {
    if (!s || s->logical_cpus == 0) return threads;
    const double ratio = s->load_1m / static_cast<double>(s->logical_cpus);
    if (ratio >= 0.90) return std::max<uint32_t>(1, threads / 2);
    if (ratio >= 0.70 && threads > 2) return threads - 2;
    return threads;
}
}

extern "C" const char *atena_memory_pressure_string(AtenaMemoryPressure pressure) {
    switch (pressure) {
        case ATENA_MEMORY_COMFORTABLE: return "comfortable";
        case ATENA_MEMORY_MODERATE: return "moderate";
        case ATENA_MEMORY_TIGHT: return "tight";
        case ATENA_MEMORY_CRITICAL: return "critical";
        default: return "unknown";
    }
}

extern "C" AtenaStatus atena_runtime_snapshot(AtenaResourceSnapshot *out) {
    if (!out) return ATENA_ERR_INVALID_ARGUMENT;
    std::memset(out, 0, sizeof(*out));

#ifdef _WIN32
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (!GlobalMemoryStatusEx(&mem)) return ATENA_ERR_IO;
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    out->ram_total_bytes = static_cast<uint64_t>(mem.ullTotalPhys);
    out->ram_available_bytes = static_cast<uint64_t>(mem.ullAvailPhys);
    out->ram_free_bytes = out->ram_available_bytes;
    out->swap_total_bytes = static_cast<uint64_t>(mem.ullTotalPageFile > mem.ullTotalPhys ? mem.ullTotalPageFile - mem.ullTotalPhys : 0);
    out->swap_free_bytes = static_cast<uint64_t>(mem.ullAvailPageFile > mem.ullAvailPhys ? mem.ullAvailPageFile - mem.ullAvailPhys : 0);
    out->logical_cpus = si.dwNumberOfProcessors ? si.dwNumberOfProcessors : 1;
    std::snprintf(out->platform, sizeof(out->platform), "%s", "windows");
    const char *arch = "unknown";
    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) arch = "x86_64";
    else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) arch = "arm64";
    else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) arch = "x86";
    std::snprintf(out->architecture, sizeof(out->architecture), "%s", arch);
#else
    const auto mem = read_meminfo();
    out->ram_total_bytes = parse_kib_line(mem, "MemTotal");
    out->ram_available_bytes = parse_kib_line(mem, "MemAvailable");
    out->ram_free_bytes = parse_kib_line(mem, "MemFree");
    out->ram_cached_bytes = parse_kib_line(mem, "Cached") + parse_kib_line(mem, "SReclaimable");
    out->ram_buffers_bytes = parse_kib_line(mem, "Buffers");
    out->swap_total_bytes = parse_kib_line(mem, "SwapTotal");
    out->swap_free_bytes = parse_kib_line(mem, "SwapFree");
    if (out->ram_total_bytes == 0) return ATENA_ERR_IO;
    if (out->ram_available_bytes == 0) out->ram_available_bytes = out->ram_free_bytes + out->ram_cached_bytes + out->ram_buffers_bytes;

    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    out->logical_cpus = cpus > 0 ? static_cast<uint32_t>(cpus) : 1U;
    double loads[3] = {0.0, 0.0, 0.0};
    if (getloadavg(loads, 3) >= 1) {
        out->load_1m = loads[0];
        out->load_5m = loads[1];
        out->load_15m = loads[2];
    }
    struct utsname u{};
    if (uname(&u) == 0) {
        std::snprintf(out->architecture, sizeof(out->architecture), "%.31s", u.machine);
        std::snprintf(out->platform, sizeof(out->platform), "%.31s", u.sysname);
    } else {
        std::snprintf(out->architecture, sizeof(out->architecture), "%s", "unknown");
        std::snprintf(out->platform, sizeof(out->platform), "%s", "unix");
    }
#endif

    out->memory_pressure = classify_pressure(out->ram_available_bytes,
                                             out->ram_total_bytes,
                                             out->swap_total_bytes,
                                             out->swap_free_bytes);
    return ATENA_OK;
}

extern "C" AtenaStatus atena_runtime_plan(const AtenaResourceSnapshot *s, AtenaRuntimePlan *out) {
    if (!s || !out) return ATENA_ERR_INVALID_ARGUMENT;
    std::memset(out, 0, sizeof(*out));
    const uint64_t avail_mib = s->ram_available_bytes / MiB;
    const uint64_t total_mib = s->ram_total_bytes / MiB;
    const uint32_t reserve = reserve_host_threads(s->logical_cpus);
    const uint32_t host_threads = s->logical_cpus > reserve
        ? s->logical_cpus - reserve : 1U;
    const double load_ratio = s->logical_cpus
        ? s->load_1m / static_cast<double>(s->logical_cpus) : 0.0;

    if (avail_mib < 700 || total_mib < 2800 ||
        s->memory_pressure == ATENA_MEMORY_CRITICAL) {
        std::snprintf(out->profile, sizeof(out->profile), "%s", "emergency");
        out->recommended_threads = clamp_threads(host_threads, 2);
        out->recommended_context_tokens = 1024;
        out->recommended_max_output_tokens = 256;
        out->recommended_batch_tokens = 64;
        out->recommended_keep_alive_seconds = 0;
        out->rag_level = 0;
        out->keep_model_resident = 0;
        out->allow_python_worker = 0;
    } else if (avail_mib < 1600 || total_mib < 4500 ||
               s->memory_pressure == ATENA_MEMORY_TIGHT) {
        std::snprintf(out->profile, sizeof(out->profile), "%s", "constrained");
        out->recommended_threads = clamp_threads(host_threads, 4);
        out->recommended_context_tokens = 2048;
        out->recommended_max_output_tokens = 512;
        out->recommended_batch_tokens = 128;
        out->recommended_keep_alive_seconds = 60;
        out->rag_level = 1;
        out->keep_model_resident = avail_mib >= 1100 ? 1 : 0;
        out->allow_python_worker = 0;
    } else if (avail_mib < 4800 || total_mib < 10000 ||
               s->memory_pressure == ATENA_MEMORY_MODERATE) {
        std::snprintf(out->profile, sizeof(out->profile), "%s", "balanced");
        out->recommended_threads = clamp_threads(host_threads, 8);
        out->recommended_context_tokens = 4096;
        out->recommended_max_output_tokens = 1024;
        out->recommended_batch_tokens = 256;
        out->recommended_keep_alive_seconds = 180;
        out->rag_level = 2;
        out->keep_model_resident = avail_mib >= 2200 ? 1 : 0;
        out->allow_python_worker = (avail_mib >= 2600 && load_ratio < 0.70) ? 1 : 0;
    } else {
        std::snprintf(out->profile, sizeof(out->profile), "%s", "performance");
        out->recommended_threads = clamp_threads(host_threads, 12);
        out->recommended_context_tokens = 8192;
        out->recommended_max_output_tokens = 2048;
        out->recommended_batch_tokens = 512;
        out->recommended_keep_alive_seconds = 300;
        out->rag_level = 3;
        out->keep_model_resident = 1;
        out->allow_python_worker = load_ratio < 0.80 ? 1 : 0;
    }

    out->recommended_threads = apply_load_headroom(out->recommended_threads, s);
    return ATENA_OK;
}
