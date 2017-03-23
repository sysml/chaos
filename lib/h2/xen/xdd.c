#include <h2/xen/xdd.h>
#include <xddconn/if.h>

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <errno.h>


int xdd_vbd_add(h2_xen_dev_vbd* vbd)
{
    int ret;

    struct xdd_devreq req;
    struct xdd_devrsp rsp;
    struct xdd_blk_devreq* blk_req;

    req.hdr.req_type = dev_cmd_add;
    req.hdr.dev_type = dev_blk;
    req.hdr.payload_len = sizeof(blk_req->add) + strlen(vbd->target) + 1;

    blk_req = malloc(req.hdr.payload_len);
    if (blk_req == NULL) {
        ret = errno;
        goto out_ret;
    }

    strncpy(blk_req->add.type, vbd->target_type, XDD_BLK_TYPE_SIZE);
    strncpy(blk_req->add.mode, vbd->access, XDD_BLK_MODE_SIZE);
    strcpy(blk_req->add.filename, vbd->target);

    req.blk = blk_req;

    ret = xddconn_client_request(&req, &rsp);
    if (ret) {
        ret = errno;
        goto out_free_blk_req;
    }

    if (rsp.hdr.error == 0) {
        vbd->major = rsp.blk->add.major;
        vbd->minor = rsp.blk->add.minor;
    }

    ret = rsp.hdr.error;

out_free_blk_req:
    free(blk_req);
out_ret:
    return ret;
}

int xdd_vbd_remove(h2_xen_dev_vbd* vbd)
{
    int ret;

    struct xdd_devreq req;
    struct xdd_devrsp rsp;
    struct xdd_blk_devreq* blk_req;

    req.hdr.req_type = dev_cmd_remove;
    req.hdr.dev_type = dev_blk;
    req.hdr.payload_len = sizeof(blk_req->remove);

    blk_req = malloc(req.hdr.payload_len);
    if (blk_req == NULL) {
        ret = errno;
        goto out_ret;
    }

    strncpy(blk_req->remove.type, vbd->target_type, XDD_BLK_TYPE_SIZE);
    blk_req->remove.major = vbd->major;
    blk_req->remove.minor = vbd->minor;

    req.blk = blk_req;

    ret = xddconn_client_request(&req, &rsp);
    if (ret) {
        ret = errno;
        goto out_free_blk_req;
    }

    ret = rsp.hdr.error;

out_free_blk_req:
    free(blk_req);
out_ret:
    return ret;
}

int xdd_vbd_query(h2_xen_dev_vbd* vbd)
{
    int ret;

    struct xdd_devreq req;
    struct xdd_devrsp rsp;
    struct xdd_blk_devreq* blk_req;

    req.hdr.req_type = dev_cmd_query;
    req.hdr.dev_type = dev_blk;
    req.hdr.payload_len = sizeof(blk_req->query);

    blk_req = malloc(req.hdr.payload_len);
    if (blk_req == NULL) {
        ret = errno;
        goto out_ret;
    }

    strncpy(blk_req->query.type, vbd->target_type, XDD_BLK_TYPE_SIZE);
    blk_req->query.major = vbd->major;
    blk_req->query.minor = vbd->minor;

    req.blk = blk_req;

    ret = xddconn_client_request(&req, &rsp);
    if (ret) {
        ret = errno;
        goto out_free_blk_req;
    }

    if (rsp.hdr.error == 0) {
        vbd->target = strdup(rsp.blk->query.filename);
    }

    ret = rsp.hdr.error;

out_free_blk_req:
    free(blk_req);
out_ret:
    return ret;
}
