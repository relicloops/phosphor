#include "phosphor/alloc.h"
#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/error.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/manifest.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"

#include <limits.h>
#include <string.h>
#include <unistd.h>

/* ---- main rm pipeline ---- */

int ph_cmd_rm(const ph_cli_config_t *config,
              const ph_parsed_args_t *args) {
    (void)config;

    /* step 1: extract flags */
    const char *specific_val = ph_args_get_flag(args, "specific");
    const char *project_val  = ph_args_get_flag(args, "project");
    bool force               = ph_args_has_flag(args, "force");
    bool verbose             = ph_args_has_flag(args, "verbose");

    if (verbose)
        ph_log_set_level(PH_LOG_DEBUG);

    if (!specific_val) {
        ph_log_error("rm: --specific is required");
        return PH_ERR_USAGE;
    }

    /* step 2: resolve project root */
    char *project_root_abs = NULL;

    if (project_val) {
        if (ph_path_is_absolute(project_val)) {
            project_root_abs = ph_path_normalize(project_val);
        } else {
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd))) {
                ph_log_error("rm: failed to get current directory");
                return PH_ERR_INTERNAL;
            }
            char *joined = ph_path_join(cwd, project_val);
            if (joined) {
                project_root_abs = ph_path_normalize(joined);
                ph_free(joined);
            }
        }
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            ph_log_error("rm: failed to get current directory");
            return PH_ERR_INTERNAL;
        }
        project_root_abs = ph_path_normalize(cwd);
    }

    if (!project_root_abs) {
        ph_log_error("rm: failed to resolve project root");
        return PH_ERR_INTERNAL;
    }

    /* step 3: manifest guard -- require --force when no manifest found.
     * audit fix (finding 13): only probe template.phosphor.toml and
     * manifest.toml via ph_manifest_find.  phosphor.toml is project
     * config, not a template manifest. */
    if (!force) {
        char *manifest_path = ph_manifest_find(project_root_abs);
        bool has_manifest = (manifest_path != NULL);
        ph_free(manifest_path);

        if (!has_manifest) {
            ph_log_error("rm: no template manifest found in project root; "
                         "use --force to remove anyway");
            ph_free(project_root_abs);
            return PH_ERR_USAGE;
        }
    }

    /* step 4: reject absolute paths and traversal in --specific */
    if (ph_path_is_absolute(specific_val)) {
        ph_log_error("rm: --specific must be a relative path");
        ph_free(project_root_abs);
        return PH_ERR_USAGE;
    }

    if (ph_path_has_traversal(specific_val)) {
        ph_log_error("rm: --specific must not contain '..' traversal");
        ph_free(project_root_abs);
        return PH_ERR_USAGE;
    }

    /* step 5: resolve target to absolute */
    char *target_abs = ph_path_join(project_root_abs, specific_val);
    if (!target_abs) {
        ph_free(project_root_abs);
        return PH_ERR_INTERNAL;
    }

    /* audit fix: canonical containment check. The previous textual-prefix
     * guard could be defeated by a symlinked intermediate directory inside
     * the project that points outside the tree: the joined string still
     * begins with project_root_abs but ph_fs_rmtree() would follow the
     * symlink target. Resolve both sides with realpath(3) via
     * ph_path_resolve() and then assert containment with ph_path_is_under().
     *
     * Two-step resolution: the target itself may not exist yet (the textual
     * checks above don't care), so we canonicalize the parent of target_abs
     * which must exist because specific_val was joined under
     * project_root_abs. For an existing target, ph_path_resolve works
     * directly. */
    char *canonical_root = ph_path_resolve(project_root_abs);
    if (!canonical_root) {
        ph_log_error("rm: cannot canonicalize project root: %s",
                     project_root_abs);
        ph_free(target_abs);
        ph_free(project_root_abs);
        return PH_ERR_INTERNAL;
    }

    char *canonical_target = ph_path_resolve(target_abs);
    if (!canonical_target) {
        /* target may not exist yet; resolve parent and rejoin basename */
        char *parent = ph_path_dirname(target_abs);
        const char *slash = strrchr(target_abs, '/');
        const char *base = slash ? slash + 1 : target_abs;
        if (parent) {
            char *canonical_parent = ph_path_resolve(parent);
            if (canonical_parent) {
                canonical_target = ph_path_join(canonical_parent, base);
                ph_free(canonical_parent);
            }
            ph_free(parent);
        }
    }

    if (!canonical_target) {
        ph_log_error("rm: cannot canonicalize target: %s", target_abs);
        ph_free(canonical_root);
        ph_free(target_abs);
        ph_free(project_root_abs);
        return PH_ERR_INTERNAL;
    }

    if (!ph_path_is_under(canonical_target, canonical_root)) {
        ph_log_error("rm: target escapes project root: %s", target_abs);
        ph_free(canonical_target);
        ph_free(canonical_root);
        ph_free(target_abs);
        ph_free(project_root_abs);
        return PH_ERR_VALIDATE;
    }

    ph_free(canonical_target);
    ph_free(canonical_root);

    /* step 6: check existence */
    ph_fs_stat_t st;
    if (ph_fs_stat(target_abs, &st) != PH_OK || !st.exists) {
        ph_log_error("rm: path does not exist: %s", specific_val);
        ph_free(target_abs);
        ph_free(project_root_abs);
        return PH_ERR_FS;
    }

    /* step 7: remove */
    if (verbose)
        ph_log_debug("rm: removing %s", target_abs);

    ph_error_t *err = NULL;
    if (ph_fs_rmtree(target_abs, &err) != PH_OK) {
        ph_log_error("rm: failed to remove %s: %s",
                      specific_val, err ? err->message : "unknown");
        ph_error_destroy(err);
        ph_free(target_abs);
        ph_free(project_root_abs);
        return PH_ERR_FS;
    }

    ph_log_info("rm: removed %s", specific_val);

    ph_free(target_abs);
    ph_free(project_root_abs);
    return 0;
}
