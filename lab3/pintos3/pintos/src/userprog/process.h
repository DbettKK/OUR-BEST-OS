#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct share_str { 
  char *file_name;                /* Program to load. */
  struct semaphore sema;          /* "Up"ed when loading complete. */
  struct child *child;            /* Child process. */
  bool isrun;                     /* Program successfully loaded? */
};
  
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */