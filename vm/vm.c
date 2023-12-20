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
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer(enum vm_type type, void* upage, bool writable, vm_initializer* init, void* aux)
{
	// printf("\n------ vm_alloc_page_with_initializer begin -----\n");
	ASSERT(VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table* spt = &thread_current()->spt;
	// printf("spt->page_list: %p \n", &spt->page_list);

	// printf("vm_alloc_page_with_initializer, type : %d, upage : %p \n", type, upage);
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page* page = (struct page*)malloc(sizeof(struct page));
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

	void *vaddr = pg_round_down(va);
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
	struct frame* victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame*
vm_evict_frame(void) {
	struct frame* victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame*
vm_get_frame(void) {
	struct frame* frame = NULL;
	/* TODO: Fill this function. */
	/* Procject 3
	1. Userpool이 꽉 찾는 지 확인
	2. 꽉 찾다면, evict 메커니즘 실행 (일단 오류 처리하고 넘어간다.)
	3. 아니라면, palloc_get_page()으로 frame을 얻는다.
	*/

	frame = (struct frame*)malloc(sizeof(struct frame));
	if (frame == NULL)
	{
		printf("malloc is failed \n");
		return NULL;
	}

	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	frame->page = NULL;
	if (frame->kva == NULL)
	{
		printf("USER POOL is not available \n");
		// TODO: Implement eviction mechanism
		free(frame);
		return NULL;
	}

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}


/* Growing the stack. */
static void
vm_stack_growth(void* addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page* page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault(struct intr_frame* f UNUSED, void* addr UNUSED,
	bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table* spt UNUSED = &thread_current()->spt;
	struct page* page = NULL;



	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// printf("\n---- vm_try_handle_fault begin--- \n");

	// printf("\n---- page fault info --- \n");
	// printf("page fault addr : %p \n", addr);
	// printf("page fault write : %d \n", write);
	// printf("page fault not_present : %d \n", not_present);

	if (!is_user_vaddr(addr))
	{
		return false;
	}

	page = spt_find_page(spt, addr);
	// printf("vm_try_handle_fault - found page -> va : %p \n", page->va);
	if (page == NULL)
	{
		// printf("vm_try_handle_fault, page is not exist \n");
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
		// printf("vm_claim_page page is not founded\n");
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

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread* curr = thread_current();
	if (!pml4_set_page(curr->pml4, page->va, frame->kva, page->writable))
	{
		return false;
	}

	// printf("page->va : %p \n", page->va);
	// printf("frame->kva : %p \n", frame->kva);
	// printf("pml4_get_page : %p \n", pml4_get_page(curr->pml4, page->va));

	// printf("---- vm_do_claim_page end---- \n");
	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init(struct supplemental_page_table* spt UNUSED)
{
	/* Project 3 */
	// printf("supplemental_page_table_init begin \n");
	list_init(&spt->page_list);
	// printf("supplemental_page_table_init end\n");

}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy(struct supplemental_page_table* dst UNUSED,
	struct supplemental_page_table* src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill(struct supplemental_page_table* spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
