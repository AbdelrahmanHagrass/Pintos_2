#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include "list.h"
#include "process.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"


static void syscall_handler (struct intr_frame *);
void* check_addr(const void*);
struct process_file* list_search(struct list* files, int fd);

extern bool running;

struct process_file {
	struct file* ptr;
	int fileId;
	struct list_elem elem;
};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
bool create(const char *file, unsigned initial_size)
{
    acquire_filesys_lock();
    bool ret = filesys_create(file, initial_size);
   release_filesys_lock();
    return ret;
}

bool remove (const char *file)
{
     acquire_filesys_lock();
    bool ret = filesys_remove(file);
     release_filesys_lock();
    return ret;
}
void halt (void)
{
    shutdown_power_off();
}
int wait (tid_t pid)
{
    return process_wait(pid);
}
int open (const char *file)
{
    int ret = -1;
   acquire_filesys_lock();
    struct thread *cur = thread_current ();
    struct file * opened_file = filesys_open(file);
    release_filesys_lock();
   
    if(opened_file != NULL)
    {
        cur->fd_count= cur->fd_count + 1;
        ret = cur->fd_count;
        
        struct process_file *F= (struct process_file*) malloc(sizeof(struct process_file));
        F->fileId= ret;
        F->ptr = opened_file;
        // add this file to this thread file list
        list_push_back(&cur->files, &F->elem);
    }
    return ret;
}
int write (int fd, const void *buffer_, unsigned size)
{
    uint8_t * buffer = (uint8_t *) buffer_;
    int ret = -1;
    if (fd == 1)
    {
        // write in the consol
        putbuf( (char *)buffer, size);
        
        return (int)size;
    }
    else
    {
        //write in file
        //get the process_file
        struct process_file *fd_elem = list_search(&thread_current()->files,fd);
        if(fd_elem == NULL || buffer_ == NULL )
        {
            return -1;
        }
        //get the file
        struct file *myfile = fd_elem->ptr;
        acquire_filesys_lock();
        ret = file_write(myfile, buffer_, size);
        release_filesys_lock();
    }
    return ret;
}
int filesize (int fd)
{
    struct file *myfile = list_search(&thread_current()->files,fd)->ptr;
   acquire_filesys_lock();
    int ret = file_length(myfile);
    release_filesys_lock();
    return ret;
}
int read (int fd, void *buffer, unsigned size)
{
    int ret = -1;
    if(fd == 0)
    {
        // read from the keyboard
        ret = input_getc();
    }
    else if(fd > 0)
    {
        //read from file
        //get the fd_element
        struct process_file *fd_elem = list_search(&thread_current()->files,fd);
        if(fd_elem == NULL || buffer == NULL)
        {
            return -1;
        }
        //get the file
        struct file *myfile = fd_elem->ptr;
        acquire_filesys_lock();
        ret = file_read(myfile, buffer, size);
       release_filesys_lock();
        if(ret < (int)size && ret != 0)
        {
            //some error happened
            ret = -1;
        }
    }
    return ret;
}

void seek (int fd, unsigned position)
{
    struct process_file *fd_elem = list_search(&thread_current()->files,fd);
    if(fd_elem == NULL)
    {
        return;
    }
    struct file *myfile = fd_elem->ptr;
   acquire_filesys_lock();
    file_seek(myfile,position);
      release_filesys_lock();
}

unsigned tell (int fd)
{
     struct process_file *fd_elem = list_search(&thread_current()->files,fd);
    if(fd_elem == NULL)
    {
        return -1;
    }
    struct file *myfile = fd_elem->ptr;
    acquire_filesys_lock();
    unsigned ret = file_tell(myfile);
   release_filesys_lock();
    return ret;
}
void* check_addr(const void *vaddr)
{
 
	if (!is_user_vaddr(vaddr))
	{
		exit(-1);
	
	}
	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if (ptr==NULL)
	{
		exit(-1);
		
	}
	return;
}
void get_args_1(struct intr_frame *f, int choose, void *args)
{
  

    int argv = *((int*) args);
   
    args += 4;
  
    if (choose == SYS_EXIT)
    {
        exit(argv);
    }
    else if (choose == SYS_EXEC)
    {
       check_addr((const void*) argv); 
        f -> eax = exec_process((const char *)argv);
    }
    else if (choose == SYS_WAIT)
    {
        f -> eax = wait(argv);
    }
    else if (choose == SYS_REMOVE)
    {
        check_addr((const void*) argv);
        f -> eax = remove((const char *) argv);
    }
    else if(choose == SYS_OPEN)
    {
        check_addr((const void*) argv);
        f -> eax = open((const char *) argv);
    }
    else if (choose == SYS_FILESIZE)
    {
        f -> eax = filesize(argv);
    }
    else if (choose == SYS_TELL)
    {
        f -> eax = tell(argv);
    }
    else if (choose == SYS_TELL)
    {
        close(&thread_current()->files,argv);
    }
}

void get_args_2(struct intr_frame *f, int choose, void *args)
{
    int argv = *((int*) args);
    args += 4;
    int argv_1 = *((int*) args);
    args += 4;

    if (choose == SYS_CREATE)
    {
        check_addr((const void*) argv);
        f -> eax = create((const char *) argv, (unsigned) argv_1);
    }
    else if(choose == SYS_SEEK)
    {
        seek(argv, (unsigned)argv_1);
    }
}


void get_args_3 (struct intr_frame *f, int choose, void *args)
{
    int argv = *((int*) args);
    args += 4;
    int argv_1 = *((int*) args);
    args += 4;
    int argv_2 = *((int*) args);
    args += 4;

   check_addr((const void*) argv_1);
    void * temp = ((void*) argv_1)+ argv_2 ;
    check_addr((const void*) temp);
    if (choose == SYS_WRITE)
    {
        f->eax = write (argv,(void *) argv_1,(unsigned) argv_2);
    }
    else f->eax = read (argv,(void *) argv_1, (unsigned) argv_2);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  struct intr_frame *temp=f;
    int syscall_number = 0;
    int * p = f->esp;
  	check_addr(p);
    
    void *args = f -> esp;
    args+=4; 
     check_addr((const void*) args);
    syscall_number = *(int*)temp->esp;
     
    switch(syscall_number)
    {
    case SYS_HALT:                  	/* Halt the operating system. */
        halt();
        break;
    case SYS_EXIT:                   /* Terminate this process. */
    
        get_args_1(f, SYS_EXIT,args);
        break;
    case SYS_EXEC:                   /* Start another process. */
		 
	  	check_addr(*(p+4));
        get_args_1(f, SYS_EXEC,args);
        break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
   
        get_args_1(f, SYS_WAIT,args);
        break;
    case SYS_CREATE:                 /* Create a file. */
        get_args_2(f, SYS_CREATE,args);
        break;
    case SYS_REMOVE:                 /* Delete a file. */
        get_args_1(f, SYS_REMOVE,args);
        break;
    case SYS_OPEN:                   /* Open a file. */
        get_args_1(f, SYS_OPEN,args);
        break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
        get_args_1(f, SYS_FILESIZE,args);
        break;
    case SYS_READ:                   /* Read from a file. */
        get_args_3(f, SYS_READ,args);
        break;
    case SYS_WRITE:                  /* Write to a file. */
        get_args_3(f, SYS_WRITE,args);
        break;
    case SYS_SEEK:                   /* Change position in a file. */
        get_args_2(f, SYS_SEEK,args);
        break;
    case SYS_TELL:                   /* Report current position in a file. */
        get_args_1(f, SYS_TELL,args);
        break;
    case SYS_CLOSE:                  /* Close a file. */
        get_args_1(f, SYS_CLOSE,args);
        break;
    default:
        exit(-1);
        break;
    }
}
tid_t exec_process(const char *file_name)
{
	acquire_filesys_lock();
	char * fn_cp = malloc(strlen(file_name)+1);
	strlcpy(fn_cp, file_name, strlen(file_name)+1);
	  
	  char * save_ptr;
	  fn_cp = strtok_r(fn_cp," ",&save_ptr);

	 struct file* f = filesys_open (fn_cp);
     
	  if(f==NULL)
	  {
	  	release_filesys_lock();
	  	return -1;
	  }

	  else
	  {
	  	file_close(f);
	  	release_filesys_lock();
	  	return process_execute(file_name);
	  }
}
void exit(int status)
{
	
	struct list_elem *e;

      for (e = list_begin (&thread_current()->parent->child_Process_List); e != list_end (&thread_current()->parent->child_Process_List);
           e = list_next (e))
        {
          struct child *chi = list_entry (e, struct child, elem);
          if(chi->tid == thread_current()->tid)
          {
          	chi->finished = true;
          	chi->exit_error = status;
          }
        }
	thread_current()->exit_error = status;

	if(thread_current()->parent->Waiting_on == thread_current()->tid)
		sema_up(&thread_current()->parent->child_sema);

	thread_exit();
}
struct process_file* list_search(struct list* files, int Id)
{

	struct list_elem *e;

      for (e = list_begin (files); e != list_end (files);
           e = list_next (e))
        {
          struct process_file *f = list_entry (e, struct process_file, elem);
          if(f->fileId== Id)
          	return f;
        }
   return NULL;
}
void close(struct list* files, int Id)
{

	struct list_elem *e;

	struct process_file *f;

      for (e = list_begin (files); e != list_end (files);
           e = list_next (e))
        {
          f = list_entry (e, struct process_file, elem);
          if(f->fileId == Id)
          {
          	file_close(f->ptr);
          	list_remove(e);
            return ;
          }
        }
}
void close_all_files(struct list* files)
{

	struct list_elem *e;

	while(!list_empty(files))
	{
		e = list_pop_front(files);

		struct process_file *f = list_entry (e, struct process_file, elem);
          
	      	file_close(f->ptr);
	      	list_remove(e);
	      	free(f);
	}
}


