/* ncurses_cfg.h -- pre-generated config for phosphor vendored ncurses.
 * Covers macOS (arm64/x86_64) and Linux (glibc/musl).
 */
#ifndef NCURSES_CFG_H
#define NCURSES_CFG_H

#define PACKAGE "ncurses"
#define NCURSES_VERSION "6.5"
#define NCURSES_PATCHDATE 20240427

/* system headers */
#define HAVE_UNISTD_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_FCNTL_H 1
#define HAVE_ERRNO_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_LOCALE_H 1
#define HAVE_DIRENT_H 1
#define HAVE_REGEX_H 1

/* terminal I/O */
#define HAVE_TERMIOS_H 1
#define HAVE_TERMIO_H 0
#define HAVE_SGTTY_H 0
#define HAVE_SYS_IOCTL_H 1
#define HAVE_TCGETATTR 1
#define HAVE_ISSETUGID 1

/* POSIX functions */
#define HAVE_GETCWD 1
#define HAVE_GETENV 1
#define HAVE_PUTENV 1
#define HAVE_SETENV 1
#define HAVE_MKSTEMP 1
#define HAVE_LINK 1
#define HAVE_SYMLINK 1
#define HAVE_REALPATH 1
#define HAVE_REMOVE 1
#define HAVE_UNLINK 1
#define HAVE_POLL 1
#define HAVE_SELECT 1
#define HAVE_NANOSLEEP 1
#define HAVE_USLEEP 1
#define HAVE_SIGACTION 1
#define HAVE_SIGVEC 0
#define HAVE_GETTIMEOFDAY 1
#define HAVE_VSNPRINTF 1
#define HAVE_SNPRINTF 1
#define HAVE_STRDUP 1
#define HAVE_STRSTR 1
#define HAVE_TSEARCH 1
#define HAVE_SIZECHANGE 1
#define HAVE_WORKING_POLL 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMCCPY 1

/* ncurses features */
#define NCURSES_EXT_FUNCS 1
#define NCURSES_EXT_COLORS 0
#define NCURSES_NO_PADDING 0
#define NCURSES_PATHSEP ':'
#define NCURSES_OSPEED_COMPAT 1
#define USE_SIGWINCH 1
#define USE_ASSUMED_COLOR 1
#define USE_HARD_TABS 1
#define USE_HASHMAP 1
#define USE_COLORFGBG 1
#define USE_HOME_TERMINFO 1
#define USE_DATABASE 1
#define USE_TERMCAP 1
#define TERMPATH "/etc/termcap:/usr/share/misc/termcap"
#define HAVE_BIG_CORE 1
#define PURE_TERMINFO 0
#define USE_ROOT_ENVIRON 1
#define HAVE_REMOVE 1
#define HAVE_UNLINK 1

/* widec off for now */
#define NCURSES_WIDECHAR 0

/* mouse */
#define USE_SYSMOUSE 0

/* types */
#define TYPEOF_CHTYPE long
#define NCURSES_MBSTATE_T 0
#define NCURSES_CH_T chtype
#define NCURSES_OPAQUE 0
#define NCURSES_OPAQUE_FORM 0
#define NCURSES_OPAQUE_MENU 0
#define NCURSES_OPAQUE_PANEL 0
#define NCURSES_SIZE_T short
#define NCURSES_TPARM_VARARGS 1
#define NCURSES_INTEROP_FUNCS 1
#define NCURSES_WATTR_MACROS 1

/* sp-funcs -- thread-safe API */
#define USE_SP_FUNCS 0
#define USE_PTHREADS 0
#define USE_REENTRANT 0
#define USE_SP_WINDOWLIST 0

/* trace */
#define HAVE_TRACE 0

/* file paths */
#define TERMINFO_DIRS "/usr/share/terminfo"

#ifdef __APPLE__
#  define TERMINFO "/usr/share/terminfo"
#else
#  define TERMINFO "/usr/share/terminfo"
#endif

#define NCURSES_CONST const
#define NCURSES_INLINE inline
#define NCURSES_EXPORT(type) type
#define NCURSES_EXPORT_VAR(type) type
#define NCURSES_API
#define NCURSES_IMPEXP
#define NCURSES_SP_NAME(name) name##_sp

/* GCC attributes */
#if defined(__GNUC__) || defined(__clang__)
#  define GCC_UNUSED __attribute__((unused))
#  define GCC_NORETURN __attribute__((noreturn))
#  define GCC_PRINTFLIKE(a,b) __attribute__((format(printf,a,b)))
#else
#  define GCC_UNUSED
#  define GCC_NORETURN
#  define GCC_PRINTFLIKE(a,b)
#endif

#endif /* NCURSES_CFG_H */
