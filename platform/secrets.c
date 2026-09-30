#include "atena/secret.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#include <wchar.h>

#define ATENA_CRED_PREFIX L"Atena/provider/"

static int wide_from_utf8(const char *src, wchar_t **out) {
    if (!src || !out) return 0;
    *out = NULL;
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, NULL, 0);
    if (count <= 0) return 0;
    wchar_t *buffer = (wchar_t*)calloc((size_t)count, sizeof(wchar_t));
    if (!buffer) return 0;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, buffer, count) <= 0) {
        free(buffer);
        return 0;
    }
    *out = buffer;
    return 1;
}

static int utf8_from_wide_bytes(const wchar_t *src, size_t wchar_count, char **out) {
    if (!src || !out) return 0;
    *out = NULL;
    if (wchar_count > (size_t)INT_MAX) return 0;
    int bytes = WideCharToMultiByte(CP_UTF8, 0, src, (int)wchar_count, NULL, 0, NULL, NULL);
    if (bytes < 0) return 0;
    char *buffer = (char*)calloc((size_t)bytes + 1U, 1U);
    if (!buffer) return 0;
    if (bytes > 0 && WideCharToMultiByte(CP_UTF8, 0, src, (int)wchar_count, buffer, bytes, NULL, NULL) <= 0) {
        free(buffer);
        return 0;
    }
    buffer[bytes] = '\0';
    *out = buffer;
    return 1;
}

static wchar_t *credential_target(const char *provider_id) {
    wchar_t *provider = NULL;
    if (!wide_from_utf8(provider_id, &provider)) return NULL;
    const size_t prefix_len = wcslen(ATENA_CRED_PREFIX);
    const size_t provider_len = wcslen(provider);
    wchar_t *target = (wchar_t*)calloc(prefix_len + provider_len + 1U, sizeof(wchar_t));
    if (target) {
        memcpy(target, ATENA_CRED_PREFIX, prefix_len * sizeof(wchar_t));
        memcpy(target + prefix_len, provider, (provider_len + 1U) * sizeof(wchar_t));
    }
    SecureZeroMemory(provider, (provider_len + 1U) * sizeof(wchar_t));
    free(provider);
    return target;
}

static AtenaStatus map_credential_error(DWORD error) {
    if (error == ERROR_NOT_FOUND) return ATENA_ERR_NOT_FOUND;
    if (error == ERROR_ACCESS_DENIED) return ATENA_ERR_PERMISSION;
    if (error == ERROR_INVALID_PARAMETER || error == ERROR_BAD_ARGUMENTS) return ATENA_ERR_INVALID_ARGUMENT;
    return ATENA_ERR_IO;
}

AtenaStatus atena_secret_store(const char *provider_id, const char *secret) {
    if (!provider_id || !*provider_id || !secret || !*secret) return ATENA_ERR_INVALID_ARGUMENT;
    wchar_t *target = credential_target(provider_id);
    wchar_t *wide_secret = NULL;
    if (!target || !wide_from_utf8(secret, &wide_secret)) {
        free(target);
        return ATENA_ERR_NO_MEMORY;
    }

    const size_t wide_len = wcslen(wide_secret);
    if (wide_len > (CRED_MAX_CREDENTIAL_BLOB_SIZE / sizeof(wchar_t))) {
        SecureZeroMemory(wide_secret, (wide_len + 1U) * sizeof(wchar_t));
        free(wide_secret);
        free(target);
        return ATENA_ERR_INVALID_ARGUMENT;
    }

    CREDENTIALW credential;
    ZeroMemory(&credential, sizeof(credential));
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = target;
    credential.CredentialBlobSize = (DWORD)(wide_len * sizeof(wchar_t));
    credential.CredentialBlob = (LPBYTE)wide_secret;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = L"Atena";

    BOOL ok = CredWriteW(&credential, 0);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    SecureZeroMemory(wide_secret, (wide_len + 1U) * sizeof(wchar_t));
    free(wide_secret);
    free(target);
    return ok ? ATENA_OK : map_credential_error(error);
}

AtenaStatus atena_secret_lookup(const char *provider_id, char **out_secret) {
    if (!provider_id || !*provider_id || !out_secret) return ATENA_ERR_INVALID_ARGUMENT;
    *out_secret = NULL;
    wchar_t *target = credential_target(provider_id);
    if (!target) return ATENA_ERR_NO_MEMORY;

    PCREDENTIALW credential = NULL;
    BOOL ok = CredReadW(target, CRED_TYPE_GENERIC, 0, &credential);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    free(target);
    if (!ok) return map_credential_error(error);
    if (!credential || !credential->CredentialBlob || credential->CredentialBlobSize == 0 ||
        (credential->CredentialBlobSize % sizeof(wchar_t)) != 0) {
        if (credential) CredFree(credential);
        return ATENA_ERR_NOT_FOUND;
    }

    const size_t wchar_count = credential->CredentialBlobSize / sizeof(wchar_t);
    char *value = NULL;
    int converted = utf8_from_wide_bytes((const wchar_t*)credential->CredentialBlob, wchar_count, &value);
    CredFree(credential);
    if (!converted || !value) return ATENA_ERR_IO;
    *out_secret = value;
    return ATENA_OK;
}

AtenaStatus atena_secret_delete(const char *provider_id) {
    if (!provider_id || !*provider_id) return ATENA_ERR_INVALID_ARGUMENT;
    wchar_t *target = credential_target(provider_id);
    if (!target) return ATENA_ERR_NO_MEMORY;
    BOOL ok = CredDeleteW(target, CRED_TYPE_GENERIC, 0);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    free(target);
    if (ok || error == ERROR_NOT_FOUND) return ATENA_OK;
    return map_credential_error(error);
}

int atena_secret_persistence_available(void) { return 1; }

#elif defined(ATENA_HAVE_LIBSECRET)
#include <libsecret/secret.h>
#include <gio/gio.h>

static const SecretSchema ATENA_SECRET_SCHEMA = {
    .name = "org.athenas.atena.provider",
    .flags = SECRET_SCHEMA_NONE,
    .attributes = {
        { "provider", SECRET_SCHEMA_ATTRIBUTE_STRING },
        { NULL, 0 }
    }
};

static AtenaStatus map_error(GError *error) {
    if (!error) return ATENA_ERR_IO;
    if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_PERMISSION_DENIED)
        return ATENA_ERR_PERMISSION;
    return ATENA_ERR_IO;
}

AtenaStatus atena_secret_store(const char *provider_id, const char *secret) {
    if (!provider_id || !*provider_id || !secret || !*secret) return ATENA_ERR_INVALID_ARGUMENT;
    GError *error = NULL;
    gboolean ok = secret_password_store_sync(&ATENA_SECRET_SCHEMA,
                                             SECRET_COLLECTION_DEFAULT,
                                             "Atena IA provider credential",
                                             secret,
                                             NULL,
                                             &error,
                                             "provider", provider_id,
                                             NULL);
    if (!ok) {
        AtenaStatus status = map_error(error);
        if (error) g_error_free(error);
        return status;
    }
    return ATENA_OK;
}

AtenaStatus atena_secret_lookup(const char *provider_id, char **out_secret) {
    if (!provider_id || !*provider_id || !out_secret) return ATENA_ERR_INVALID_ARGUMENT;
    *out_secret = NULL;
    GError *error = NULL;
    gchar *value = secret_password_lookup_sync(&ATENA_SECRET_SCHEMA,
                                               NULL,
                                               &error,
                                               "provider", provider_id,
                                               NULL);
    if (!value) {
        if (error) {
            AtenaStatus status = map_error(error);
            g_error_free(error);
            return status;
        }
        return ATENA_ERR_NOT_FOUND;
    }
    size_t n = strlen(value) + 1U;
    *out_secret = (char*)malloc(n);
    if (*out_secret) memcpy(*out_secret, value, n);
    secret_password_free(value);
    return *out_secret ? ATENA_OK : ATENA_ERR_NO_MEMORY;
}

AtenaStatus atena_secret_delete(const char *provider_id) {
    if (!provider_id || !*provider_id) return ATENA_ERR_INVALID_ARGUMENT;
    GError *error = NULL;
    gboolean ok = secret_password_clear_sync(&ATENA_SECRET_SCHEMA,
                                             NULL,
                                             &error,
                                             "provider", provider_id,
                                             NULL);
    if (!ok && error) {
        AtenaStatus status = map_error(error);
        g_error_free(error);
        return status;
    }
    return ATENA_OK;
}

int atena_secret_persistence_available(void) { return 1; }

#else

AtenaStatus atena_secret_store(const char *provider_id, const char *secret) {
    (void)provider_id; (void)secret;
    return ATENA_ERR_UNSUPPORTED;
}
AtenaStatus atena_secret_lookup(const char *provider_id, char **out_secret) {
    (void)provider_id;
    if (out_secret) *out_secret = NULL;
    return ATENA_ERR_UNSUPPORTED;
}
AtenaStatus atena_secret_delete(const char *provider_id) {
    (void)provider_id;
    return ATENA_ERR_UNSUPPORTED;
}
int atena_secret_persistence_available(void) { return 0; }

#endif

void atena_secret_free(char *secret) {
    if (!secret) return;
    size_t n = strlen(secret);
#ifdef _WIN32
    SecureZeroMemory(secret, n);
#else
    volatile char *p = secret;
    while (n--) *p++ = 0;
#endif
    free(secret);
}
