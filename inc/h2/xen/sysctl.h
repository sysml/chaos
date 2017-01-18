#ifndef __H2__XEN__SYSCTL__H__
#define __H2__XEN__SYSCTL__H__

#include <h2/h2.h>


int h2_xen_sysctl_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_sysctl* sysctl);
int h2_xen_sysctl_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_sysctl* sysctl);

#endif /* __H2__XEN__SYSCTL__H__ */
