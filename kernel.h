/*
 * ps4-kexec - a kexec() implementation for Orbis OS / FreeBSD
 *
 * Copyright (C) 2015-2016 shuffle2 <godisgovernment@gmail.com>
 * Copyright (C) 2015-2016 Hector Martin "marcan" <marcan@marcan.st>
 *
 * This code is licensed to you under the 2-clause BSD license. See the LICENSE
 * file for more information.
 */

#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"

#define PAGE_SIZE 0x4000
#define PA_TO_DM(x) (((uintptr_t)x) | 0xfffffe0000000000ull)
#define DM_TO_ID(x) (((uintptr_t)x) & 0x000000ffffffffffull)

typedef u64 vm_paddr_t;
typedef u64 vm_offset_t;
typedef u64 vm_size_t;
typedef void * vm_map_t;
typedef char vm_memattr_t;
typedef void * pmap_t;

typedef void (*smp_rendezvous_callback_t)(void *);

struct ksym_t {
    int (*printf)(const char *fmt, ...);

    int (*copyin)(const void *uaddr, void *kaddr, size_t len);
    int (*copyout)(const void *kaddr, void *uaddr, size_t len);
    int (*copyinstr)(const void *uaddr, void *kaddr, size_t len, size_t *done);

    void **kernel_map;
    void *kernel_pmap_store;
    vm_offset_t (*kmem_alloc_contig)(vm_map_t map, vm_size_t size, int flags,
                                     vm_paddr_t low, vm_paddr_t high,
                                     unsigned long alignment,
                                     unsigned long boundary,
                                     vm_memattr_t memattr);
    void (*kmem_free)(vm_map_t, vm_offset_t, vm_size_t);
    vm_paddr_t (*pmap_extract)(pmap_t pmap, vm_offset_t va);

    void (*sched_pin)(void);
    void (*sched_unpin)(void);
    void (*smp_rendezvous)(smp_rendezvous_callback_t,
                           smp_rendezvous_callback_t,
                           smp_rendezvous_callback_t, void *);
    // yes...it is misspelled :)
    void (*smp_no_rendevous_barrier)(void *);
};

extern struct ksym_t kern;

// Assign a working printf function to this to debug the symbol resolver
extern int (*early_printf)(const char *fmt, ...);

void *kernel_resolve(const char *name);

void *kernel_alloc_contig(size_t size);
void kernel_free_contig(void *addr, size_t size);

int kernel_hook_install(void *target, void *hook);

int kernel_init(void);

#endif
