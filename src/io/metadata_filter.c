#include "phosphor/fs.h"
#include "phosphor/platform.h"

#include <string.h>

static const char *deny_exact[] = {
    ".DS_Store",
    "Thumbs.db",
    ".Spotlight-V100",
    ".Trashes",
    "desktop.ini",
    ".fseventsd",
    "__MACOSX",
};

static const char *deny_patterns[] = {
    "._*",
    ".phosphor-staging-*",
};

bool ph_metadata_is_denied(const char *basename) {
    if (!basename) return false;

    /* exact match */
    for (size_t i = 0; i < sizeof(deny_exact) / sizeof(deny_exact[0]); i++) {
        if (strcmp(basename, deny_exact[i]) == 0) return true;
    }

    /* glob patterns */
    for (size_t i = 0; i < sizeof(deny_patterns) / sizeof(deny_patterns[0]); i++) {
        if (ph_fs_fnmatch(deny_patterns[i], basename)) return true;
    }

    return false;
}
