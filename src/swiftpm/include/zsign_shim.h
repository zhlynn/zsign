#ifndef ZSIGN_SHIM_H
#define ZSIGN_SHIM_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *profile_path;
    const char *entitlements_path;
} zsign_asset_t;

// Every call is serialized on a process-wide lock: zsign keeps global state.
// On failure *error receives the captured zsign log; release it with zsign_free.

bool zsign_extract(const char *ipa_path, const char *folder, char **error);

// The first asset signs the main app and anything without a matching profile.
// Other assets are matched to nested bundles by application identifier.
bool zsign_sign_folder(const char *folder,
                       const char *private_key_path,
                       const char *certificate_path,
                       const char *password,
                       const zsign_asset_t *assets,
                       size_t asset_count,
                       char **error);

bool zsign_archive(const char *folder, const char *ipa_path, int zip_level, char **error);

void zsign_free(char *pointer);

#ifdef __cplusplus
}
#endif

#endif
