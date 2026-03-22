#include "phosphor/embedded.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"

#include <sys/stat.h>

ph_result_t ph_embedded_write_to_dir(const char *dest_dir) {
  if (!dest_dir)
    return PH_ERR;

  const ph_embedded_file_t *files = ph_embedded_list();
  size_t count = ph_embedded_count();

  for (size_t i = 0; i < count; i++) {
    char *full_path = ph_path_join(dest_dir, files[i].path);
    if (!full_path)
      return PH_ERR;

    /* ensure parent directory exists */
    char *dir = ph_path_dirname(full_path);
    if (dir) {
      ph_fs_mkdir_p(dir, 0755);
      ph_free(dir);
    }

    /* write file content */
    if (ph_fs_write_file(full_path, files[i].data, files[i].size) != PH_OK) {
      ph_log_error("failed to write embedded file: %s", full_path);
      ph_free(full_path);
      return PH_ERR;
    }

    ph_free(full_path);
  }

  return PH_OK;
}
