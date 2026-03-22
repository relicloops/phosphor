#ifndef PHOSPHOR_EMBEDDED_H
#define PHOSPHOR_EMBEDDED_H

#include "phosphor/types.h"

#include <stdbool.h>
#include <stddef.h>

/* ---- embedded template file registry ---- */

typedef struct {
    const char          *path;      /* relative path, e.g. "src/app.tsx" */
    const unsigned char *data;      /* buffer pointer */
    size_t               size;      /* buffer size in bytes */
    bool                 is_binary; /* true for SVG, images, fonts */
} ph_embedded_file_t;

/* lookup a single embedded file by relative path */
const ph_embedded_file_t *ph_embedded_lookup(const char *path);

/* total number of embedded files */
size_t ph_embedded_count(void);

/* pointer to the embedded file array */
const ph_embedded_file_t *ph_embedded_list(void);

/* write all embedded files to a directory (creates subdirs as needed) */
ph_result_t ph_embedded_write_to_dir(const char *dest_dir);

#endif /* PHOSPHOR_EMBEDDED_H */
