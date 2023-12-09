#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "user/syscall.h"
#include "filesys/file.h"
#include "devices/input.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

int check_address(void *addr);

void halt(void) NO_RETURN;
void exit(int status) NO_RETURN;
pid_t fork(const char *thread_name);
int exec(const char *file);
int wait(pid_t);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned length);
int write(int fd, const void *buffer, unsigned length);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	/* File Discriptor */
	// lock_init(&filesys_lock);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	switch (f->R.rax)
	{
	case SYS_HALT:
		halt();
		break;

	case SYS_EXIT:
		exit(f->R.rdi);
		break;

	case SYS_FORK:
		break;

	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;

	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;

	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;

	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;

	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;

	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;

	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;

	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;

	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;

	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;

	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	}
}

/* System Call help Function */
int check_address(void *addr)
{
	struct thread *curr = thread_current();
	if (is_kernel_vaddr(addr) || pml4_get_page(curr->pml4, addr) == NULL)
	{
		exit(-1);
	}
}

/* System Call Begin */
void halt(void)
{
	power_off();
}

void exit(int status)
{
	struct thread *curr = thread_current();
	curr->return_status = status;
	printf("%s: exit(%d)\n", curr->name, status);
	thread_exit();
}

int open(const char *file)
{
	check_address(file);
	if (file == NULL)
	{
		return -1;
	}

	struct file *curr_file = filesys_open(file);
	if (curr_file == NULL)
	{
		return -1;
	}

	int fd = process_add_file(curr_file);
	return fd;
}

int filesize(int fd)
{
	struct file *curr_file = process_get_file(fd);
	if (curr_file == NULL)
	{
		return -1;
	}

	return file_length(curr_file);
}

int read(int fd, void *buffer, unsigned length)
{
	check_address(buffer);
	struct thread *curr = thread_current();
	off_t read_bite;
	if (fd >= curr->next_fd)
	{
		exit(-1);
	}

	struct file *curr_file = process_get_file(fd);
	if (curr_file == NULL)
	{
		return -1;
	}

	if (fd == 0)
	{
		int i;
		unsigned char *buf = buffer;
		for (i = 0; i < length; i++)
		{
			char c = input_getc();
			*buf = c;
			buf++;
			if (c == '\0')
			{
				break;
			}
		}
		return i;
	}
	else if (fd == 1)
	{
		return -1;
	}
	else
	{
		read_bite = file_read(curr_file, buffer, length);
	}
	return read_bite;
}

int write(int fd, const void *buffer, unsigned length)
{
	check_address(buffer);
	struct thread *curr = thread_current();
	int write_byte;
	if (fd >= curr->next_fd)
	{
		exit(-1);
	}

	if (fd == 0)
	{
		return -1;
	}
	else if (fd == 1)
	{
		putbuf(buffer, length);
		return length;
	}
	else
	{
		struct file *f = curr->fdt[fd];
		write_byte = file_write(f, buffer, length);
	}
	return write_byte;
}

bool create(const char *file, unsigned initial_size)
{
	check_address(file);
	if (file == NULL)
	{
		return false;
	}
	return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

void seek(int fd, unsigned position)
{
	struct thread *curr = thread_current();
	if (fd > curr->next_fd)
	{
		return;
	}
	struct file *curr_file = process_get_file(fd);
	if (curr_file == NULL)
	{
		return;
	}
	if (position < 0)
	{
		return;
	}
	file_seek(curr_file, position);
	return;
}

unsigned tell(int fd)
{
	struct thread *curr = thread_current();
	if (fd > curr->next_fd)
	{
		return;
	}

	struct file *curr_file = process_get_file(fd);
	if (curr_file == NULL)
	{
		return -1;
	}
	return file_tell(curr_file);
}

void close(int fd)
{
	struct thread *curr = thread_current();
	if (fd > curr->next_fd)
	{
		return;
	}
	process_close_file(fd);
	return;
}

int wait(pid_t pid)
{

	if (!pid || !check_address(pid))
	{
		printf("wait function of pid is not vaild \n");
		return -1;
	}

	if (process_wait(pid) == -1)
	{
		printf("process_wait is failed \n");
		return -1;
	}

	return 0;
}

int exec(const char *file)
{
	pid_t pid;
	struct thread *curr = thread_current();

	if (!file || !check_address(file))
	{
		printf("cmd_line is not vaild \n");
		return -1;
	}

	pid = process_create_initd(file);
	sema_down(&curr->sema_wait);

	if (pid == -1)
	{
		printf("process_create_initd is failed \n");
		exit(-1);
	}
	return pid;
}

pid_t fork(const char *thread_name)
{
	return;
}
