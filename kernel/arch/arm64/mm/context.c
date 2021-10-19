/*
 * Based on arch/arm/mm/context.c
 *
 * Copyright (C) 2002-2003 Deep Blue Solutions Ltd, all rights reserved.
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
#include <base/types.h>
#include <base/linkage.h>
#include <base/overflow.h>

#include <minix_rt/cpumask.h>
#include <minix_rt/spinlock.h>
#include <minix_rt/smp.h>
#include <minix_rt/memory.h>

#include <asm/mmu_context.h>
#include <asm/sysreg.h>

static u32 asid_bits;
static DEFINE_RAW_SPINLOCK(cpu_asid_lock);

static atomic64_t asid_generation;
static unsigned long *asid_map;

static atomic64_t active_asids[CONFIG_NR_CPUS];
static u64 reserved_asids[CONFIG_NR_CPUS];
static cpumask_t tlb_flush_pending;

#define ASID_MASK		(~GENMASK(asid_bits - 1, 0))
#define ASID_FIRST_VERSION	(1UL << asid_bits)

#define NUM_USER_ASIDS		(ASID_FIRST_VERSION)
#define asid2idx(asid)		((asid) & ~ASID_MASK)
#define idx2asid(idx)		asid2idx(idx)

static inline unsigned int __attribute_const__
cpuid_feature_extract_unsigned_field_width(u64 features, int field, int width)
{
	return (u64)(features << (64 - width - field)) >> (64 - width);
}

static inline unsigned int __attribute_const__
cpuid_feature_extract_unsigned_field(u64 features, int field)
{
	return cpuid_feature_extract_unsigned_field_width(features, field, 4);
}

/* Get the ASIDBits supported by the current CPU */
static u32 get_cpu_asid_bits(void)
{
	u32 asid;
	int fld = cpuid_feature_extract_unsigned_field(read_cpuid(ID_AA64MMFR0_EL1),
						ID_AA64MMFR0_ASID_SHIFT);

	switch (fld) {
	default:
		printf("CPU%lld: Unknown ASID size (%d); assuming 8-bit\n",
					smp_processor_id(),  fld);
		/* Fallthrough */
	case 0:
		asid = 8;
		break;
	case 2:
		asid = 16;
	}

	return asid;
}

/* Check if the current cpu's ASIDBits is compatible with asid_bits */
void verify_cpu_asid_bits(void)
{
	u32 asid = get_cpu_asid_bits();

	if (asid < asid_bits) {
		/*
		 * We cannot decrease the ASID size at runtime, so panic if we support
		 * fewer ASID bits than the boot CPU.
		 */
		printf("CPU%lld: smaller ASID size(%u) than boot CPU (%u)\n",
				smp_processor_id(), asid, asid_bits);
		cpu_panic_kernel();
	}
}

static void flush_context(void)
{
	int i;
	u64 asid;

	/* Update the list of reserved ASIDs and the ASID bitmap. */
	bitmap_clear(asid_map, 0, NUM_USER_ASIDS);

	for_each_possible_cpu(i) {
		asid = atomic64_xchg_relaxed(&active_asids[i], 0);
		/*
		 * If this CPU has already been through a
		 * rollover, but hasn't run another task in
		 * the meantime, we must preserve its reserved
		 * ASID, as this is the only trace we have of
		 * the process it is still running.
		 */
		if (asid == 0)
			asid = reserved_asids[i];
		__set_bit(asid2idx(asid), asid_map);
		reserved_asids[i] = asid;
	}

	/*
	 * Queue a TLB invalidation for each CPU to perform on next
	 * context-switch
	 */
	cpumask_setall(&tlb_flush_pending);
}

static bool check_update_reserved_asid(u64 asid, u64 newasid)
{
	int cpu;
	bool hit = false;

	/*
	 * Iterate over the set of reserved ASIDs looking for a match.
	 * If we find one, then we can update our mm to use newasid
	 * (i.e. the same ASID in the current generation) but we can't
	 * exit the loop early, since we need to ensure that all copies
	 * of the old ASID are updated to reflect the mm. Failure to do
	 * so could result in us missing the reserved ASID in a future
	 * generation.
	 */
	for_each_possible_cpu(cpu) {
		if (reserved_asids[cpu] == asid) {
			hit = true;
			reserved_asids[cpu] = newasid;
		}
	}

	return hit;
}

static u64 new_context(struct mm_struct *mm)
{
	static u32 cur_idx = 1;
	u64 asid = atomic64_read(&mm->context.id);
	u64 generation = atomic64_read(&asid_generation);

	if (asid != 0) {
		u64 newasid = generation | (asid & ~ASID_MASK);

		/*
		 * If our current ASID was active during a rollover, we
		 * can continue to use it and this was just a false alarm.
		 */
		if (check_update_reserved_asid(asid, newasid))
			return newasid;

		/*
		 * We had a valid ASID in a previous life, so try to re-use
		 * it if possible.
		 */
		if (!__test_and_set_bit(asid2idx(asid), asid_map))
			return newasid;
	}

	/*
	 * Allocate a free ASID. If we can't find one, take a note of the
	 * currently active ASIDs and mark the TLBs as requiring flushes.  We
	 * always count from ASID #2 (index 1), as we use ASID #0 when setting
	 * a reserved TTBR0 for the init_mm and we allocate ASIDs in even/odd
	 * pairs.
	 */
	asid = find_next_zero_bit(asid_map, NUM_USER_ASIDS, cur_idx);
	if (asid != NUM_USER_ASIDS)
		goto set_asid;

	/* We're out of ASIDs, so increment the global generation count */
	generation = atomic64_add_return_relaxed(ASID_FIRST_VERSION,
						 &asid_generation);
	flush_context();

	/* We have more ASIDs than CPUs, so this will always succeed */
	asid = find_next_zero_bit(asid_map, NUM_USER_ASIDS, 1);

set_asid:
	__set_bit(asid, asid_map);
	cur_idx = asid;
	return idx2asid(asid) | generation;
}

void check_and_switch_context(struct mm_struct *mm, unsigned int cpu)
{
	u64 flags;
	u64 asid, old_active_asid;

	asid = atomic64_read(&mm->context.id);

	/*
	 * The memory ordering here is subtle.
	 * If our active_asids is non-zero and the ASID matches the current
	 * generation, then we update the active_asids entry with a relaxed
	 * cmpxchg. Racing with a concurrent rollover means that either:
	 *
	 * - We get a zero back from the cmpxchg and end up waiting on the
	 *   lock. Taking the lock synchronises with the rollover and so
	 *   we are forced to see the updated generation.
	 *
	 * - We get a valid ASID back from the cmpxchg, which means the
	 *   relaxed xchg in flush_context will treat us as reserved
	 *   because atomic RmWs are totally ordered for a given location.
	 */
	old_active_asid = atomic64_read(&active_asids[cpu]);
	if (old_active_asid &&
	    !((asid ^ atomic64_read(&asid_generation)) >> asid_bits) &&
	    atomic64_cmpxchg_relaxed(&active_asids[cpu],
				     old_active_asid, asid))
		goto switch_mm_fastpath;

	raw_spin_lock_irqsave(&cpu_asid_lock, flags);
	/* Check that our ASID belongs to the current generation. */
	asid = atomic64_read(&mm->context.id);
	if ((asid ^ atomic64_read(&asid_generation)) >> asid_bits) {
		asid = new_context(mm);
		atomic64_set(&mm->context.id, asid);
	}

	if (cpumask_test_and_clear_cpu(cpu, &tlb_flush_pending))
		local_flush_tlb_all();

	atomic64_set(&active_asids[cpu], asid);
	raw_spin_unlock_irqrestore(&cpu_asid_lock, flags);

switch_mm_fastpath:
	cpu_switch_mm(mm->pgd, mm);
}

/* Errata workaround post TTBRx_EL1 update. */
asmlinkage void post_ttbr_update_workaround(void)
{
	asm("nop; nop; nop");
}

int asids_init(void)
{
	phys_addr_t phys_addr;
	size_t bytes;
	asid_bits = get_cpu_asid_bits();
	/*
	 * Expect allocation after rollover to fail if we don't have at least
	 * one more ASID than CPUs. ASID #0 is reserved for init_mm.
	 */
	WARN_ON(NUM_USER_ASIDS - 1 <= num_possible_cpus());
	atomic64_set(&asid_generation, ASID_FIRST_VERSION);

	if (unlikely(check_mul_overflow(BITS_TO_LONGS(NUM_USER_ASIDS), sizeof(*asid_map), &bytes)))
		return -1;

	phys_addr = memblock_alloc(&memblock_kernel, bytes, sizeof(*asid_map));
	if (!phys_addr)
		panic("Failed to allocate bitmap for %lu ASIDs\n",
		      NUM_USER_ASIDS);
	
	asid_map = phys_to_virt(phys_addr);

	memset(asid_map, '\0', bytes);

	printf("ASID allocator initialised with %lu entries\n", NUM_USER_ASIDS);
	return 0;
}
