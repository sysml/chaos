#ifndef __H2__GUEST_CTRL__H__
#define __H2__GUEST_CTRL__H__

#include <h2/config.h>
#include <h2/guest.h>


struct h2_guest_ctrl_create {
    stream_desc sd;
    h2_serialized_cfg serialized_cfg;

    bool restore;

    int (*cb_read_header)(struct h2_guest_ctrl_create* gc);
    int (*cb_read_config)(struct h2_guest_ctrl_create* gc);
    int (*cb_do_config)(h2_serialized_cfg* cs, h2_hyp_t hyp, h2_guest** guest);
};
typedef struct h2_guest_ctrl_create h2_guest_ctrl_create;

int  h2_guest_ctrl_create_open(h2_guest_ctrl_create* gc, bool restore);
void h2_guest_ctrl_create_close(h2_guest_ctrl_create* gc);


struct h2_guest_ctrl_save {
    stream_desc sd;
    h2_serialized_cfg serialized_cfg;

    int (*cb_do_config)(h2_serialized_cfg* cs, h2_hyp_t hyp, h2_guest* guest);
    int (*cb_write_header)(struct h2_guest_ctrl_save* gs);
    int (*cb_write_config)(struct h2_guest_ctrl_save* gs);
};
typedef struct h2_guest_ctrl_save h2_guest_ctrl_save;

int  h2_guest_ctrl_save_open(h2_guest_ctrl_save* gs);
void h2_guest_ctrl_save_close(h2_guest_ctrl_save* gs);

#endif /* __H2__GUEST_CTRL__H__ */
