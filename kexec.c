/*
 * ps4-kexec - a kexec() implementation for Orbis OS / FreeBSD
 *
 * Copyright (C) 2015-2016 shuffle2 <godisgovernment@gmail.com>
 * Copyright (C) 2015-2016 Hector Martin "marcan" <marcan@marcan.st>
 *
 * This code is licensed to you under the 2-clause BSD license. See the LICENSE
 * file for more information.
 */

#include "kernel.h"
#include "linux_boot.h"
#include "x86.h"
#include "kexec.h"
#include "firmware.h"

int sys_kexec(void *td, struct sys_kexec_args *uap)
{
    int err = 0;
    size_t initramfs_size = uap->initramfs_size;
    void *image = NULL;
    void *initramfs = NULL;
    struct boot_params *bp = NULL;
    size_t cmd_line_maxlen = 0;
    char *cmd_line = NULL;

    kern.printf("sys_kexec invoked\n");

    // Look up our shutdown hook point
    void *icc_query_nowait = kernel_resolve("icc_query_nowait");
    if (!icc_query_nowait) {
        err = 2; // ENOENT
        goto cleanup;
    }

    // Copy in kernel image
    image = kernel_alloc_contig(uap->image_size);
    if (!image) {
        kern.printf("Failed to allocate image\n");
        err = 12; // ENOMEM
        goto cleanup;
    }
    err = kern.copyin(uap->image, image, uap->image_size);
    if (err) {
        kern.printf("Failed to copy in image\n");
        goto cleanup;
    }

    // Copy in initramfs
    initramfs = kernel_alloc_contig(initramfs_size + FW_CPIO_SIZE);
    if (!initramfs) {
        kern.printf("Failed to allocate initramfs\n");
        err = 12; // ENOMEM
        goto cleanup;
    }
    if (initramfs_size) {
        err = kern.copyin(uap->initramfs, initramfs, initramfs_size);
        if (err) {
            kern.printf("Failed to copy in initramfs\n");
            goto cleanup;
        }
    }

    err = firmware_extract(((u8*)initramfs) + initramfs_size);
    if (err < 0) {
        kern.printf("Failed to extract GPU firmware - continuing anyway\n");
    } else {
        initramfs_size += err;
    }

    // Copy in cmdline
    cmd_line_maxlen = ((struct boot_params *)image)->hdr.cmdline_size + 1;
    cmd_line = kernel_alloc_contig(cmd_line_maxlen);
    if (!cmd_line) {
        kern.printf("Failed to allocate cmdline\n");
        err = 12;
        goto cleanup;
    }
    err = kern.copyinstr(uap->cmd_line, cmd_line, cmd_line_maxlen, NULL);
    if (err) {
        kern.printf("Failed to copy in cmdline\n");
        goto cleanup;
    }
    cmd_line[cmd_line_maxlen - 1] = 0;

    kern.printf("\nkexec parameters:\n");
    kern.printf("    Kernel image size:   %d bytes\n", uap->image_size);
    kern.printf("    Initramfs size:      %d bytes (%d from user)\n",
                initramfs_size, uap->initramfs_size);
    kern.printf("    Kernel command line: %s\n", cmd_line);
    kern.printf("    Kernel image buffer: %p\n", image);
    kern.printf("    Initramfs buffer:    %p\n", initramfs);

    // Allocate our boot params
    bp = kernel_alloc_contig(sizeof(*bp));
    if (!bp) {
        kern.printf("Failed to allocate bp\n");
        err = 12;
        goto cleanup;
    }

    // Initialize bp
    // TODO should probably do this from cpu_quiesce_gate, then bp doesn't
    // need to be allocated here, just placed directly into low mem
    prepare_boot_params(bp, image);

    set_nix_info(image, bp, initramfs, initramfs_size, cmd_line);

    // Hook the final ICC shutdown function
    if (!kernel_hook_install(hook_icc_query_nowait, icc_query_nowait)) {
        kern.printf("Failed to install shutdown hook\n");
        err = 22;
        goto cleanup;
    }

    kern.printf("******************************************************\n");
    kern.printf("kexec successfully armed. Please shut down the system.\n");
    kern.printf("******************************************************\n\n");

    return 0;

cleanup:
    kernel_free_contig(cmd_line, cmd_line_maxlen);
    kernel_free_contig(bp, sizeof(*bp));
    kernel_free_contig(image, uap->image_size);
    kernel_free_contig(initramfs, uap->initramfs_size);
    return err;
}
