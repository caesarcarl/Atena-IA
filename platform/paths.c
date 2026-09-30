#include "atena/path.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifndef ATENA_DATA_DIR
#define ATENA_DATA_DIR "/usr/share/atena"
#endif
#ifndef ATENA_LIBEXEC_DIR
#define ATENA_LIBEXEC_DIR "/usr/libexec/atena"
#endif

static int set_path(char *out, size_t cap, const char *a, const char *b) {
    if (!out || !cap || !a || !b) return 0;
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    const size_t alen = strlen(a);
    const int has_sep = alen > 0 && (a[alen-1] == '/' || a[alen-1] == '\\');
    int n = snprintf(out, cap, "%s%s%s", a, has_sep ? "" : (sep == '\\' ? "\\" : "/"), b);
    return n >= 0 && (size_t)n < cap;
}

#ifdef _WIN32
static int utf8_from_wide(const wchar_t *src, char *out, size_t cap) {
    if (!src || !out || cap == 0) return 0;
    int n = WideCharToMultiByte(CP_UTF8, 0, src, -1, out, (int)cap, NULL, NULL);
    return n > 0 && (size_t)n <= cap;
}

static int wide_from_utf8(const char *src, wchar_t *out, size_t cap) {
    if (!src || !out || cap == 0) return 0;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, out, (int)cap);
    return n > 0 && (size_t)n <= cap;
}

static AtenaStatus known_folder(REFKNOWNFOLDERID id, char *out, size_t cap) {
    PWSTR w = NULL;
    HRESULT hr = SHGetKnownFolderPath(id, KF_FLAG_CREATE, NULL, &w);
    if (FAILED(hr) || !w) return ATENA_ERR_PATH;
    int ok = utf8_from_wide(w, out, cap);
    CoTaskMemFree(w);
    return ok ? ATENA_OK : ATENA_ERR_PATH;
}

static AtenaStatus mkdir_private(const char *path) {
    wchar_t w[ATENA_PATH_MAX];
    if (!wide_from_utf8(path, w, ATENA_PATH_MAX)) return ATENA_ERR_PATH;
    size_t len = wcslen(w);
    if (len < 3) return ATENA_ERR_PATH;
    for (size_t i = 3; i <= len; ++i) {
        if (w[i] != L'\\' && w[i] != L'/' && w[i] != L'\0') continue;
        wchar_t saved = w[i];
        w[i] = L'\0';
        if (!CreateDirectoryW(w, NULL)) {
            DWORD e = GetLastError();
            if (e != ERROR_ALREADY_EXISTS) {
                w[i] = saved;
                return e == ERROR_ACCESS_DENIED ? ATENA_ERR_PERMISSION : ATENA_ERR_PATH;
            }
        }
        w[i] = saved;
    }
    return ATENA_OK;
}

static int executable_sibling(char *out, size_t cap, const char *name) {
    wchar_t module[ATENA_PATH_MAX];
    DWORD n = GetModuleFileNameW(NULL, module, ATENA_PATH_MAX);
    if (!n || n >= ATENA_PATH_MAX) return 0;
    wchar_t *slash = wcsrchr(module, L'\\');
    if (!slash) return 0;
    slash[1] = L'\0';
    wchar_t wname[256];
    if (!wide_from_utf8(name, wname, 256)) return 0;
    if (wcslen(module) + wcslen(wname) + 1 >= ATENA_PATH_MAX) return 0;
    wcscat_s(module, ATENA_PATH_MAX, wname);
    if (GetFileAttributesW(module) == INVALID_FILE_ATTRIBUTES) return 0;
    return utf8_from_wide(module, out, cap);
}

static int current_user_sid(char *out, size_t cap) {
    HANDLE token = NULL;
    DWORD bytes = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &bytes);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !bytes) { CloseHandle(token); return 0; }
    TOKEN_USER *user = (TOKEN_USER*)malloc(bytes);
    if (!user) { CloseHandle(token); return 0; }
    int ok = 0;
    if (GetTokenInformation(token, TokenUser, user, bytes, &bytes)) {
        LPSTR sid = NULL;
        if (ConvertSidToStringSidA(user->User.Sid, &sid) && sid) {
            int n = snprintf(out, cap, "%s", sid);
            ok = n >= 0 && (size_t)n < cap;
            LocalFree(sid);
        }
    }
    free(user);
    CloseHandle(token);
    return ok;
}

#else

static AtenaStatus mkdir_private(const char *path) {
    char tmp[ATENA_PATH_MAX];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(tmp)) return ATENA_ERR_PATH;
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0700) != 0 && errno != EEXIST)
            return (errno == EACCES || errno == EPERM) ? ATENA_ERR_PERMISSION : ATENA_ERR_PATH;
        *p = '/';
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST)
        return (errno == EACCES || errno == EPERM) ? ATENA_ERR_PERMISSION : ATENA_ERR_PATH;
    if (chmod(tmp, 0700) != 0)
        return (errno == EACCES || errno == EPERM) ? ATENA_ERR_PERMISSION : ATENA_ERR_PATH;
    return ATENA_OK;
}

static int executable_sibling(char *out, size_t cap, const char *name) {
    char current[ATENA_PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", current, sizeof(current)-1);
    if (n <= 0 || (size_t)n >= sizeof(current)-1) return 0;
    current[n] = '\0';
    char *slash = strrchr(current, '/');
    if (!slash) return 0;
    *slash = '\0';
    if (!set_path(out, cap, current, name)) return 0;
    return access(out, X_OK) == 0;
}
#endif

AtenaStatus atena_paths_resolve(AtenaPaths *p) {
    if (!p) return ATENA_ERR_INVALID_ARGUMENT;
    memset(p, 0, sizeof(*p));

#ifdef _WIN32
    char roaming[ATENA_PATH_MAX], local[ATENA_PATH_MAX];
    if (known_folder(&FOLDERID_RoamingAppData, roaming, sizeof(roaming)) != ATENA_OK ||
        known_folder(&FOLDERID_LocalAppData, local, sizeof(local)) != ATENA_OK)
        return ATENA_ERR_PATH;

    if (!set_path(p->config_dir, sizeof(p->config_dir), roaming, "Atena") ||
        !set_path(p->data_dir, sizeof(p->data_dir), local, "Atena") ||
        !set_path(p->cache_dir, sizeof(p->cache_dir), local, "Atena\\Cache") ||
        !set_path(p->runtime_dir, sizeof(p->runtime_dir), local, "Atena\\Runtime") ||
        !set_path(p->database, sizeof(p->database), p->data_dir, "atena.db") ||
        !set_path(p->identity_user_dir, sizeof(p->identity_user_dir), p->config_dir, "identity") ||
        !set_path(p->startup_lock, sizeof(p->startup_lock), p->runtime_dir, "start.lock"))
        return ATENA_ERR_PATH;

    char sid[192] = "user";
    (void)current_user_sid(sid, sizeof(sid));
    int ep = snprintf(p->endpoint, sizeof(p->endpoint), "\\\\.\\pipe\\Atena-Core-v2-%s", sid);
    if (ep < 0 || (size_t)ep >= sizeof(p->endpoint)) return ATENA_ERR_PATH;

    const char *v = getenv("ATENA_IDENTITY_DIR");
    if (v && *v) {
        if (snprintf(p->identity_dir, sizeof(p->identity_dir), "%s", v) >= (int)sizeof(p->identity_dir))
            return ATENA_ERR_PATH;
    } else if (!executable_sibling(p->identity_dir, sizeof(p->identity_dir), "identity")) {
        /* Installer places identity beside executables. The path may not exist at configure time. */
        wchar_t module[ATENA_PATH_MAX];
        DWORD n = GetModuleFileNameW(NULL, module, ATENA_PATH_MAX);
        if (!n || n >= ATENA_PATH_MAX) return ATENA_ERR_PATH;
        wchar_t *slash = wcsrchr(module, L'\\');
        if (!slash) return ATENA_ERR_PATH;
        slash[1] = L'\0';
        wcscat_s(module, ATENA_PATH_MAX, L"identity");
        if (!utf8_from_wide(module, p->identity_dir, sizeof(p->identity_dir))) return ATENA_ERR_PATH;
    }

    v = getenv("ATENA_CORE_EXECUTABLE");
    if (v && *v) {
        if (snprintf(p->core_executable, sizeof(p->core_executable), "%s", v) >= (int)sizeof(p->core_executable))
            return ATENA_ERR_PATH;
    } else if (!executable_sibling(p->core_executable, sizeof(p->core_executable), "atena-core.exe")) {
        return ATENA_ERR_PATH;
    }
#else
    const char *home = getenv("HOME");
    if (!home || !*home) return ATENA_ERR_PATH;
    const char *v;

    v = getenv("XDG_CONFIG_HOME");
    if (!set_path(p->config_dir, sizeof(p->config_dir), (v && *v) ? v : home,
                  (v && *v) ? "atena" : ".config/atena")) return ATENA_ERR_PATH;
    v = getenv("XDG_DATA_HOME");
    if (!set_path(p->data_dir, sizeof(p->data_dir), (v && *v) ? v : home,
                  (v && *v) ? "atena" : ".local/share/atena")) return ATENA_ERR_PATH;
    v = getenv("XDG_CACHE_HOME");
    if (!set_path(p->cache_dir, sizeof(p->cache_dir), (v && *v) ? v : home,
                  (v && *v) ? "atena" : ".cache/atena")) return ATENA_ERR_PATH;

    v = getenv("XDG_RUNTIME_DIR");
    if (v && *v) {
        if (!set_path(p->runtime_dir, sizeof(p->runtime_dir), v, "atena")) return ATENA_ERR_PATH;
    } else {
        /* Private fallback still belongs to the user and never uses a global /tmp socket. */
        if (!set_path(p->runtime_dir, sizeof(p->runtime_dir), p->cache_dir, "runtime")) return ATENA_ERR_PATH;
    }

    if (!set_path(p->database, sizeof(p->database), p->data_dir, "atena.db") ||
        !set_path(p->endpoint, sizeof(p->endpoint), p->runtime_dir, "core-v2.sock") ||
        !set_path(p->identity_user_dir, sizeof(p->identity_user_dir), p->config_dir, "identity") ||
        !set_path(p->startup_lock, sizeof(p->startup_lock), p->runtime_dir, "start.lock"))
        return ATENA_ERR_PATH;

    v = getenv("ATENA_IDENTITY_DIR");
    if (v && *v) snprintf(p->identity_dir, sizeof(p->identity_dir), "%s", v);
    else if (!executable_sibling(p->identity_dir, sizeof(p->identity_dir), "identity"))
        snprintf(p->identity_dir, sizeof(p->identity_dir), "%s/identity", ATENA_DATA_DIR);

    v = getenv("ATENA_CORE_EXECUTABLE");
    if (v && *v) snprintf(p->core_executable, sizeof(p->core_executable), "%s", v);
    else if (!executable_sibling(p->core_executable, sizeof(p->core_executable), "atena-core"))
        snprintf(p->core_executable, sizeof(p->core_executable), "%s/atena-core", ATENA_LIBEXEC_DIR);
#endif
    return ATENA_OK;
}

AtenaStatus atena_paths_prepare(const AtenaPaths *p) {
    if (!p) return ATENA_ERR_INVALID_ARGUMENT;
    AtenaStatus st;
    if ((st = mkdir_private(p->config_dir)) != ATENA_OK) return st;
    if ((st = mkdir_private(p->data_dir)) != ATENA_OK) return st;
    if ((st = mkdir_private(p->cache_dir)) != ATENA_OK) return st;
    if ((st = mkdir_private(p->runtime_dir)) != ATENA_OK) return st;
    return ATENA_OK;
}
