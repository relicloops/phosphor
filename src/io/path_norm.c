#include "phosphor/path.h"
#include "phosphor/alloc.h"
#include "phosphor/error.h"

#include <stdlib.h>
#include <string.h>

char *ph_path_normalize(const char *path) {
    if (!path) return NULL;

    size_t len = strlen(path);
    if (len == 0) {
        char *out = ph_alloc(2);
        if (!out) return NULL;
        out[0] = '.';
        out[1] = '\0';
        return out;
    }

    char *buf = ph_alloc(len + 1);
    if (!buf) return NULL;

    size_t wi = 0;
    bool prev_slash = false;

    for (size_t ri = 0; ri < len; ri++) {
        char c = path[ri];

        if (c == '/') {
            /* collapse consecutive slashes */
            if (prev_slash && wi > 1) continue;
            /* skip "/./" sequences */
            if (ri + 1 < len && path[ri + 1] == '.' &&
                (ri + 2 >= len || path[ri + 2] == '/')) {
                ri++;  /* skip the dot, loop will skip the slash */
                continue;
            }
            buf[wi++] = '/';
            prev_slash = true;
        } else if (c == '.' && wi == 0 &&
                   (ri + 1 >= len || path[ri + 1] == '/')) {
            /* leading "./" -- skip */
            if (ri + 1 < len) ri++;  /* skip the slash */
            continue;
        } else {
            buf[wi++] = c;
            prev_slash = false;
        }
    }

    /* remove trailing slash unless it's the root "/" */
    if (wi > 1 && buf[wi - 1] == '/') wi--;

    if (wi == 0) {
        buf[0] = '.';
        wi = 1;
    }

    buf[wi] = '\0';
    return buf;
}

bool ph_path_has_traversal(const char *path) {
    if (!path) return false;

    size_t len = strlen(path);
    for (size_t i = 0; i < len; i++) {
        /* check for ".." as a path component */
        if (path[i] != '.') continue;
        if (i + 1 >= len || path[i + 1] != '.') continue;

        /* verify it's at a component boundary */
        bool start_ok = (i == 0 || path[i - 1] == '/');
        bool end_ok   = (i + 2 >= len || path[i + 2] == '/');

        if (start_ok && end_ok) return true;
    }
    return false;
}

bool ph_path_is_absolute(const char *path) {
    return path && path[0] == '/';
}

char *ph_path_join(const char *base, const char *rel) {
    if (!base || !rel) return NULL;

    /* if rel is absolute, return a copy of rel */
    if (rel[0] == '/') {
        size_t len = strlen(rel);
        char *out = ph_alloc(len + 1);
        if (!out) return NULL;
        memcpy(out, rel, len + 1);
        return out;
    }

    size_t blen = strlen(base);
    size_t rlen = strlen(rel);

    /* remove trailing slash from base */
    while (blen > 1 && base[blen - 1] == '/') blen--;

    /* skip leading "./" from rel */
    while (rlen >= 2 && rel[0] == '.' && rel[1] == '/') {
        rel += 2;
        rlen -= 2;
    }

    size_t total = blen + 1 + rlen + 1;
    char *out = ph_alloc(total);
    if (!out) return NULL;

    memcpy(out, base, blen);
    out[blen] = '/';
    memcpy(out + blen + 1, rel, rlen);
    out[blen + 1 + rlen] = '\0';

    return out;
}

char *ph_path_dirname(const char *path) {
    if (!path) return NULL;

    size_t len = strlen(path);
    if (len == 0) {
        char *out = ph_alloc(2);
        if (!out) return NULL;
        out[0] = '.';
        out[1] = '\0';
        return out;
    }

    /* skip trailing slashes */
    size_t end = len;
    while (end > 1 && path[end - 1] == '/') end--;

    /* find last slash */
    size_t last_slash = end;
    while (last_slash > 0 && path[last_slash - 1] != '/') last_slash--;

    if (last_slash == 0) {
        char *out = ph_alloc(2);
        if (!out) return NULL;
        out[0] = '.';
        out[1] = '\0';
        return out;
    }

    /* skip trailing slashes from dirname */
    size_t dlen = last_slash;
    while (dlen > 1 && path[dlen - 1] == '/') dlen--;

    char *out = ph_alloc(dlen + 1);
    if (!out) return NULL;
    memcpy(out, path, dlen);
    out[dlen] = '\0';
    return out;
}

char *ph_path_basename(const char *path) {
    if (!path) return NULL;

    size_t len = strlen(path);
    if (len == 0) {
        char *out = ph_alloc(2);
        if (!out) return NULL;
        out[0] = '.';
        out[1] = '\0';
        return out;
    }

    /* skip trailing slashes */
    size_t end = len;
    while (end > 1 && path[end - 1] == '/') end--;

    /* find last slash before end */
    size_t start = end;
    while (start > 0 && path[start - 1] != '/') start--;

    size_t blen = end - start;
    char *out = ph_alloc(blen + 1);
    if (!out) return NULL;
    memcpy(out, path + start, blen);
    out[blen] = '\0';
    return out;
}

const char *ph_path_extension(const char *path) {
    if (!path) return NULL;

    /* find the basename first */
    const char *base = path;
    const char *p = path;
    while (*p) {
        if (*p == '/') base = p + 1;
        p++;
    }

    /* find last dot in basename */
    const char *dot = NULL;
    p = base;
    while (*p) {
        if (*p == '.') dot = p;
        p++;
    }

    /* no dot, or dot is first char (hidden file) */
    if (!dot || dot == base) return NULL;

    return dot;
}

char *ph_path_resolve(const char *path) {
    if (!path) return NULL;

    /* first try realpath directly -- works for existing paths */
    char *rp = realpath(path, NULL);
    if (rp) {
        size_t len = strlen(rp);
        char *out = ph_alloc(len + 1);
        if (!out) {
            free(rp);
            return NULL;
        }
        memcpy(out, rp, len + 1);
        free(rp);
        return out;
    }

    /*
     * fallback: walk back through parent directories until realpath()
     * succeeds, then append the remaining tail. reject if the tail
     * contains ".." traversal.
     */
    size_t plen = strlen(path);
    char *work = ph_alloc(plen + 1);
    if (!work) return NULL;
    memcpy(work, path, plen + 1);

    /* find longest existing prefix */
    char *tail = NULL;
    for (;;) {
        char *slash = strrchr(work, '/');
        if (!slash) {
            /* no more separators -- resolve "." and use all as tail */
            ph_free(work);
            char *cwd_rp = realpath(".", NULL);
            if (!cwd_rp) return NULL;
            if (ph_path_has_traversal(path)) {
                free(cwd_rp);
                return NULL;
            }
            size_t cwd_len = strlen(cwd_rp);
            size_t total = cwd_len + 1 + plen + 1;
            char *out = ph_alloc(total);
            if (!out) { free(cwd_rp); return NULL; }
            memcpy(out, cwd_rp, cwd_len);
            out[cwd_len] = '/';
            memcpy(out + cwd_len + 1, path, plen + 1);
            free(cwd_rp);
            return out;
        }

        if (tail) {
            /* shift existing tail to make room for new prefix piece */
            size_t tail_len = strlen(tail);
            size_t piece_len = strlen(slash + 1);
            char *new_tail = ph_alloc(piece_len + 1 + tail_len + 1);
            if (!new_tail) { ph_free(work); return NULL; }
            memcpy(new_tail, slash + 1, piece_len);
            new_tail[piece_len] = '/';
            memcpy(new_tail + piece_len + 1, tail, tail_len + 1);
            ph_free(tail);
            tail = new_tail;
        } else {
            size_t piece_len = strlen(slash + 1);
            tail = ph_alloc(piece_len + 1);
            if (!tail) { ph_free(work); return NULL; }
            memcpy(tail, slash + 1, piece_len + 1);
        }

        *slash = '\0';

        /* empty means we hit root "/" */
        const char *try_path = (work[0] == '\0') ? "/" : work;
        char *rp2 = realpath(try_path, NULL);
        if (rp2) {
            ph_free(work);
            if (ph_path_has_traversal(tail)) {
                free(rp2);
                ph_free(tail);
                return NULL;
            }
            size_t rp2_len = strlen(rp2);
            size_t tail_len = strlen(tail);
            size_t total = rp2_len + 1 + tail_len + 1;
            char *out = ph_alloc(total);
            if (!out) { free(rp2); ph_free(tail); return NULL; }
            memcpy(out, rp2, rp2_len);
            if (rp2_len == 1 && rp2[0] == '/') {
                memcpy(out + 1, tail, tail_len + 1);
            } else {
                out[rp2_len] = '/';
                memcpy(out + rp2_len + 1, tail, tail_len + 1);
            }
            free(rp2);
            ph_free(tail);
            return out;
        }

        if (work[0] == '\0') {
            /* reached root and nothing resolved */
            ph_free(work);
            ph_free(tail);
            return NULL;
        }
    }
}

bool ph_path_is_under(const char *child, const char *root) {
    if (!child || !root) return false;

    char *rchild = ph_path_resolve(child);
    if (!rchild) return false;
    char *rroot = ph_path_resolve(root);
    if (!rroot) { ph_free(rchild); return false; }

    size_t root_len = strlen(rroot);
    bool under = false;
    if (strncmp(rchild, rroot, root_len) == 0) {
        char next = rchild[root_len];
        /* either exact match, or root is "/" (root_len==1), or '/' follows */
        if (next == '\0' || next == '/' ||
            (root_len == 1 && rroot[0] == '/'))
            under = true;
    }

    ph_free(rchild);
    ph_free(rroot);
    return under;
}

char *ph_path_safe_join(const char *base, const char *rel, ph_error_t **err) {
    if (!base || !rel) {
        if (err)
            *err = ph_error_create(PH_ERR_INTERNAL, 0,
                "ph_path_safe_join: NULL argument");
        return NULL;
    }

    if (ph_path_is_absolute(rel)) {
        if (err)
            *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                "absolute path not allowed: %s", rel);
        return NULL;
    }

    if (ph_path_has_traversal(rel)) {
        if (err)
            *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                "path traversal not allowed: %s", rel);
        return NULL;
    }

    return ph_path_join(base, rel);
}
