#include "phosphor/fs.h"
#include "phosphor/platform.h"
#include "phosphor/alloc.h"

ph_result_t ph_io_read_file(const char *path, uint8_t **out_data,
                             size_t *out_len, ph_error_t **err) {
    if (!path || !out_data || !out_len) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_io_read_file: NULL argument");
        return PH_ERR;
    }

    if (ph_fs_read_file(path, out_data, out_len) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "cannot read file: %s", path);
        return PH_ERR;
    }
    return PH_OK;
}

ph_result_t ph_io_write_file(const char *path, const uint8_t *data,
                              size_t len, ph_error_t **err) {
    if (!path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_io_write_file: NULL path");
        return PH_ERR;
    }

    if (ph_fs_write_file(path, data, len) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "cannot write file: %s", path);
        return PH_ERR;
    }
    return PH_OK;
}
