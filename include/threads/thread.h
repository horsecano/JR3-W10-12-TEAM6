#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

enum proccess_status
{
	PROCESS_RUNNING,
	PROCESS_END
};



/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
 /* The `elem' member has a dual purpose.  It can be an element in
  * the run queue (thread.c), or it can be an element in a
  * semaphore wait list (synch.c).  It can be used these two ways
  * only because they are mutually exclusive: only a thread in the
  * ready state is on the run queue, whereas only a thread in the
  * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int priority;			   /* Priority. */

	/* Shared between thread.c and synch.c. */
	struct list_elem elem; /* List element. */

	/* Store local tick */
	int64_t wakeup_tick;

	/* Priority Donation */
	int origin_priority;
	struct lock* wait_on_lock;
	struct list donations;
	struct list_elem d_elem;

	/* Advanced Prority */
	int nice;
	int recent_cpu;

	/* User program */
	/*
	TODO
	1. 자식 프로세스가 성공적으로 생성되었는지를 나타내는 플래그 추가 (실행 파일 로드에 실패하면-1)
	2. 프로세스의 종료 유무 필드 추가
	3. 정상적으로 종료가 되었는지를 status 확인하는 필드 추가
	4. 프로세스의 대기를 위한 세마포어에 대한 필드 추가
	5. 자식 프로세스 리스트의 대한 필드 추가
	6. 부모 프로세스 디스크립터 포인터 필드 추가
	*/
	struct semaphore sema_wait; // process_wait을 위한 세마포어
	struct semaphore sema_exit; // process_exit을 위한 세마포어
	struct semaphore sema_fork; // process_exit을 위한 세마포어

	struct thread* parent;		 // 부모 쓰레드 포인터
	struct list child_list;		 // 자식 프로세스 리스트의 대한 필드
	struct list_elem child_elem; // 자식 프로세스 리스트에 대한 원소
	int return_status;
	struct intr_frame parent_if;
	bool exited; // 프로세스의 종료 유무
	bool waited; // 부모 쓰레드가 wait 중인지의 여부

	/* File Discriptor */
	struct file* fdt[64];
	// struct file **fdt;

	int next_fd;
	struct file* running_file;

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t* pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
	void* user_rsp;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void* aux);
tid_t thread_create(const char* name, int priority, thread_func*, void*);

void thread_block(void);
void thread_unblock(struct thread*);

struct thread* thread_current(void);
tid_t thread_tid(void);
const char* thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame* tf);
/* Alarm Clock */
void thread_sleep(int64_t ticks);
void thread_wakeup(int64_t global_tick);

/* Priority Scheduling */
bool cmp_priority(struct list_elem* a, struct list_elem* b, void* aux UNUSED);
void test_max_priority(void);

/* Priority Donation */
void donate_priority(void);

/// @brief
/// @param lock
void remove_with_lock(struct lock* lock);
void refresh_priority(void);

/* Advanced Scheduler */
void mlfqs_prioirty(struct thread* t);
void mlfqs_recent_cpu(struct thread* t);
void mlfqs_load_avg(void);
void mlfqs_increment(void);
void mlfqs_recalc(void);

#endif /* threads/thread.h */
