#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include "threads/thread.h"
#include <list.h>
#include "threads/synch.h"

void syscall_init (void);
void exit(int);
tid_t exec_process(const char *);
#endif /* userprog/syscall.h */
