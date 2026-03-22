#ifndef PHOSPHOR_PLATFORM_H
#define PHOSPHOR_PLATFORM_H

#include "phosphor/types.h"

#include <sys/types.h>

/* --- platform detection ------------------------------------------------- */

#if defined(__APPLE__) && defined(__MACH__)
    #define PH_PLATFORM_MACOS  1
    #define PH_PLATFORM_POSIX  1
#elif defined(__linux__)
    #define PH_PLATFORM_LINUX  1
    #define PH_PLATFORM_POSIX  1
#else
    #error "unsupported platform -- phosphor v1 targets macOS and Linux only"
#endif

/* --- filesystem --------------------------------------------------------- */

/*
 * ph_fs_stat_t -- minimal stat result.
 *
 * ownership: value type, no heap pointers.
 */
typedef struct {
    off_t  size;
    mode_t mode;
    bool   is_dir;
    bool   is_file;
    bool   is_link;
    bool   exists;
} ph_fs_stat_t;

ph_result_t ph_fs_stat(const char *path, ph_fs_stat_t *out);
ph_result_t ph_fs_read_file(const char *path, uint8_t **out_data,
                            size_t *out_len);
ph_result_t ph_fs_write_file(const char *path, const uint8_t *data,
                             size_t len);
ph_result_t ph_fs_rename(const char *old_path, const char *new_path);
ph_result_t ph_fs_fsync_path(const char *path);
ph_result_t ph_fs_chmod(const char *path, mode_t mode);
ph_result_t ph_fs_mkdir_p(const char *path, mode_t mode);
bool        ph_fs_fnmatch(const char *pattern, const char *string);

/* --- process ------------------------------------------------------------ */

/*
 * ph_proc_result_t -- child process completion result.
 *
 * ownership: value type, no heap pointers.
 */
typedef struct {
    int  exit_code;
    bool signaled;
    int  signal_num;
} ph_proc_result_t;

ph_result_t ph_proc_spawn(const char *const argv[],
                           const char *const envp[],
                           const char *cwd,
                           ph_proc_result_t *out);

/* --- timing ------------------------------------------------------------- */

/* monotonic nanosecond timestamp */
uint64_t ph_clock_monotonic_ns(void);

/* elapsed milliseconds between two monotonic timestamps */
double ph_clock_elapsed_ms(uint64_t start_ns, uint64_t end_ns);

#endif /* PHOSPHOR_PLATFORM_H */
