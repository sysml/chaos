#include <h2/xen/sysctl.h>
#include <h2/xen/noxs.h>


int h2_xen_sysctl_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_sysctl* sysctl)
{
    int ret = EINVAL;

#ifdef CONFIG_H2_XEN_NOXS
    if (ctx->noxs.active && guest->hyp.guest.xen->noxs.active) {
        ret = h2_xen_noxs_sysctl_create(ctx, guest, sysctl);
    }
#endif

    return ret;
}

int h2_xen_sysctl_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_sysctl* sysctl)
{
    int ret = EINVAL;

#ifdef CONFIG_H2_XEN_NOXS
    if (ctx->noxs.active && guest->hyp.guest.xen->noxs.active) {
        ret = h2_xen_noxs_sysctl_destroy(ctx, guest, sysctl);
    }
#endif

    return ret;
}
