/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "stdio.h"
#include "threads/mmu.h"
#include "vm/uninit.h"
#include "threads/thread.h"
#include "string.h"
#include "userprog/process.h"

/* Frame List */
struct list frame_list;


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init(void) {
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_list);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
	page_get_type(struct page* page) {
	int ty = VM_TYPE(page->operations->type);
	switch (ty) {
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame* vm_get_victim(void);
static bool vm_do_claim_page(struct page* page);
static struct frame* vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `_page`. */
bool
vm_alloc_page_with_initializer(enum vm_type type, void* upage, bool writable, vm_initializer* init, void* aux)
{
	// printf("------ vm_alloc_page_with_initializer begin -----\n");
	ASSERT(VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table* spt = &thread_current()->spt;
	struct page* page;
	// printf("spt->page_list: %p \n", &spt->page_list);

	// printf("vm_alloc_page_with_initializer, type : %d, upage : %p \n", type, upage);
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		page = (struct page*)malloc(sizeof(struct page));
		if (page == NULL)
		{
			return false;
		}

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			// printf("VM_ANON begin \n");
			uninit_new(page, upage, init, type, aux, anon_initializer);
			break;

		case VM_FILE:
			// printf("VM_FILE begin \n");
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
			break;
		}

		/* TODO: Insert the page into the spt. */
		page->writable = writable;
		spt_insert_page(spt, page);

		// printf("------ vm_alloc_page_with_initializer end -----\n");

		return true;
	}
err:
	printf("spt_find_page isn't NULL!\n");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page*
	spt_find_page(struct supplemental_page_table* spt UNUSED, void* va UNUSED) {
	struct page* page = NULL;
	/* TODO: Fill this function. */
	/* Project 3
	1. Va가 유효한지 검사한다. (일단 넘어가자)
	2. page_list를 순회하면서, va에 매칭되는 page를 찾아 리턴한다.
	3. 존재하지 않다면, NULL을 리턴
	*/

	if (!is_user_vaddr(va))
	{
		// printf("spt_find_page, not vaild addr. Failed \n");
		return NULL;
	}

	void* vaddr = pg_round_down(va);
	// printf("spt_find_page va : %p \n", vaddr);

	struct list_elem* e;
	for (e = list_begin(&spt->page_list); e != list_end(&spt->page_list); e = list_next(e))
	{
		struct page* curr_page = list_entry(e, struct page, page_elem);
		if (curr_page->va == vaddr)
		{
			page = curr_page;
			// printf("page -> va : %p \n", page->va);
			return page;
		}
	}
	// printf("spt_find_page, page is not founded \n");
	return NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page(struct supplemental_page_table* spt UNUSED, struct page* page)
{
	// printf("---- spt_insert_page begin ----- \nspt->page_list : %p  page->va : %p \n", &spt->page_list, page->va);
	int succ = false;
	if ((spt_find_page(spt, page->va)) != NULL)
	{
		// printf("spt_find_page(page) != NULL, insert Failed \n");
		return succ;
	}
	list_push_front(&spt->page_list, &page->page_elem);
	succ = true;
	// printf("insert page->frame : %p \n", page->frame);
	// printf("----- spt_insert_page : %d , end ------\n", succ);

	return succ;
}

void
spt_remove_page(struct supplemental_page_table* spt, struct page* page) {
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame*
vm_get_victim(void) {

	// printf("vm_get_victim begin \n");
	struct frame* victim = NULL;
	struct supplemental_page_table* spt = &thread_current()->spt;

	struct list_elem* e;
	for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e))
	{
		victim = list_entry(e, struct frame, frame_elem);
		if (pml4_is_accessed(thread_current()->pml4, victim->page->va))
		{
			pml4_set_accessed(thread_current()->pml4, victim->page->va, 0);
		}
		else
		{
			// printf("found victimx : %p \n", victim->page->va);
			return victim;
		}
	}


	/* TODO: The policy for eviction is up to you. */
	// if (!list_empty(&spt->frame_list))
	// {
	// 	victim = list_entry(list_pop_front(&spt->frame_list), struct frame, frame_elem);
	// 	printf("vm_get_victim found! \n");
	// 	return victim;
	// }

	// printf("vm_get_victim not found!! \n");

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame*
vm_evict_frame(void) {

	struct supplemental_page_table* spt = &thread_current()->spt;
	struct frame* victim UNUSED = vm_get_victim();
	if (victim == NULL)
	{
		printf("victim is NULL! \n");
		return NULL;
	}
	/* TODO: swap out the victim and return the evicted frame. */
	if (!swap_out(victim->page))
	{
		printf("swap_out failed! \n");
		return NULL;
	}
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame*
vm_get_frame(void) {
	// printf("vm_get_frame begin \n");

	struct frame* frame = NULL;
	struct supplemental_page_table* spt = &thread_current()->spt;

	frame = (struct frame*)malloc(sizeof(struct frame));
	if (frame == NULL)
	{
		printf("malloc is failed \n");
		return NULL;
	}

	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	if (frame->kva == NULL)
	{
		// printf("USER POOL is not available!! \n");
		// TODO: Implement eviction mechanism
		frame = vm_evict_frame();
		if (frame == NULL)
		{
			printf("vm_evict_frame is failed! \n");
			return NULL;
		}
		frame->page = NULL;

		return frame;
	}
	list_push_front(&frame_list, &frame->frame_elem);
	frame->page = NULL;


	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);


	return frame;
}


/* Growing the stack. */
static void
vm_stack_growth(void* addr UNUSED)
{
	struct thread* curr = thread_current();

	void* stack_bottom = pg_round_down(addr);
	// printf("curr stack_bottom : %p \n", stack_bottom);
	if (spt_find_page(&curr->spt, stack_bottom))
	{
		// printf("stack is already exist, Don't need to grow! \n");
		return;
	}

	if (!vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom, true))
	{
		printf("setup_stack - vm_alloc_page failed \n");
		return;
	}

	curr->user_rsp = stack_bottom;
	// printf("curr curr->user_rsp : %p \n", curr->user_rsp);

	// if (!vm_claim_page(stack_bottom))
	// {
	// 	printf("setup_stack - vm_claim_page failed \n");
	// 	return;
	// }
	// exit(-1);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page* page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault(struct intr_frame* f UNUSED, void* addr UNUSED,
	bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{

	// printf("\n---- vm_try_handle_fault begin--- \n");
	// printf("\n---- page fault info --- \n");
	// printf("page fault addr : %p \n", addr);
	// printf("page fault write : %d \n", write);
	// printf("page fault not_present : %d \n", not_present);
	// printf("page fault user mode : %d \n", user);

	struct supplemental_page_table* spt UNUSED = &thread_current()->spt;
	struct page* page = NULL;
	struct thread* curr = thread_current();
	int8_t HEURISTICS = 8;

	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (!is_user_vaddr(addr) || addr == NULL)
	{
		// printf("invailed addr!! \n");
		return false;
	}

	if (!not_present)
	{
		// printf("not_present fail!! \n");
		return false;
	}

	void* stack_bottom = user ? f->rsp : curr->user_rsp;
	if (addr >= USER_STACK_LIMIT && addr <= USER_STACK)
	{
		if (addr >= stack_bottom - HEURISTICS)
		{
			printf("addr : %p, stacked need to grow!! \n", addr);
			vm_stack_growth(addr);
		}
	}

	page = spt_find_page(spt, addr);
	if (page == NULL)
	{
		// printf("vm_try_handle_fault, page is not exist \n");
		return false;
	}
	// printf("vm_try_handle_fault - found page -> va : %p \n", page->va);


	if (write == true && !page->writable)
	{
		// printf("Don't allowed page writing!!!\n");
		return false;
	}

	// printf("---- vm_try_handle_fault end--- \n");
	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page(struct page* page) {
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page(void* va UNUSED) {

	/* Project 3
	1. va가 유효한 지 체크 (일단 패스)
	2. va가 spt 테이블에 이미 존재하는 지 체크
	3. (1), (2)도 아니라면, 으로 리턴 vm_do_claim_page(page)
	*/
	// printf("vm_claim_page begin va : %p \n", va);

	struct page* page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
	{
		printf("vm_claim_page page is not founded\n");
		return false;
	}

	// printf("vm_claim_page end\n");
	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page* page)
{
	// printf("--- vm_do_claim_page begin --- \n");
	struct frame* frame = vm_get_frame();
	ASSERT(frame != NULL);
	// printf("frame got successfullly \n");

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread* curr = thread_current();
	if (!pml4_set_page(curr->pml4, page->va, frame->kva, page->writable))
	{
		printf("pml4_set_page is failed! \n");
		return false;
	}

	// printf("page->va : %p \n", page->va);
	// printf("frame->kva : %p \n", frame->kva);
	// printf("pml4_get_page : %p \n", pml4_get_page(curr->pml4, page->va));

	if (!swap_in(page, frame->kva))
	{
		printf("swap_in is failed! \n");
		return false;
	}

	// printf("---- vm_do_claim_page end---- \n");
	return  true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init(struct supplemental_page_table* spt UNUSED)
{
	/* Project 3 */
	// printf("supplemental_page_table_init begin \n");
	list_init(&spt->page_list);
	list_init(&spt->mapped_list);
	// printf("supplemental_page_table_init end\n");

}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy(struct supplemental_page_table* dst UNUSED,
	struct supplemental_page_table* src UNUSED)
{
	/*
	목표 : src -> dst로 복사하는 것

	TOOD
	1. src에 있는 리스트를 순회하면서 모든 페이지 구조체에 대해서 작업을 수행한다.
		for (list end 까지)
			- src의 페이지 구조체를 dst에 삽입한다.
			- dst에 삽입된 페이지 구조체에 대해서 Claim 루틴을 수행한다.
	*/
	// printf("supplemental_page_table_copy begin\n");


	struct thread* curr = thread_current();
	struct list* src_list = &src->page_list;
	// struct list* dst_list = &dst->page_list;

	struct list_elem* e;
	for (e = list_begin(src_list); e != list_end(src_list); e = list_next(e))
	{
		struct page* curr_page = list_entry(e, struct page, page_elem);
		if (curr_page != NULL)
		{
			// printf("curr_page type - %d \n", curr_page->operations->type);
			if (curr_page->operations->type == VM_UNINIT)
			{
				if (!vm_alloc_page_with_initializer(curr_page->uninit.type, curr_page->va, curr_page->writable, curr_page->uninit.init, curr_page->uninit.aux))
				{
					printf("vm_alloc_page_with_initializer failed");
				}
				continue;
			}
			else
			{
				struct page* dup_page = (struct page*)malloc(sizeof(struct page));
				if (dup_page == NULL)
				{
					printf("malloc failed for dup_page\n");
					return false;
				}

				*dup_page = *curr_page;
				// printf("---- dup_page info ----- \n");
				// printf("dup_page va : %p\n", dup_page->va);
				// printf("dup_page frame -> kva: %p\n", dup_page->frame->kva);


				if (!spt_insert_page(dst, dup_page))
				{
					printf("spt_insert_page failed for dup_page\n");
					free(dup_page);
					return false;
				}

				// printf("spt_insert_page\n");

				if (!vm_do_claim_page(dup_page))
				{
					printf("vm_claim_page failed for dup_page\n");
					return false;
				}

				memcpy(dup_page->frame->kva, curr_page->frame->kva, PGSIZE);
			}
		}
	}
	// printf("supplemental_page_table_copy end\n");

	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill(struct supplemental_page_table* spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */

	 /*
	 개요 : 리스트의 모든 페이지 구조체를 삭제한다. (카피랑 비슷하게 하면 될 듯)
	 TODO :
	 1. 리스트를 돈다.
	 2. page를 얻는다.
	 3. destroy 함수를 호출한다.
	 */
	struct thread* curr = thread_current();
	struct list_elem* e;
	for (e = list_begin(&spt->page_list); e != list_end(&spt->page_list); e = list_next(e))
	{
		struct page* curr_page = list_entry(e, struct page, page_elem);
		if (curr_page != NULL)
		{
			list_remove(&curr_page->page_elem);
			destroy(curr_page);
			// list_remove(&curr_page->frame->frame_elem);
		}
	}


}

