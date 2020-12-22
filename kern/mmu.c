/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * KVM/MIPS MMU handling in the KVM module.
 *
 * Copyright (C) 2012  MIPS Technologies, Inc.  All rights reserved.
 * Authors: Sanjay Lal <sanjayl@kymasys.com>
 */

#include <linux/highmem.h>
#include <linux/hugetlb.h>
#include <linux/page-flags.h>
#include <linux/uaccess.h>
#include <linux/nospec.h>
#include <linux/srcu.h>
#include <linux/swap.h>

#include <asm/mmu_context.h>
#include <asm/pgalloc.h>

#include "vz.h"
#include "memslot.h"

#define KVM_MMU_CACHE_MIN_PAGES 2

static int mmu_topup_memory_cache(struct kvm_mmu_memory_cache *cache, int min,
				  int max)
{
	void *page;

	BUG_ON(max > KVM_NR_MEM_OBJS);
	if (cache->nobjs >= min)
		return 0;
	while (cache->nobjs < max) {
		page = (void *)__get_free_page(GFP_KERNEL);
		if (!page)
			return -ENOMEM;
		cache->objects[cache->nobjs++] = page;
	}
	return 0;
}

static void mmu_free_memory_cache(struct kvm_mmu_memory_cache *mc)
{
	while (mc->nobjs)
		free_page((unsigned long)mc->objects[--mc->nobjs]);
}

static void *mmu_memory_cache_alloc(struct kvm_mmu_memory_cache *mc)
{
	void *p;

	BUG_ON(!mc || !mc->nobjs);
	p = mc->objects[--mc->nobjs];
	return p;
}

void kvm_mmu_free_memory_caches(struct vz_vcpu *vcpu)
{
	mmu_free_memory_cache(&vcpu->mmu_page_cache);
}

/**
 * kvm_pgd_init() - Initialise KVM GPA page directory.
 * @page:	Pointer to page directory (PGD) for KVM GPA.
 *
 * Initialise a KVM GPA page directory with pointers to the invalid table, i.e.
 * representing no mappings. This is similar to pgd_init(), however it
 * initialises all the page directory pointers, not just the ones corresponding
 * to the userland address space (since it is for the guest physical address
 * space rather than a virtual address space).
 */
static void kvm_pgd_init(void *page)
{
	unsigned long *p, *end;
	unsigned long entry;

	entry = (unsigned long)invalid_pmd_table;

	p = (unsigned long *)page;
	end = p + PTRS_PER_PGD;

	do {
		p[0] = entry;
		p[1] = entry;
		p[2] = entry;
		p[3] = entry;
		p[4] = entry;
		p += 8;
		p[-3] = entry;
		p[-2] = entry;
		p[-1] = entry;
	} while (p != end);
}

/**
 * kvm_pgd_alloc() - Allocate and initialise a KVM GPA page directory.
 *
 * Allocate a blank KVM GPA page directory (PGD) for representing guest physical
 * to host physical page mappings.
 *
 * Returns:	Pointer to new KVM GPA page directory.
 *		NULL on allocation failure.
 */
pgd_t *kvm_pgd_alloc(void)
{
	pgd_t *ret;

	ret = (pgd_t *)__get_free_pages(GFP_KERNEL, PGD_ORDER);
	if (ret)
		kvm_pgd_init(ret);

	return ret;
}

/**
 * kvm_mips_walk_pgd() - Walk page table with optional allocation.
 * @pgd:	Page directory pointer.
 * @addr:	Address to index page table using.
 * @cache:	MMU page cache to allocate new page tables from, or NULL.
 *
 * Walk the page tables pointed to by @pgd to find the PTE corresponding to the
 * address @addr. If page tables don't exist for @addr, they will be created
 * from the MMU cache if @cache is not NULL.
 *
 * Returns:	Pointer to pte_t corresponding to @addr.
 *		NULL if a page table doesn't exist for @addr and !@cache.
 *		NULL if a page table allocation failed.
 */
static pte_t *kvm_mips_walk_pgd(pgd_t *pgd, struct kvm_mmu_memory_cache *cache,
				unsigned long addr)
{
	pud_t *pud;
	pmd_t *pmd;

	pgd += pgd_index(addr);
	if (pgd_none(*pgd)) {
		/* Not used on MIPS yet */
		BUG();
		return NULL;
	}
	pud = pud_offset(pgd, addr);
	if (pud_none(*pud)) {
		pmd_t *new_pmd;

		if (!cache)
			return NULL;
		new_pmd = mmu_memory_cache_alloc(cache);
		pmd_init((unsigned long)new_pmd,
			 (unsigned long)invalid_pte_table);
		pud_populate(NULL, pud, new_pmd);
	}
	pmd = pmd_offset(pud, addr);
#ifdef CONFIG_MIPS_HUGE_TLB_SUPPORT
	if (pmd_huge(*pmd)) {
		return (pte_t *)pmd;
	}
#endif
	if (pmd_none(*pmd)) {
		pte_t *new_pte;

		if (!cache)
			return NULL;
		new_pte = mmu_memory_cache_alloc(cache);
		clear_page(new_pte);
		pmd_populate_kernel(NULL, pmd, new_pte);
	}
	return pte_offset(pmd, addr);
}

/* Caller must hold kvm->mm_lock */
static pte_t *kvm_mips_pte_for_gpa(struct vz_vm *kvm,
				   struct kvm_mmu_memory_cache *cache,
				   unsigned long addr)
{
	return kvm_mips_walk_pgd(kvm->gpa_mm.pgd, cache, addr);
}

/*
 * kvm_mips_flush_gpa_{pte,pmd,pud,pgd,pt}.
 * Flush a range of guest physical address space from the VM's GPA page tables.
 */

static bool kvm_mips_flush_gpa_pte(pte_t *pte, unsigned long start_gpa,
				   unsigned long end_gpa)
{
	int i_min = __pte_offset(start_gpa);
	int i_max = __pte_offset(end_gpa);
	bool safe_to_remove = (i_min == 0 && i_max == PTRS_PER_PTE - 1);
	int i;

	for (i = i_min; i <= i_max; ++i) {
		if (!pte_present(pte[i]))
			continue;

		set_pte(pte + i, __pte(0));
	}
	return safe_to_remove;
}

static bool kvm_mips_flush_gpa_pmd(pmd_t *pmd, unsigned long start_gpa,
				   unsigned long end_gpa)
{
	pte_t *pte;
	unsigned long end = ~0ul;
	int i_min = __pmd_offset(start_gpa);
	int i_max = __pmd_offset(end_gpa);
	bool safe_to_remove = (i_min == 0 && i_max == PTRS_PER_PMD - 1);
	int i;

	for (i = i_min; i <= i_max; ++i, start_gpa = 0) {
		if (!pmd_present(pmd[i]))
			continue;

		if (pmd_huge(pmd[i])) {
			pmd_clear(pmd + i);
			continue;
		}

		pte = pte_offset(pmd + i, 0);
		if (i == i_max)
			end = end_gpa;

		if (kvm_mips_flush_gpa_pte(pte, start_gpa, end)) {
			pmd_clear(pmd + i);
			pte_free_kernel(NULL, pte);
		} else {
			safe_to_remove = false;
		}
	}
	return safe_to_remove;
}

static bool kvm_mips_flush_gpa_pud(pud_t *pud, unsigned long start_gpa,
				   unsigned long end_gpa)
{
	pmd_t *pmd;
	unsigned long end = ~0ul;
	int i_min = __pud_offset(start_gpa);
	int i_max = __pud_offset(end_gpa);
	bool safe_to_remove = (i_min == 0 && i_max == PTRS_PER_PUD - 1);
	int i;

	for (i = i_min; i <= i_max; ++i, start_gpa = 0) {
		if (!pud_present(pud[i]))
			continue;

		pmd = pmd_offset(pud + i, 0);
		if (i == i_max)
			end = end_gpa;

		if (kvm_mips_flush_gpa_pmd(pmd, start_gpa, end)) {
			pud_clear(pud + i);
			pmd_free(NULL, pmd);
		} else {
			safe_to_remove = false;
		}
	}
	return safe_to_remove;
}

static bool kvm_mips_flush_gpa_pgd(pgd_t *pgd, unsigned long start_gpa,
				   unsigned long end_gpa)
{
	pud_t *pud;
	unsigned long end = ~0ul;
	int i_min = pgd_index(start_gpa);
	int i_max = pgd_index(end_gpa);
	bool safe_to_remove = (i_min == 0 && i_max == PTRS_PER_PGD - 1);
	int i;

	for (i = i_min; i <= i_max; ++i, start_gpa = 0) {
		if (!pgd_present(pgd[i]))
			continue;

		pud = pud_offset(pgd + i, 0);
		if (i == i_max)
			end = end_gpa;

		if (kvm_mips_flush_gpa_pud(pud, start_gpa, end)) {
			pgd_clear(pgd + i);
			pud_free(NULL, pud);
		} else {
			safe_to_remove = false;
		}
	}
	return safe_to_remove;
}

/**
 * kvm_mips_flush_gpa_pt() - Flush a range of guest physical addresses.
 * @kvm:	KVM pointer.
 * @start_gfn:	Guest frame number of first page in GPA range to flush.
 * @end_gfn:	Guest frame number of last page in GPA range to flush.
 *
 * Flushes a range of GPA mappings from the GPA page tables.
 *
 * The caller must hold the @kvm->mmu_lock spinlock.
 *
 * Returns:	Whether its safe to remove the top level page directory because
 *		all lower levels have been removed.
 */
bool kvm_mips_flush_gpa_pt(struct vz_vm *kvm, gfn_t start_gfn, gfn_t end_gfn)
{
	return kvm_mips_flush_gpa_pgd(kvm->gpa_mm.pgd, start_gfn << PAGE_SHIFT,
				      end_gfn << PAGE_SHIFT);
}

#define BUILD_PTE_RANGE_OP(name, op, op1)                                      \
	static int kvm_mips_##name##_pte(pte_t *pte, unsigned long start,      \
					 unsigned long end)                    \
	{                                                                      \
		int ret = 0;                                                   \
		int i_min = __pte_offset(start);                               \
		int i_max = __pte_offset(end);                                 \
		int i;                                                         \
		pte_t old, new;                                                \
                                                                               \
		for (i = i_min; i <= i_max; ++i) {                             \
			if (!pte_present(pte[i]))                              \
				continue;                                      \
                                                                               \
			old = pte[i];                                          \
			new = op(old);                                         \
			if (pte_val(new) == pte_val(old))                      \
				continue;                                      \
			set_pte(pte + i, new);                                 \
			ret = 1;                                               \
		}                                                              \
		return ret;                                                    \
	}                                                                      \
                                                                               \
	/* returns true if anything was done */                                \
	static int kvm_mips_##name##_pmd(pmd_t *pmd, unsigned long start,      \
					 unsigned long end)                    \
	{                                                                      \
		int ret = 0;                                                   \
		pte_t *pte;                                                    \
		unsigned long cur_end = ~0ul;                                  \
		int i_min = __pmd_offset(start);                               \
		int i_max = __pmd_offset(end);                                 \
		int i;                                                         \
		pmd_t old, new;                                                \
                                                                               \
		for (i = i_min; i <= i_max; ++i, start = 0) {                  \
			if (!pmd_present(pmd[i]))                              \
				continue;                                      \
                                                                               \
			if (pmd_huge(pmd[i])) {                                \
				old = pmd[i];                                  \
				new = op1(old);                                \
				if (pmd_val(new) == pmd_val(old))              \
					continue;                              \
				set_pmd(pmd + i, new);                         \
				continue;                                      \
			}                                                      \
                                                                               \
			pte = pte_offset(pmd + i, 0);                          \
			if (i == i_max)                                        \
				cur_end = end;                                 \
                                                                               \
			ret |= kvm_mips_##name##_pte(pte, start, cur_end);     \
		}                                                              \
		return ret;                                                    \
	}                                                                      \
                                                                               \
	static int kvm_mips_##name##_pud(pud_t *pud, unsigned long start,      \
					 unsigned long end)                    \
	{                                                                      \
		int ret = 0;                                                   \
		pmd_t *pmd;                                                    \
		unsigned long cur_end = ~0ul;                                  \
		int i_min = __pud_offset(start);                               \
		int i_max = __pud_offset(end);                                 \
		int i;                                                         \
                                                                               \
		for (i = i_min; i <= i_max; ++i, start = 0) {                  \
			if (!pud_present(pud[i]))                              \
				continue;                                      \
                                                                               \
			pmd = pmd_offset(pud + i, 0);                          \
			if (i == i_max)                                        \
				cur_end = end;                                 \
                                                                               \
			ret |= kvm_mips_##name##_pmd(pmd, start, cur_end);     \
		}                                                              \
		return ret;                                                    \
	}                                                                      \
                                                                               \
	static int kvm_mips_##name##_pgd(pgd_t *pgd, unsigned long start,      \
					 unsigned long end)                    \
	{                                                                      \
		int ret = 0;                                                   \
		pud_t *pud;                                                    \
		unsigned long cur_end = ~0ul;                                  \
		int i_min = pgd_index(start);                                  \
		int i_max = pgd_index(end);                                    \
		int i;                                                         \
                                                                               \
		for (i = i_min; i <= i_max; ++i, start = 0) {                  \
			if (!pgd_present(pgd[i]))                              \
				continue;                                      \
                                                                               \
			pud = pud_offset(pgd + i, 0);                          \
			if (i == i_max)                                        \
				cur_end = end;                                 \
                                                                               \
			ret |= kvm_mips_##name##_pud(pud, start, cur_end);     \
		}                                                              \
		return ret;                                                    \
	}

/*
 * kvm_mips_mkclean_gpa_pt.
 * Mark a range of guest physical address space clean (writes fault) in the VM's
 * GPA page table to allow dirty page tracking.
 */

BUILD_PTE_RANGE_OP(mkclean, pte_mkclean, pmd_mkclean)

/**
 * kvm_mips_mkclean_gpa_pt() - Make a range of guest physical addresses clean.
 * @kvm:	KVM pointer.
 * @start_gfn:	Guest frame number of first page in GPA range to flush.
 * @end_gfn:	Guest frame number of last page in GPA range to flush.
 *
 * Make a range of GPA mappings clean so that guest writes will fault and
 * trigger dirty page logging.
 *
 * The caller must hold the @kvm->mmu_lock spinlock.
 *
 * Returns:	Whether any GPA mappings were modified, which would require
 *		derived mappings (GVA page tables & TLB enties) to be
 *		invalidated.
 */
int kvm_mips_mkclean_gpa_pt(struct vz_vm *kvm, gfn_t start_gfn, gfn_t end_gfn)
{
	return kvm_mips_mkclean_pgd(kvm->gpa_mm.pgd, start_gfn << PAGE_SHIFT,
				    end_gfn << PAGE_SHIFT);
}

/*
 * kvm_mips_mkold_gpa_pt.
 * Mark a range of guest physical address space old (all accesses fault) in the
 * VM's GPA page table to allow detection of commonly used pages.
 */

BUILD_PTE_RANGE_OP(mkold, pte_mkold, pmd_mkold)

static int kvm_mips_mkold_gpa_pt(struct vz_vm *kvm, gfn_t start_gfn,
				 gfn_t end_gfn)
{
	return kvm_mips_mkold_pgd(kvm->gpa_mm.pgd, start_gfn << PAGE_SHIFT,
				  end_gfn << PAGE_SHIFT);
}

// TODO copy code from arch/mips/mm/tlbex.c:check_pabits
// x86 phys_limit : 80 00000000
static int get_pabit()
{
	return 1;
}

// TODO we will move this to memslots
#define ADDR_INVAL ((unsigned long)-1)
static unsigned long dune_hva_to_gpa(struct mm_struct *mm, unsigned long hva)
{
	uintptr_t mmap_start, stack_start;
	uintptr_t phys_end = (1ULL << get_pabit());
	uintptr_t gpa;

	BUG_ON(!mm);

	mmap_start = LG_ALIGN(mm->mmap_base) - GPA_MAP_SIZE;
	stack_start = LG_ALIGN(mm->start_stack) - GPA_STACK_SIZE;

	if (hva >= stack_start) {
		if (hva - stack_start >= GPA_STACK_SIZE)
			return ADDR_INVAL;
		gpa = hva - stack_start + phys_end - GPA_STACK_SIZE;
	} else if (hva >= mmap_start) {
		if (hva - mmap_start >= GPA_MAP_SIZE)
			return ADDR_INVAL;
		gpa = hva - mmap_start + phys_end - GPA_STACK_SIZE -
		      GPA_MAP_SIZE;
	} else {
		if (hva >= phys_end - GPA_STACK_SIZE - GPA_MAP_SIZE)
			return ADDR_INVAL;
		gpa = hva;
	}

	return gpa;
}

static unsigned long dune_gpa_to_hva(struct mm_struct *mm, unsigned long gpa)
{
	uintptr_t phys_end = (1ULL << get_pabit());

	if (gpa < phys_end - GPA_STACK_SIZE - GPA_MAP_SIZE)
		return gpa;
	else if (gpa < phys_end - GPA_STACK_SIZE)
		return gpa - (phys_end - GPA_STACK_SIZE - GPA_MAP_SIZE) +
		       LG_ALIGN(mm->mmap_base) - GPA_MAP_SIZE;
	else if (gpa < phys_end)
		return gpa - (phys_end - GPA_STACK_SIZE) +
		       LG_ALIGN(mm->start_stack) - GPA_STACK_SIZE;
	else
		return ADDR_INVAL;
}

static int
handle_hva_to_gpa(struct vz_vm *kvm, unsigned long start, unsigned long end,
		  int (*handler)(struct vz_vm *kvm, gfn_t gfn, gpa_t gfn_end,
				 struct kvm_memory_slot *memslot, void *data),
		  void *data)
{
	struct kvm_memslots *slots;
	struct kvm_memory_slot *memslot;
	int ret = 0;

	slots = kvm_memslots(kvm);

	/* we only care about the pages that the guest sees */
	kvm_for_each_memslot (memslot, slots) {
		unsigned long hva_start, hva_end;
		gfn_t gfn, gfn_end;

		hva_start = max(start, memslot->userspace_addr);
		hva_end = min(end, memslot->userspace_addr +
					   (memslot->npages << PAGE_SHIFT));
		if (hva_start >= hva_end)
			continue;

		/*
		 * {gfn(page) | page intersects with [hva_start, hva_end)} =
		 * {gfn_start, gfn_start+1, ..., gfn_end-1}.
		 */
		gfn = hva_to_gfn_memslot(hva_start, memslot);
		gfn_end = hva_to_gfn_memslot(hva_end + PAGE_SIZE - 1, memslot);

		ret |= handler(kvm, gfn, gfn_end, memslot, data);
	}
	return ret;
}

static int kvm_unmap_hva_handler(struct vz_vm *kvm, gfn_t gfn, gfn_t gfn_end,
				 struct kvm_memory_slot *memslot, void *data)
{
	kvm_mips_flush_gpa_pt(kvm, gfn, gfn_end - 1);
	return 1;
}

static void kvm_vz_flush_shadow_all(struct vz_vm *kvm)
{
	if (cpu_has_guestid) {
		/* Flush GuestID for each VCPU individually */
		kvm_flush_remote_tlbs(kvm);
	}
}

int kvm_unmap_hva_range(struct vz_vm *kvm, unsigned long start,
			unsigned long end)
{
	handle_hva_to_gpa(kvm, start, end, &kvm_unmap_hva_handler, NULL);

	kvm_vz_flush_shadow_all(kvm);
	return 0;
}

static int kvm_set_spte_handler(struct vz_vm *kvm, gfn_t gfn, gfn_t gfn_end,
				struct kvm_memory_slot *memslot, void *data)
{
	gpa_t gpa = gfn << PAGE_SHIFT;
	pte_t hva_pte = *(pte_t *)data;
	pte_t *gpa_pte = kvm_mips_pte_for_gpa(kvm, NULL, gpa);
	pte_t old_pte;

	if (!gpa_pte)
		return 0;

	/* Mapping may need adjusting depending on memslot flags */
	old_pte = *gpa_pte;

	set_pte(gpa_pte, hva_pte);

	/* Replacing an absent or old page doesn't need flushes */
	if (!pte_present(old_pte) || !pte_young(old_pte))
		return 0;

	/* Pages swapped, aged, moved, or cleaned require flushes */
	return !pte_present(hva_pte) || !pte_young(hva_pte) ||
	       pte_pfn(old_pte) != pte_pfn(hva_pte) ||
	       (pte_dirty(old_pte) && !pte_dirty(hva_pte));
}

void kvm_set_spte_hva(struct vz_vm *kvm, unsigned long hva, pte_t pte)
{
	unsigned long end = hva + PAGE_SIZE;
	int ret;

	ret = handle_hva_to_gpa(kvm, hva, end, &kvm_set_spte_handler, &pte);
	if (ret)
    kvm_vz_flush_shadow_all(kvm);
}

static int kvm_age_hva_handler(struct vz_vm *kvm, gfn_t gfn, gfn_t gfn_end,
			       struct kvm_memory_slot *memslot, void *data)
{
	return kvm_mips_mkold_gpa_pt(kvm, gfn, gfn_end);
}

static int kvm_test_age_hva_handler(struct vz_vm *kvm, gfn_t gfn, gfn_t gfn_end,
				    struct kvm_memory_slot *memslot, void *data)
{
	gpa_t gpa = gfn << PAGE_SHIFT;
	pte_t *gpa_pte = kvm_mips_pte_for_gpa(kvm, NULL, gpa);

	if (!gpa_pte)
		return 0;
	return pte_young(*gpa_pte);
}

int kvm_age_hva(struct vz_vm *kvm, unsigned long start, unsigned long end)
{
	return handle_hva_to_gpa(kvm, start, end, kvm_age_hva_handler, NULL);
}

int kvm_test_age_hva(struct vz_vm *kvm, unsigned long hva)
{
	return handle_hva_to_gpa(kvm, hva, hva, kvm_test_age_hva_handler, NULL);
}

static pud_t *kvm_mips_get_pud(struct vz_vm *kvm,
			       struct kvm_mmu_memory_cache *cache,
			       phys_addr_t addr)
{
	pgd_t *pgd;

	pgd = kvm->gpa_mm.pgd + pgd_index(addr);
	if (pgd_none(*pgd)) {
		/* Not used on MIPS yet */
		BUG();
		return NULL;
	}

	return pud_offset(pgd, addr);
}

static pmd_t *kvm_mips_get_pmd(struct vz_vm *kvm,
			       struct kvm_mmu_memory_cache *cache,
			       phys_addr_t addr)
{
	pud_t *pud;
	pmd_t *pmd;

	pud = kvm_mips_get_pud(kvm, cache, addr);
	if (!pud || pud_huge(*pud))
		return NULL;

	if (pud_none(*pud)) {
		if (!cache)
			return NULL;
		pmd = mmu_memory_cache_alloc(cache);
		pmd_init((unsigned long)pmd, (unsigned long)invalid_pte_table);
		pud_populate(NULL, pud, pmd);
	}

	return pmd_offset(pud, addr);
}

// TODO only syntax fixed, without tracking the details
int kvm_mips_set_pmd_huge(struct vz_vcpu *vcpu,
			  struct kvm_mmu_memory_cache *cache, phys_addr_t addr,
			  const pmd_t *new_pmd)
{
	pmd_t *pmd, old_pmd;

retry:
	pmd = kvm_mips_get_pmd(vcpu->kvm, cache, addr);
	VM_BUG_ON(!pmd);

	old_pmd = *pmd;
	/*
	 * Multiple vcpus faulting on the same PMD entry, can
	 * lead to them sequentially updating the PMD with the
	 * same value. Following the break-before-make
	 * (pmd_clear() followed by tlb_flush()) process can
	 * hinder forward progress due to refaults generated
	 * on missing translations.
	 *
	 * Skip updating the page table if the entry is
	 * unchanged.
	 */
	if (pmd_val(old_pmd) == pmd_val(*new_pmd))
		return 0;

	if (pmd_present(old_pmd)) {
		/*
		 * If we already have PTE level mapping for this block,
		 * we must unmap it to avoid inconsistent TLB state and
		 * leaking the table page. We could end up in this situation
		 * if the memory slot was marked for dirty logging and was
		 * reverted, leaving PTE level mappings for the pages accessed
		 * during the period. So, unmap the PTE level mapping for this
		 * block and retry, as we could have released the upper level
		 * table in the process.
		 *
		 * Normal THP split/merge follows mmu_notifier callbacks and do
		 * get handled accordingly.
		 */
		if (!pmd_huge(old_pmd)) {
			kvm_mips_flush_gpa_pt(vcpu->kvm,
					      (addr & PMD_MASK) >> PAGE_SHIFT,
					      ((addr & PMD_MASK) + PMD_SIZE -
					       1) >> PAGE_SHIFT);
			goto retry;
		}
		/*
		 * Mapping in huge pages should only happen through a
		 * fault.  If a page is merged into a transparent huge
		 * page, the individual subpages of that huge page
		 * should be unmapped through MMU notifiers before we
		 * get here.
		 *
		 * Merging of CompoundPages is not supported; they
		 * should become splitting first, unmapped, merged,
		 * and mapped back in on-demand.
		 */
		WARN_ON_ONCE(pmd_pfn(old_pmd) != pmd_pfn(*new_pmd));
		pmd_clear(pmd);
	}

	kvm_vz_host_tlb_inv(vcpu, addr & PMD_MASK);
	set_pmd(pmd, *new_pmd);
	return 0;
}

/*
 * Adjust pfn start boundary if support for transparent hugepage
 */
static bool transparent_hugepage_adjust(kvm_pfn_t *pfnp, unsigned long *gpap)
{
	kvm_pfn_t pfn = *pfnp;
	gfn_t gfn = *gpap >> PAGE_SHIFT;
	struct page *page = pfn_to_page(pfn);

	/*
	 * PageTransCompoundMap() returns true for THP and
	 * hugetlbfs. Make sure the adjustment is done only for THP
	 * pages.
	 */
	if ((!PageHuge(page)) && PageTransCompound(page) &&
	    (atomic_read(&page->_mapcount) < 0)) {
		unsigned long mask;
		/*
		 * The address we faulted on is backed by a transparent huge
		 * page.  However, because we map the compound huge page and
		 * not the individual tail page, we need to transfer the
		 * refcount to the head page.  We have to be careful that the
		 * THP doesn't start to split while we are adjusting the
		 * refcounts.
		 *
		 * We are sure this doesn't happen, because mmu_notifier_retry
		 * was successful and we are holding the mmu_lock, so if this
		 * THP is trying to split, it will be blocked in the mmu
		 * notifier before touching any of the pages, specifically
		 * before being able to call __split_huge_page_refcount().
		 *
		 * We can therefore safely transfer the refcount from PG_tail
		 * to PG_head and switch the pfn from a tail page to the head
		 * page accordingly.
		 */
		mask = PTRS_PER_PMD - 1;
		VM_BUG_ON((gfn & mask) != (pfn & mask));
		if (pfn & mask) {
			*gpap &= PMD_MASK;
			kvm_release_pfn_clean(pfn);
			pfn &= ~mask;
			kvm_get_pfn(pfn);
			*pfnp = pfn;
		}

		return true;
	}

	return false;
}

static bool fault_supports_huge_mapping(struct kvm_memory_slot *memslot,
					unsigned long hva,
					unsigned long map_size)
{
	gpa_t gpa_start;
	hva_t uaddr_start, uaddr_end;
	size_t size;

	size = memslot->npages * PAGE_SIZE;

	gpa_start = memslot->base_gfn << PAGE_SHIFT;

	uaddr_start = memslot->userspace_addr;
	uaddr_end = uaddr_start + size;

	/*
	 * Pages belonging to memslots that don't have the same alignment
	 * within a PMD/PUD for userspace and GPA cannot be mapped with stage-2
	 * PMD/PUD entries, because we'll end up mapping the wrong pages.
	 *
	 * Consider a layout like the following:
	 *
	 *    memslot->userspace_addr:
	 *    +-----+--------------------+--------------------+---+
	 *    |abcde|fgh  Stage-1 block  |    Stage-1 block tv|xyz|
	 *    +-----+--------------------+--------------------+---+
	 *
	 *    memslot->base_gfn << PAGE_SIZE:
	 *      +---+--------------------+--------------------+-----+
	 *      |abc|def  Stage-2 block  |    Stage-2 block   |tvxyz|
	 *      +---+--------------------+--------------------+-----+
	 *
	 * If we create those stage-2 blocks, we'll end up with this incorrect
	 * mapping:
	 *   d -> f
	 *   e -> g
	 *   f -> h
	 */
	if ((gpa_start & (map_size - 1)) != (uaddr_start & (map_size - 1)))
		return false;

	/*
	 * Next, let's make sure we're not trying to map anything not covered
	 * by the memslot. This means we have to prohibit block size mappings
	 * for the beginning and end of a non-block aligned and non-block sized
	 * memory slot (illustrated by the head and tail parts of the
	 * userspace view above containing pages 'abcde' and 'xyz',
	 * respectively).
	 *
	 * Note that it doesn't matter if we do the check using the
	 * userspace_addr or the base_gfn, as both are equally aligned (per
	 * the check above) and equally sized.
	 */
	return (hva & ~(map_size - 1)) >= uaddr_start &&
	       (hva & ~(map_size - 1)) + map_size <= uaddr_end;
}

bool kvm_is_reserved_pfn(kvm_pfn_t pfn)
{
	if (pfn_valid(pfn))
		return PageReserved(pfn_to_page(pfn));

	return true;
}

void kvm_set_pfn_accessed(kvm_pfn_t pfn)
{
	if (!kvm_is_reserved_pfn(pfn))
		mark_page_accessed(pfn_to_page(pfn));
}

void kvm_set_pfn_dirty(kvm_pfn_t pfn)
{
	if (!kvm_is_reserved_pfn(pfn)) {
		struct page *page = pfn_to_page(pfn);

		if (!PageReserved(page))
			SetPageDirty(page);
	}
}

// TODO in dune, ad bit isn't being tracking
/**
 * _kvm_mips_map_page_fast() - Fast path GPA fault handler.
 * @vcpu:		VCPU pointer.
 * @gpa:		Guest physical address of fault.
 * @write_fault:	Whether the fault was due to a write.
 * @out_entry:		New PTE for @gpa (written on success unless NULL).
 * @out_buddy:		New PTE for @gpa's buddy (written on success unless
 *			NULL).
 *
 * Perform fast path GPA fault handling, doing all that can be done without
 * calling into KVM. This handles marking old pages young (for idle page
 * tracking), and dirtying of clean pages (for dirty page logging).
 *
 * Returns:	0 on success, in which case we can update derived mappings and
 *		resume guest execution.
 *		-EFAULT on failure due to absent GPA mapping or write to
 *		read-only page, in which case KVM must be consulted.
 */
static int _kvm_mips_map_page_fast(struct vz_vcpu *vcpu, unsigned long gpa,
				   bool write_fault, pte_t *out_entry,
				   pte_t *out_buddy)
{
	struct vz_vm *kvm = vcpu->kvm;
	gfn_t gfn = gpa >> PAGE_SHIFT;
	pte_t *ptep;
	kvm_pfn_t pfn = 0; /* silence bogus GCC warning */
	bool pfn_valid = false;
	int ret = 0;

	spin_lock(&kvm->mmu_lock);

	/* Fast path - just check GPA page table for an existing entry */
	ptep = kvm_mips_pte_for_gpa(kvm, NULL, gpa);
	if (!ptep || !pte_present(*ptep)) {
		ret = -EFAULT;
		goto out;
	}

	/* Track access to pages marked old */
	if (!pte_young(*ptep)) {
		set_pte(ptep, pte_mkyoung(*ptep));
		pfn = pte_pfn(*ptep);
		pfn_valid = true;
		/* call kvm_set_pfn_accessed() after unlock */
	}

	if (write_fault && !pte_dirty(*ptep)) {
		if (!pte_write(*ptep)) {
			ret = -EFAULT;
			goto out;
		}

		/* Track dirtying of writeable pages */
		set_pte(ptep, pte_mkdirty(*ptep));
		pfn = pte_pfn(*ptep);
		if (pmd_huge(*((pmd_t *)ptep))) {
			int i;
			gfn_t base_gfn = (gpa & PMD_MASK) >> PAGE_SHIFT;
			for (i = 0; i < PTRS_PER_PTE; i++)
				mark_page_dirty(kvm, base_gfn + i);
		} else
			mark_page_dirty(kvm, gfn);
		kvm_set_pfn_dirty(pfn);
	}

	if (out_entry)
		*out_entry = *ptep;
	if (out_buddy)
		*out_buddy = *ptep_buddy(ptep);

out:
	spin_unlock(&kvm->mmu_lock);
	if (pfn_valid)
		kvm_set_pfn_accessed(pfn);
	return ret;
}


/**
 * kvm_mips_map_page() - Map a guest physical page.
 * @vcpu:		VCPU pointer.
 * @gpa:		Guest physical address of fault.
 * @write_fault:	Whether the fault was due to a write.
 * @out_entry:		New PTE for @gpa (written on success unless NULL).
 * @out_buddy:		New PTE for @gpa's buddy (written on success unless
 *			NULL).
 *
 * Handle GPA faults by creating a new GPA mapping (or updating an existing
 * one).
 *
 * This takes care of marking pages young or dirty (idle/dirty page tracking),
 * asking KVM for the corresponding PFN, and creating a mapping in the GPA page
 * tables. Derived mappings (GVA page tables and TLBs) must be handled by the
 * caller.
 *
 * Returns:	0 on success, in which case the caller may use the @out_entry
 *		and @out_buddy PTEs to update derived mappings and resume guest
 *		execution.
 *		-EFAULT if there is no memory region at @gpa or a write was
 *		attempted to a read-only memory region. This is usually handled
 *		as an MMIO access.
 */
static int kvm_mips_map_page(struct vz_vcpu *vcpu, unsigned long gpa,
			     bool write_fault, pte_t *out_entry,
			     pte_t *out_buddy)
{
	struct vz_vm *kvm = vcpu->kvm;
	struct kvm_mmu_memory_cache *memcache = &vcpu->mmu_page_cache;
	gfn_t gfn = gpa >> PAGE_SHIFT;
	int srcu_idx, err = 0;
	kvm_pfn_t pfn;
	pte_t *ptep;
	bool writeable;
	unsigned long prot_bits;
	unsigned long mmu_seq;
	u32 exccode = (vcpu->host_cp0_cause >> CAUSEB_EXCCODE) & 0x1f;

	unsigned long hva;
	struct kvm_memory_slot *memslot;
	bool force_pte = false;
	struct vm_area_struct *vma;
	unsigned long vma_pagesize;
	bool writable;
	int ret;

	/* Try the fast path to handle old / clean pages */
	srcu_idx = srcu_read_lock(&kvm->srcu);
	if ((exccode != EXCCODE_TLBRI) && (exccode != EXCCODE_TLBXI)) {
		err = _kvm_mips_map_page_fast(vcpu, gpa, write_fault, out_entry,
					      out_buddy);
		if (!err)
			goto out;
	}

	memslot = gfn_to_memslot(kvm, gfn);
	hva = gfn_to_hva_memslot_prot(memslot, gfn, &writable);
	if (kvm_is_error_hva(hva) || (write_fault && !writable))
		goto out;

	/* Let's check if we will get back a huge page backed by hugetlbfs */
	down_read(&current->mm->mmap_sem);
	vma = find_vma_intersection(current->mm, hva, hva + 1);
	if (unlikely(!vma)) {
		dune_err("Failed to find VMA for hva 0x%lx\n", hva);
		up_read(&current->mm->mmap_sem);
		err = -EFAULT;
		goto out;
	}

	vma_pagesize = vma_kernel_pagesize(vma);

	if (!fault_supports_huge_mapping(memslot, hva, vma_pagesize)) {
		force_pte = true;
		vma_pagesize = PAGE_SIZE;
	}

	/* PMD is not folded, adjust gfn to new boundary */
	if (vma_pagesize == PMD_SIZE)
		gfn = (gpa & huge_page_mask(hstate_vma(vma))) >> PAGE_SHIFT;

	up_read(&current->mm->mmap_sem);

	/* We need a minimum of cached pages ready for page table creation */
	err = mmu_topup_memory_cache(memcache, KVM_MMU_CACHE_MIN_PAGES,
				     KVM_NR_MEM_OBJS);
	if (err)
		goto out;

retry:
	/*
	 * Used to check for invalidations in progress, of the pfn that is
	 * returned by pfn_to_pfn_prot below.
	 */
	mmu_seq = kvm->mmu_notifier_seq;
	/*
	 * Ensure the read of mmu_notifier_seq isn't reordered with PTE reads in
	 * gfn_to_pfn_prot() (which calls get_user_pages()), so that we don't
	 * risk the page we get a reference to getting unmapped before we have a
	 * chance to grab the mmu_lock without mmu_notifier_retry() noticing.
	 *
	 * This smp_rmb() pairs with the effective smp_wmb() of the combination
	 * of the pte_unmap_unlock() after the PTE is zapped, and the
	 * spin_lock() in kvm_mmu_notifier_invalidate_<page|range_end>() before
	 * mmu_notifier_seq is incremented.
	 */
	smp_rmb();

	/* Slow path - ask KVM core whether we can access this GPA */
	pfn = gfn_to_pfn_prot(kvm, gfn, write_fault, &writeable);
	if (is_error_noslot_pfn(pfn)) {
		err = -EFAULT;
		goto out;
	}

	spin_lock(&kvm->mmu_lock);
	/* Check if an invalidation has taken place since we got pfn */
	if (mmu_notifier_retry(kvm, mmu_seq)) {
		/*
		 * This can happen when mappings are changed asynchronously, but
		 * also synchronously if a COW is triggered by
		 * gfn_to_pfn_prot().
		 */
		spin_unlock(&kvm->mmu_lock);
		kvm_release_pfn_clean(pfn);
		goto retry;
	}

	if (vma_pagesize == PAGE_SIZE && !force_pte) {
		/*
		 * Only PMD_SIZE transparent hugepages(THP) are
		 * currently supported. This code will need to be
		 * updated to support other THP sizes.
		 *
		 * Make sure the host VA and the guest IPA are sufficiently
		 * aligned and that the block is contained within the memslot.
		 */
		if (fault_supports_huge_mapping(memslot, hva, PMD_SIZE) &&
		    transparent_hugepage_adjust(&pfn, &gpa)) {
			vma_pagesize = PMD_SIZE;
		}
	}

	/* Set up the prot bits */
	prot_bits = _PAGE_PRESENT | __READABLE | _page_cachable_default;
	if (writeable) {
		prot_bits |= _PAGE_WRITE;
		if (write_fault) {
			prot_bits |= __WRITEABLE;
			kvm_set_pfn_dirty(pfn);
		}
	}

	if (vma_pagesize == PMD_SIZE) {
		pmd_t new_pmd = pfn_pmd(pfn, __pgprot(prot_bits));

		new_pmd = pmd_mkhuge(new_pmd);

		if (writeable && write_fault) {
			int i;
			gfn_t base_gfn = (gpa & PMD_MASK) >> PAGE_SHIFT;
			for (i = 0; i < PTRS_PER_PTE; i++)
				mark_page_dirty(kvm, base_gfn + i);
		}

		ret = kvm_mips_set_pmd_huge(vcpu, memcache, gpa, &new_pmd);
	} else {
		pte_t new_pte = pfn_pte(pfn, __pgprot(prot_bits));

		if (writeable && write_fault)
			mark_page_dirty(kvm, gfn);

		/* Ensure page tables are allocated */
		ptep = kvm_mips_pte_for_gpa(kvm, memcache, gpa);
		set_pte(ptep, new_pte);

		err = 0;
		if (out_entry)
			*out_entry = new_pte;
		if (out_buddy)
			*out_buddy = *ptep_buddy(&new_pte);
	}

	spin_unlock(&kvm->mmu_lock);
	kvm_release_pfn_clean(pfn);
	kvm_set_pfn_accessed(pfn);
out:
	srcu_read_unlock(&kvm->srcu, srcu_idx);
	return err;
}

int kvm_mips_handle_vz_root_tlb_fault(unsigned long badvaddr,
				      struct vz_vcpu *vcpu, bool write_fault)
{
	int ret;

	if (current_cpu_type() == CPU_LOONGSON3_COMP) {
		if (kvm_is_visible_gfn(vcpu->kvm, badvaddr >> PAGE_SHIFT) ==
		    0) {
			ret = RESUME_HOST;
			return ret;
		}
	}

	ret = kvm_mips_map_page(vcpu, badvaddr, write_fault, NULL, NULL);
	if (ret)
		return ret;

	/* Invalidate this entry in the TLB */
	return kvm_vz_host_tlb_inv(vcpu, badvaddr);
}

/* Restore ASID once we are scheduled back after preemption */
void dune_arch_vcpu_load(struct vz_vcpu *vcpu, int cpu)
{
	unsigned long flags;

	dune_debug("%s: vcpu %p, cpu: %d\n", __func__, vcpu, cpu);

	local_irq_save(flags);

	vcpu->cpu = cpu;
	if (vcpu->last_sched_cpu != cpu) {
		dune_debug("[%d->%d]KVM VCPU[%d] switch\n",
			   vcpu->last_sched_cpu, cpu, vcpu->vcpu_id);
	}

	/* restore guest state to registers */
	// kvm_mips_callbacks->vcpu_load(vcpu, cpu);

	local_irq_restore(flags);
}

// TODO
/* ASID can change if another task is scheduled during preemption */
void dune_arch_vcpu_put(struct vz_vcpu *vcpu)
{
	unsigned long flags;
	int cpu;

	local_irq_save(flags);

	cpu = smp_processor_id();
	vcpu->last_sched_cpu = cpu;
	vcpu->cpu = -1;

	// TODO
	/* save guest state in registers */
	// kvm_mips_callbacks->vcpu_put(vcpu, cpu);

	local_irq_restore(flags);
}
