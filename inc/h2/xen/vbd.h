#ifndef __H2__XEN__VBD__H__
#define __H2__XEN__VBD__H__

#include <h2/h2.h>


void h2_xen_vbd_reuse(h2_xen_dev_vbd* vbd);
void h2_xen_vbd_free(h2_xen_dev_vbd* vbd);


int h2_xen_vbd_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vbd* vbd);
int h2_xen_vbd_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vbd* vbd);

#endif /* __H2__XEN__VBD__H__ */
