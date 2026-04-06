#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"

void handle_tick(ph_dashboard_t *db) {
    if (db->cmd_msg_frames > 0)
        db->cmd_msg_frames--;

    if (db->btn_flash > 0)
        db->btn_flash--;
}

#endif /* PHOSPHOR_HAS_NCURSES */
