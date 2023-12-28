/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "stdio.h"
#include "stdlib.h"
#include "threads/mmu.h"
#include "stddef.h"
#include "string.h"
#include "bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk* swap_disk;
static bool anon_swap_in(struct page* page, void* kva);
static bool anon_swap_out(struct page* page);
static void anon_destroy(struct page* page);

static struct bitmap* swap_table;
static int bitcnt;
#define SECTOCR_IN_PAGE 8


/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops =
{
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init(void)
{
	// printf("\n--- vm_anon_init begin \n");

	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	if (swap_disk == NULL)
	{
		printf("swap_disk is NULL!! \n");
		return;
	}
	// printf("\n---- disk info ---\n");
	// printf("swap_disk addr : %p \n", swap_disk);
	// printf("swap_disk size : %d \n", disk_size(swap_disk));

	bitcnt = disk_size(swap_disk) / SECTOCR_IN_PAGE;
	// printf("bitcnt : %d \n", bitcnt);

	swap_table = bitmap_create(bitcnt);
	if (swap_table == NULL)
	{
		printf("swap_table creation failed! \n");
		return;
	}
	// printf("swap_table : %p \n", swap_table);

	// printf("\n--- vm_anon_init end \n");

}

/* Initialize the file mapping */
bool
anon_initializer(struct page* page, enum vm_type type, void* kva)
{
	// printf("\n--- anon_initializer begin \n");

	// printf("---  anon_page info ----\n");
	// printf("anon_page va : %p : \n", page->va);
	// printf("type : %d : \n", type);
	// printf("kva : %p : \n", kva);


	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page* anon_page = &page->anon;
	anon_page->swap_offset = -1;

	// printf("\n--- anon_initializer end \n");
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page* page, void* kva)
{
	// printf("\n--- anon_swap_in begin \n");
	// printf("page : %p\n", page->va);
	// printf("kva : %p\n", kva);
	// printf("page present : %d\n", page->present_bit);

	if (page == NULL)
	{
		printf("page is NULL! failed \n");
		return false;
	}

	struct anon_page* anon_page = &page->anon;
	// if (page->present_bit == PAGE_IS_IN_MEMORY)
	// {
	// 	printf("page is alreay in Memory!\n Don't need to swap in! \n");
	// 	return false;
	// }

	off_t swap_sec = anon_page->swap_offset;
	if (swap_sec == -1)
	{
		// printf("swap_sec is -1, already in MEMORY!! \n");
		return true;
	}


	int free_slot = swap_sec / SECTOCR_IN_PAGE;

	// printf("swap_sec : %d \n", swap_sec);
	// printf("free_slot : %d \n", free_slot);

	for (int sec = 0; sec < SECTOCR_IN_PAGE; sec++)
	{
		disk_read(swap_disk, swap_sec + sec, kva + DISK_SECTOR_SIZE * sec);
	}

	pml4_set_page(thread_current()->pml4, page->va, kva, true);
	bitmap_set(swap_table, free_slot, false);
	// page->present_bit = PAGE_IS_IN_MEMORY;
	// anon_page->swap_offset = -1;

	// printf("\n--- anon_swap_in end \n");

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page* page)
{
	// printf("\n--- anon_swap_out begin---\n");

	struct anon_page* anon_page = &page->anon;
	size_t free_slot;
	off_t swap_sec;

	// if (page->present_bit == PAGE_IS_OUT_OF_MEMORY)
	// {
	// 	printf("page is alreay SWAPED out!\n Don't need to swap out! \n");
	// 	return false;
	// }

	// printf("current bitmap_size is %d \n", bitmap_size(swap_table));

	free_slot = bitmap_scan_and_flip(swap_table, 0, 1, false);
	if (free_slot == BITMAP_ERROR)
	{
		PANIC("free_slot is fulled!! \n");
	}

	swap_sec = free_slot * SECTOCR_IN_PAGE;
	// printf("current free_slot is %d \n", free_slot);
	// printf("current swap_sec is %d \n", swap_sec);
	// printf("page->frame->kva is %p \n", page->frame->kva);

	for (int sec = 0; sec < SECTOCR_IN_PAGE; sec++)
	{
		// printf("disk_write begin, sector : %d, kav : %p \n", swap_sec + sec, page->frame->kva + DISK_SECTOR_SIZE * sec);
		disk_write(swap_disk, swap_sec + sec, page->frame->kva + DISK_SECTOR_SIZE * sec);
	}

	pml4_clear_page(thread_current()->pml4, page->va);

	// printf("page frame is cleared! \n");

	anon_page->swap_offset = swap_sec;
	// page->present_bit = PAGE_IS_OUT_OF_MEMORY;
	page->frame = NULL;

	// printf("\n--- anon_swap_out end \n");

	return true;

}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page* page)
{
	// struct thread* curr = thread_current();
	struct anon_page* anon_page = &page->anon;
	// if (page->operations->type == VM_ANON)
	// {
	// 	free(page);
	// }

	// palloc_free_page(page->frame->kva);
	// free(page->frame);
	// pml4_clear_page(curr->pml4, page->va);
}
