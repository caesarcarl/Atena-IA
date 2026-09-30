#include "atena/platform.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>

static int wide_from_utf8(const char *src, wchar_t *out, size_t cap) {
    if (!src || !out || cap == 0 || cap > (size_t)INT_MAX) return 0;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, out, (int)cap);
    return n > 0 && (size_t)n <= cap;
}

static AtenaStatus validate_core_paths(const AtenaPaths *paths) {
    if (!paths || !paths->core_executable[0] || !paths->identity_dir[0]) return ATENA_ERR_INVALID_ARGUMENT;
    wchar_t core[ATENA_PATH_MAX], identity[ATENA_PATH_MAX];
    if (!wide_from_utf8(paths->core_executable, core, ATENA_PATH_MAX) ||
        !wide_from_utf8(paths->identity_dir, identity, ATENA_PATH_MAX)) return ATENA_ERR_PATH;

    DWORD attrs = GetFileAttributesW(core);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) return ATENA_ERR_PATH;
    attrs = GetFileAttributesW(identity);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) return ATENA_ERR_PATH;

    static const wchar_t *required[] = {L"persona.json", L"pedagogy.json", L"regional.json"};
    for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
        wchar_t file[ATENA_PATH_MAX];
        if (swprintf_s(file, ATENA_PATH_MAX, L"%ls\\%ls", identity, required[i]) < 0) return ATENA_ERR_PATH;
        attrs = GetFileAttributesW(file);
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) return ATENA_ERR_PATH;
    }
    return ATENA_OK;
}

static HANDLE open_log(const AtenaPaths *paths, wchar_t out_path[ATENA_PATH_MAX]) {
    wchar_t data_dir[ATENA_PATH_MAX];
    if (!wide_from_utf8(paths->data_dir, data_dir, ATENA_PATH_MAX)) return INVALID_HANDLE_VALUE;
    if (swprintf_s(out_path, ATENA_PATH_MAX, L"%ls\\core.log", data_dir) < 0) return INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE log = CreateFileW(out_path, FILE_APPEND_DATA | SYNCHRONIZE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log == INVALID_HANDLE_VALUE) return log;
    SetFilePointer(log, 0, NULL, FILE_END);

    char marker[(ATENA_PATH_MAX * 2) + 256];
    int n = snprintf(marker, sizeof(marker),
                     "\r\n=== Atena Core spawn ===\r\n"
                     "executable=%s\r\nendpoint=%s\r\ndatabase=%s\r\nidentity=%s\r\n",
                     paths->core_executable, paths->endpoint, paths->database, paths->identity_dir);
    if (n > 0) {
        DWORD written = 0;
        (void)WriteFile(log, marker, (DWORD)((size_t)n < sizeof(marker) ? (size_t)n : sizeof(marker) - 1U), &written, NULL);
    }
    return log;
}

AtenaStatus atena_process_start_core(const AtenaPaths *paths) {
    AtenaStatus check = validate_core_paths(paths);
    if (check != ATENA_OK) return check;

    wchar_t exe[ATENA_PATH_MAX], endpoint[ATENA_PATH_MAX], database[ATENA_PATH_MAX], identity[ATENA_PATH_MAX];
    if (!wide_from_utf8(paths->core_executable, exe, ATENA_PATH_MAX) ||
        !wide_from_utf8(paths->endpoint, endpoint, ATENA_PATH_MAX) ||
        !wide_from_utf8(paths->database, database, ATENA_PATH_MAX) ||
        !wide_from_utf8(paths->identity_dir, identity, ATENA_PATH_MAX)) return ATENA_ERR_PATH;

    wchar_t cmd[(ATENA_PATH_MAX * 4) + 128];
    if (swprintf_s(cmd, sizeof(cmd) / sizeof(cmd[0]),
                   L"\"%ls\" --socket \"%ls\" --db \"%ls\" --identity \"%ls\"",
                   exe, endpoint, database, identity) < 0) return ATENA_ERR_PATH;

    wchar_t log_path[ATENA_PATH_MAX];
    HANDLE log = open_log(paths, log_path);
    if (log == INVALID_HANDLE_VALUE) return ATENA_ERR_IO;

    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE null_in = CreateFileW(L"NUL", GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (null_in == INVALID_HANDLE_VALUE) {
        CloseHandle(log);
        return ATENA_ERR_IO;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = null_in;
    si.hStdOutput = log;
    si.hStdError = log;
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessW(exe, cmd, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
                             NULL, NULL, &si, &pi);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(null_in);
    CloseHandle(log);

    if (!ok) {
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return ATENA_ERR_PATH;
        if (error == ERROR_ACCESS_DENIED) return ATENA_ERR_PERMISSION;
        return ATENA_ERR_IO;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return ATENA_OK;
}

#else
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
extern char **environ;

static AtenaStatus validate_core_paths(const AtenaPaths *paths) {
    if (access(paths->core_executable, F_OK) != 0) return ATENA_ERR_PATH;
    if (access(paths->core_executable, X_OK) != 0)
        return (errno == EACCES || errno == EPERM) ? ATENA_ERR_PERMISSION : ATENA_ERR_PATH;

    struct stat st;
    if (stat(paths->identity_dir, &st) != 0 || !S_ISDIR(st.st_mode)) return ATENA_ERR_PATH;
    static const char *required[] = {"persona.json", "pedagogy.json", "regional.json"};
    for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
        char file[ATENA_PATH_MAX];
        int n = snprintf(file, sizeof(file), "%s/%s", paths->identity_dir, required[i]);
        if (n < 0 || (size_t)n >= sizeof(file) || access(file, R_OK) != 0) return ATENA_ERR_PATH;
    }
    return ATENA_OK;
}

static void append_spawn_marker(const AtenaPaths *paths, const char *log_path) {
    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return;
    dprintf(fd,
            "\n=== Atena Core spawn ===\n"
            "executable=%s\nendpoint=%s\ndatabase=%s\nidentity=%s\n",
            paths->core_executable, paths->endpoint, paths->database, paths->identity_dir);
    close(fd);
}

AtenaStatus atena_process_start_core(const AtenaPaths *paths) {
    if (!paths || !paths->core_executable[0]) return ATENA_ERR_INVALID_ARGUMENT;
    AtenaStatus check = validate_core_paths(paths);
    if (check != ATENA_OK) return check;

    char log_path[ATENA_PATH_MAX];
    int n = snprintf(log_path, sizeof(log_path), "%s/core.log", paths->data_dir);
    if (n < 0 || (size_t)n >= sizeof(log_path)) return ATENA_ERR_PATH;
    append_spawn_marker(paths, log_path);

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) return ATENA_ERR_IO;
    if (posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) != 0 ||
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log_path,
                                         O_WRONLY | O_CREAT | O_APPEND, 0600) != 0 ||
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, log_path,
                                         O_WRONLY | O_CREAT | O_APPEND, 0600) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        return ATENA_ERR_IO;
    }

    char *argv[] = {(char*)paths->core_executable,
                    "--socket", (char*)paths->endpoint,
                    "--db", (char*)paths->database,
                    "--identity", (char*)paths->identity_dir,
                    NULL};
    pid_t pid = 0;
    int rc = posix_spawn(&pid, paths->core_executable, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc == 0) return ATENA_OK;
    if (rc == ENOENT || rc == ENOTDIR) return ATENA_ERR_PATH;
    if (rc == EACCES || rc == EPERM) return ATENA_ERR_PERMISSION;
    return ATENA_ERR_IO;
}
#endif
