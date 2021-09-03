/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SEL4M_MM_TYPES_H_
#define __SEL4M_MM_TYPES_H_

#ifndef __ASSEMBLY__

#include <base/common.h>

#include <asm/pgtable-types.h>
#include <asm/mmu.h>
struct page {
	int a;
};

struct mm_struct {
	pgd_t * pgd;
	/* Architecture-specific MM context */
	mm_context_t context;
};

/*
 * Used for sizing the vmemmap region on some architectures
 */
#define STRUCT_PAGE_MAX_SHIFT	(order_base_2(sizeof(struct page)))

#endif /* !__ASSEMBLY__ */
#endif /* !__SEL4M_MM_TYPES_H_ */