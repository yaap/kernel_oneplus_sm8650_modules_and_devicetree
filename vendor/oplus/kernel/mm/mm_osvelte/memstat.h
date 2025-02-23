/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#ifndef _OSVELTE_MEMSTAT_H
#define _OSVELTE_MEMSTAT_H

/* not safe, need upstream patch. */
#define ASHMEM_NAME_LEN		256
#define ASHMEM_NAME_PREFIX "dev/ashmem/"
#define ASHMEM_NAME_DEF	"dev/ashmem"
#define ASHMEM_NAME_PREFIX_LEN (sizeof(ASHMEM_NAME_PREFIX) - 1)
#define ASHMEM_FULL_NAME_LEN (ASHMEM_NAME_LEN + ASHMEM_NAME_PREFIX_LEN)

struct ashmem_area {
	char name[ASHMEM_FULL_NAME_LEN];
	struct list_head unpinned_list;
	struct file *file;
	size_t size;
	unsigned long prot_mask;
};

#define PAGES(size) ((size) >> PAGE_SHIFT)
#define K(x) ((x) << (PAGE_SHIFT - 10))

extern inline int is_dma_buf_file(struct file *file);
#ifdef CONFIG_ASHMEM
extern int is_ashmem_file(struct file *file);
#else
static inline int is_ashmem_file(struct file *file)
{
	return false;
}
#endif /* CONFIG_ASHMEM */
struct task_struct *find_lock_task_mm_dup(struct task_struct *p);

#endif /* _OSVELTE_MEMSTAT_H */
