#include <h2/config.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define XEN_BDEV    "xvd"
#define IDE_BDEV    "hd"
#define SCSI_BDEV   "sd"


static bool is_bdev_xen(const char* vdev,
        int* out_disk, int* out_partition)
{
    return (sscanf(vdev, "d%dp%d", out_disk, out_partition) == 2);
}

static int disk_id_to_idx(const char* disk_id,
        int* out_disk_idx, int* out_disk_id_len)
{
    const char* p;
    int n = -1;

    p = disk_id;
    while (1) {
        char c = *p;

        if ('a' <= c && c <= 'z') {
            n++;
            n *= 26;
            n += c - 'a';

        } else {
            break;
        }

        p++;
    }

    *out_disk_idx = n;
    *out_disk_id_len = p - disk_id;

    return 0;
}

static bool is_bdev(const char* vdev, char* prefix,
        int* out_disk, int* out_partition)
{
    int ret;
    int _ret;

    const char* p;
    int prefix_len;
    int disk_id_len;

    ret = false;

    p = vdev;
    prefix_len = strlen(prefix);

    if (strncmp(p, prefix, prefix_len)) {
        goto out;
    }

    p += prefix_len;

    _ret = disk_id_to_idx(p, out_disk, &disk_id_len);
    if (_ret || disk_id_len <= 0) {
        goto out;
    }

    p += disk_id_len;

    if (*p == 0) {
        *out_partition = 0;

    } else if (*p == '0') {
        goto out;

    } else {
        *out_partition = atoi(p);
    }


    ret = true;

out:
    return ret;
}

int h2_vdev_to_vbd_id(const char* vdev,
        int* out_disk, int* out_partition)
{
    int vbd, disk, partition;

    vbd = -1;

    if (is_bdev_xen(vdev, &disk, &partition) ||
        is_bdev(vdev, XEN_BDEV, &disk, &partition)) {

        if (disk <= 15 && partition <= 15) {
            vbd = 202 << 8 | disk << 4 | partition;

        } else if (disk <= ((1 << 20) - 1) && partition <= 255) {
            vbd = 1 << 28 | disk << 8 | partition;

        } else {
            goto out;
        }

    } else if (is_bdev(vdev, IDE_BDEV, &disk, &partition)) {
        if (disk > 3 || partition > 63) {
            goto out;
        }

        if (disk < 2) {
            vbd = 3 << 8 | disk << 6 | partition;

        } else {
            vbd = 22 << 8 | (disk - 2) << 6 | partition;
        }

    } else if (is_bdev(vdev, SCSI_BDEV, &disk, &partition)) {
        if (disk > 15 || partition > 15) {
            goto out;
        }

        vbd = 8 << 8 | disk << 4 | partition;

    } else {
        goto out;
    }

    *out_disk = disk;
    *out_partition = partition;

out:
    return vbd;
}
