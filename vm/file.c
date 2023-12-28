/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "stdio.h"
#include "stdlib.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#include "vm/file.h"
#include "stdint.h"
#include "string.h"


static bool file_backed_swap_in(struct page* page, void* kva);
static bool file_backed_swap_out(struct page* page);
static void file_backed_destroy(struct page* page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init(void)
{
	// printf("vm_file_init begin \n");

	// printf("vm_file_init end \n");
}

/* Initialize the file backed page */
bool
file_backed_initializer(struct page* page, enum vm_type type, void* kva) {
	// printf("\nfile_backed_initializer begin \n");

	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page* file_page = &page->file;
	struct load_segment_info* info = (struct load_segment_info*)page->uninit.aux;

	// memset(uninit, 0, sizeof(struct uninit_page));

	// printf("\n-------- aux(get) info --------- \n");
	// printf("file : %p\n", info->file);
	// printf("off_t : %d\n", info->ofs);
	// printf("upage : %p\n", info->upage);
	// printf("read_bytes : %d\n", info->read_bytes);
	// printf("zero_bytes : %d\n", info->zero_bytes);
	// printf("writable : %d\n", info->writable);

	file_page->file = info->file;
	file_page->offset = info->ofs;
	file_page->read_bytes = info->read_bytes;
	file_page->zero_bytes = info->zero_bytes;

	// printf("file_backed_initializer end \n");

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page* page, void* kva)
{
	// printf("file_backed_swap_in begin ! \n");

	struct file_page* file_page UNUSED = &page->file;

	struct load_segment_info* info = (struct load_segment_info*)page->uninit.aux;

	struct file* file = file_page->file;
	off_t ofs = file_page->offset;
	uint32_t read_bytes = file_page->read_bytes;
	uint32_t zero_bytes = file_page->zero_bytes;

	if (file_read_at(file, kva, read_bytes, ofs) != (int)read_bytes)
	{
		printf("file_read_at failed! \n");
		return false;
	}
	memset(kva + read_bytes, 0, zero_bytes);
	// printf("file_backed_swap_in end ! \n");

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page* page)
{
	// printf("file_backed_swap_out begin ! \n");

	struct file_page* file_page UNUSED = &page->file;
	struct thread* curr = thread_current();
	struct file* file = file_page->file;
	off_t ofs = file_page->offset;
	uint32_t read_bytes = file_page->read_bytes;
	// printf("file : %p \n", file);
	// printf("ofs : %d \n", ofs);
	// printf("read_bytes : %d \n", read_bytes);

	if (pml4_is_dirty(curr->pml4, page->va))
	{
		file_write_at(file, page->frame->kva, read_bytes, ofs);
		pml4_set_dirty(curr->pml4, page->va, false);
	}
	pml4_clear_page(curr->pml4, page->va);

	// printf("file_backed_swap_out end ! \n");
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page* page) {
	struct file_page* file_page UNUSED = &page->file;
	struct thread* curr = thread_current();

	// printf("file_backed_destroy begin ! \n");

	// printf("file_backed_swap_out page va : %p \n", page->va);
	if (pml4_is_dirty(curr->pml4, page->va))
	{
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->offset);
		pml4_set_dirty(curr->pml4, page->va, 0);
	}
	pml4_clear_page(curr->pml4, page->va);

	// printf("file_backed_destroy end ! \n");

}


/* Do the mmap */
void* do_mmap(void* addr, size_t length, int writable, struct file* file, off_t offset)
{
	struct supplemental_page_table* spt = &thread_current()->spt;
	struct file* re_file = file_reopen(file);
	void* orgin_addr = addr;
	size_t aligned_length = ((length + (PGSIZE - 1)) / PGSIZE) * PGSIZE;
	int mmap_cnt = aligned_length / PGSIZE;
	size_t read_bytes = file_length(file) > length ? length : file_length(file);
	size_t zero_bytes = aligned_length - read_bytes;

	while (read_bytes > 0 || zero_bytes > 0)
	{
		size_t page_read_bytes = PGSIZE > read_bytes ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct load_segment_info* info = (struct load_segment_info*)malloc(sizeof(struct load_segment_info));
		if (info == NULL)
		{
			printf("malloc failed for load_segment_info\n");
			return false;
		}

		info->file = re_file;
		info->ofs = offset;
		info->upage = addr;
		info->read_bytes = page_read_bytes;
		info->zero_bytes = page_zero_bytes;
		info->writable = writable;

		// printf("\n-------- info(sending) info --------- \n");
		// printf("file : %p\n", info->file);
		// printf("off_t : %d\n", info->ofs);
		// printf("upage : %p\n", info->upage);
		// printf("read_bytes : %d\n", info->read_bytes);
		// printf("zero_bytes : %d\n", info->zero_bytes);
		// printf("writable : %d\n", info->writable);


		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, info))
		{
			free(info);
			return NULL;
		}


		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;

	}

	struct page* page = spt_find_page(&thread_current()->spt, orgin_addr);
	if (page == NULL)
	{
		printf("page shouln't be null at this point \n");
	}
	list_push_front(&spt->mapped_list, &page->mapp_elem);
	page->mmap_cnt = mmap_cnt;
	// printf("mmap_cnt : %d \n", mmap_cnt);


	return orgin_addr;
}


/* Do the munmap */
void
do_munmap(void* addr)
{
	// printf("--- munmap begin ----\n");

	struct thread* curr = thread_current();
	struct page* page = spt_find_page(&curr->spt, addr);
	int cnt = page->mmap_cnt;

	for (size_t i = 0; i < cnt; i++)
	{
		// struct load_segment_info* info = (struct load_segment_info*)page->uninit.aux;

		// printf("page va :  %p\n", page->va);
		// printf("offset :  %d\n", info->ofs);
		// printf("read_bytes :  %d\n", info->read_bytes);
		// printf("zero_bytes :  %d\n", info->zero_bytes);
		// printf("pml4_is_dirty : %d \n", pml4_is_dirty(curr->pml4, page->va));
		if (page)
		{
			list_remove(&page->page_elem);
			// spt_remove_page(&curr->spt, page);

			destroy(page);

		}
		addr += PGSIZE;
		page = spt_find_page(&curr->spt, addr);

		// if (pml4_is_dirty(curr->pml4, page->va))
		// {
		// 	if (file_write_at(info->file, page->va, info->read_bytes, info->ofs) != info->read_bytes)
		// 	{
		// 		printf("file_write_at is failed!! \n");
		// 		return;
		// 	}
		// 	pml4_set_dirty(curr->pml4, page->va, 0);
		// }
		// printf("spt_remove_page begin \n");
		// list_remove(&page->page_elem);
		// printf("spt_remove_page end \n");
		// pml4_clear_page(curr->pml4, addr);
		// printf("pml4_clear_page end\n");

		// printf("addr : %p \n", addr);

	}
	// printf("--- munmap end ----\n");


}
