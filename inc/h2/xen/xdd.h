#ifndef __H2__XEN__XDD__H__
#define __H2__XEN__XDD__H__

#include <h2/xen/def.h>

int xdd_vbd_query(h2_xen_dev_vbd* vbd);
int xdd_vbd_add(h2_xen_dev_vbd* vbd);
int xdd_vbd_remove(h2_xen_dev_vbd* vbd);

#endif /* __H2__XEN__XDD__H__ */
