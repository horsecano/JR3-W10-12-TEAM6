#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "user/syscall.h"

/*
Project 3
lazy_load_segment 시 인자로 전달 */
struct load_segment_info
{
    struct file* file;
    off_t ofs;
    uint8_t* upage;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
};


tid_t process_create_initd(const char* file_name);
tid_t process_fork(const char* name, struct intr_frame* if_);
int process_exec(void* f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread* next);

// /* User Program */
struct thread* get_child_process(pid_t pid);
int remove_child_process(pid_t pid);

/* File Descipoter */
int process_add_file(struct file* f);
struct file* process_get_file(int fd);
void process_close_file(int fd);

#endif /* userprog/process.h */
