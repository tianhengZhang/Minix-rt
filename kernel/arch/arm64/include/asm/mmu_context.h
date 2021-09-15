/*
 * Based on arch/arm/include/asm/mmu_context.h
 *
 * Copyright (C) 1996 Russell King.
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_MMU_CONTEXT_H_
#define __ASM_MMU_CONTEXT_H_

#ifndef __ASSEMBLY__

#include <base/types.h>

#include <sel4m/sched.h>
#include <sel4m/smp.h>

#include <asm/base/barrier.h>

#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#include <asm/sysreg.h>
#include <asm/memory.h>
#include <asm/current.h>

/*
 * TCR.T0SZ value to use when the ID map is active. Usually equals
 * TCR_T0SZ(VA_BITS), unless system RAM is positioned very high in
 * physical memory, in which case it will be smaller.
 */
extern u64 idmap_t0sz;
extern u64 idmap_ptrs_per_pgd;

extern void cpu_do_idle(void);
extern void cpu_do_switch_mm(unsigned long pgd_phys, struct mm_struct *mm);
extern void check_and_switch_context(struct mm_struct *mm, unsigned int cpu);

#define init_new_context(tsk,mm)	({ atomic64_set(&(mm)->context.id, 0); 0; })

static inline void contextidr_thread_switch(struct task_struct *next)
{

	write_sysreg(task_pid_nr(next), contextidr_el1);
	isb();
}

/*
 * Set TTBR0 to empty_zero_page. No translations will be possible via TTBR0.
 */
static inline void cpu_set_reserved_ttbr0(void)
{
	unsigned long ttbr = __pa_symbol(empty_zero_page);

	write_sysreg(ttbr, ttbr0_el1);
	isb();
}

static inline void cpu_switch_mm(pgd_t *pgd, struct mm_struct *mm)
{
	BUG_ON(pgd == swapper_pg_dir);
	cpu_set_reserved_ttbr0();

	cpu_do_switch_mm(virt_to_phys(pgd),mm);
}

static inline bool __cpu_uses_extended_idmap(void)
{
	return unlikely(idmap_t0sz != TCR_T0SZ(VA_BITS));
}

/*
 * True if the extended ID map requires an extra level of translation table
 * to be configured.
 */
static inline bool __cpu_uses_extended_idmap_level(void)
{
	return ARM64_HW_PGTABLE_LEVELS(64 - idmap_t0sz) > CONFIG_PGTABLE_LEVELS;
}

/*
 * Set TCR.T0SZ to its default value (based on VA_BITS)
 */
static inline void __cpu_set_tcr_t0sz(unsigned long t0sz)
{
	unsigned long tcr;

	if (!__cpu_uses_extended_idmap())
		return;

	tcr = read_sysreg(tcr_el1);
	tcr &= ~TCR_T0SZ_MASK;
	tcr |= t0sz << TCR_T0SZ_OFFSET;
	write_sysreg(tcr, tcr_el1);
	isb();
}

#define cpu_set_default_tcr_t0sz()	__cpu_set_tcr_t0sz(TCR_T0SZ(VA_BITS))
#define cpu_set_idmap_tcr_t0sz()	__cpu_set_tcr_t0sz(idmap_t0sz)

/*
 * Remove the idmap from TTBR0_EL1 and install the pgd of the active mm.
 *
 * The idmap lives in the same VA range as userspace, but uses global entries
 * and may use a different TCR_EL1.T0SZ. To avoid issues resulting from
 * speculative TLB fetches, we must temporarily install the reserved page
 * tables while we invalidate the TLBs and set up the correct TCR_EL1.T0SZ.
 *
 * If current is a not a user task, the mm covers the TTBR1_EL1 page tables,
 * which should not be installed in TTBR0_EL1. In this case we can leave the
 * reserved page tables in place.
 */
static inline void cpu_uninstall_idmap(void)
{
	struct mm_struct *mm = current->mm;

	cpu_set_reserved_ttbr0();
	local_flush_tlb_all();
	cpu_set_default_tcr_t0sz();

	if (mm != &init_mm)
		cpu_switch_mm(mm->pgd, mm);
}

static inline void cpu_install_idmap(void)
{
	cpu_set_reserved_ttbr0();
	local_flush_tlb_all();
	cpu_set_idmap_tcr_t0sz();

	cpu_switch_mm(__va(__pa_symbol(idmap_pg_dir)), &init_mm);
}

/*
 * Atomically replaces the active TTBR1_EL1 PGD with a new VA-compatible PGD,
 * avoiding the possibility of conflicting TLB entries being allocated.
 */
static inline void cpu_replace_ttbr1(pgd_t *pgdp)
{
	typedef void (ttbr_replace_func)(phys_addr_t);
	extern ttbr_replace_func idmap_cpu_replace_ttbr1;
	ttbr_replace_func *replace_phys;

	/* phys_to_ttbr() zeros lower 2 bits of ttbr with 52-bit PA */
	phys_addr_t ttbr1 = virt_to_phys(pgdp);

	replace_phys = (void *)__pa_symbol(idmap_cpu_replace_ttbr1);

	cpu_install_idmap();
	replace_phys(ttbr1);
	cpu_uninstall_idmap();
}

static inline void __switch_mm(struct mm_struct *next)
{
	unsigned int cpu = smp_processor_id();

	/*
	 * init_mm.pgd does not contain any user mappings and it is always
	 * active for kernel addresses in TTBR1. Just set the reserved TTBR0.
	 */
	if (next == &init_mm) {
		cpu_set_reserved_ttbr0();
		return;
	}

	check_and_switch_context(next, cpu);
}

static inline void
switch_mm(struct mm_struct *prev, struct mm_struct *next,
	  struct task_struct *tsk)
{
	if (prev != next)
		__switch_mm(next);
}

#endif /* !__ASSEMBLY__ */
#endif /* !__ASM_MMU_CONTEXT_H_ */
