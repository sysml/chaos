#include <h2/xen/vbd.h>
#ifdef CONFIG_H2_XEN_NOXS
#include <h2/xen/noxs.h>
#endif
#include <h2/xen/xs.h>


void h2_xen_vbd_reuse(h2_xen_dev_vbd* vbd)
{
    vbd->valid = false;
}

void h2_xen_vbd_free(h2_xen_dev_vbd* vbd)
{
    if (vbd->target) {
        free(vbd->target);
        vbd->target = NULL;
    }

    if (vbd->target_type) {
        free(vbd->target_type);
        vbd->target_type = NULL;
    }

    if (vbd->vdev) {
        free(vbd->vdev);
        vbd->vdev = NULL;
    }

    if (vbd->access) {
        free(vbd->access);
        vbd->access = NULL;
    }

    if (vbd->script) {
        free(vbd->script);
        vbd->script = NULL;
    }
}


int h2_xen_vbd_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vbd* vbd)
{
    int ret;

    if (vbd->valid) {
        ret = EINVAL;
        goto out;
    }

    switch (vbd->meth) {
        case h2_xen_dev_meth_t_xs:
            if (ctx->xs.active && guest->hyp.guest.xen->xs.active) {
                /* TODO */
                ret = 0;
            } else {
                ret = EINVAL;
            }
            break;

#ifdef CONFIG_H2_XEN_NOXS
        case h2_xen_dev_meth_t_noxs:
            if (ctx->noxs.active && guest->hyp.guest.xen->noxs.active) {
                /* TODO */
                ret = 0;
            } else {
                ret = EINVAL;
            }
            break;
#endif
    }

out:
    return ret;
}

int h2_xen_vbd_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vbd* vbd)
{
    int ret;

    if (!vbd->valid) {
        ret = EINVAL;
        goto out;
    }

    switch (vbd->meth) {
        case h2_xen_dev_meth_t_xs:
            if (ctx->xs.active && guest->hyp.guest.xen->xs.active) {
                /* TODO */
                ret = 0;
            } else {
                ret = EINVAL;
            }
            break;

#ifdef CONFIG_H2_XEN_NOXS
        case h2_xen_dev_meth_t_noxs:
            if (ctx->noxs.active && guest->hyp.guest.xen->noxs.active) {
                /* TODO */
                ret = 0;
            } else {
                ret = EINVAL;
            }
            break;
#endif
    }

out:
    return ret;
}
