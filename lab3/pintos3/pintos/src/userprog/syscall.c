#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"

#define max_syscall 20

static int sys_halt (void);
static int sys_exit (int status);
static int sys_exec (const char *ufile);
static int sys_wait (tid_t);
static int sys_create (const char *ufile, unsigned initial_size);
static int sys_remove (const char *ufile);
static int sys_open (const char *ufile);
static int sys_filesize (int handle);
static int sys_read (int handle, void *udst_, unsigned size);
static int sys_write (int handle, void *usrc_, unsigned size);
static int sys_seek (int handle, unsigned position);
static int sys_tell (int handle);
static int sys_close (int handle);
static int sys_mmap (int handle, void *addr);
static int sys_munmap (int mapping);

static void syscall_handler (struct intr_frame *);
static void copy_in (void *, const void *, size_t);

void exit_special (void);
void * check_ptr2(const void *vaddr);

static struct lock fs_lock;
/* Our implementation for storing the array of system calls for Task2 and Task3 */
static void (*syscalls[max_syscall])(struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* Our implementation for Task2: initialize halt,exit,exec */
  syscalls[SYS_HALT] = &sys_halt;
  syscalls[SYS_EXIT] = &sys_exit;
  syscalls[SYS_EXEC] = &sys_exec;
  /* Our implementation for Task3: initialize create, remove, open, filesize, read, write, seek, tell, and close */
  syscalls[SYS_WAIT] = &sys_wait;
  syscalls[SYS_CREATE] = &sys_create;
  syscalls[SYS_REMOVE] = &sys_remove;
  syscalls[SYS_OPEN] = &sys_open;
  syscalls[SYS_WRITE] = &sys_write;
  syscalls[SYS_SEEK] = &sys_seek;
  syscalls[SYS_TELL] = &sys_tell;
  syscalls[SYS_CLOSE] =&sys_close;
  syscalls[SYS_READ] = &sys_read;
  syscalls[SYS_FILESIZE] = &sys_filesize;

  lock_init (&fs_lock);
}

/* System call handler. */
static void
syscall_handler (struct intr_frame *f)
{
  /* For Task2 practice, just add 1 to its first argument, and print its result */
  int * p = f->esp;
  check_ptr2 (p + 1);
  int type = * (int *)f->esp;
  if(type <= 0 || type >= max_syscall){
    exit_special ();
  }
  syscalls[type](f);
}


/* Copies SIZE bytes from user address USRC to kernel address
   DST.
   Call thread_exit() if any of the user accesses are invalid. */
static void
copy_in (void *dst_, const void *usrc_, size_t size)
{
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;

  while (size > 0)
    {
      size_t chunk_size = PGSIZE - pg_ofs (usrc);
      if (chunk_size > size)
        chunk_size = size;

      if (!page_lock (usrc, false))
        thread_exit ();
      memcpy (dst, usrc, chunk_size);
      page_unlock (usrc);

      dst += chunk_size;
      usrc += chunk_size;
      size -= chunk_size;
    }
}

/* Creates a copy of user string US in kernel memory
   and returns it as a page that must be freed with
   palloc_free_page().
   Truncates the string at PGSIZE bytes in size.
   Call thread_exit() if any of the user accesses are invalid. */
static char *
copy_in_string (const char *us)
{
  char *ks;
  char *upage;
  size_t length;

  ks = palloc_get_page (0);
  if (ks == NULL)
    thread_exit ();

  length = 0;
  for (;;)
    {
      upage = pg_round_down (us);
      if (!page_lock (upage, false))
        goto lock_error;

      for (; us < upage + PGSIZE; us++)
        {
          ks[length++] = *us;
          if (*us == '\0')
            {
              page_unlock (upage);
              return ks;
            }
          else if (length >= PGSIZE)
            goto too_long_error;
        }

      page_unlock (upage);
    }

 too_long_error:
  page_unlock (upage);
 lock_error:
  palloc_free_page (ks);
  thread_exit ();
}

/* Halt system call. */
void
sys_halt (struct intr_frame* f)
{
  shutdown_power_off ();
}

/* Exit system call. */
void
sys_exit (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  thread_current ()->st_exit = *user_ptr;
  thread_exit ();
}

/* Exec system call. */
void
sys_exec (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  check_ptr2 (*(user_ptr + 1));
  *user_ptr++;
  f->eax = process_execute((char*)* user_ptr);
}

/* Wait system call. */
void 
sys_wait (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  f->eax = process_wait(*user_ptr);
}


/* Create system call. */
static int
sys_create (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 5);
  check_ptr2 (*(user_ptr + 4));
  *user_ptr++;

  lock_acquire (&fs_lock);
  f->eax = filesys_create ((const char *)*user_ptr, *(user_ptr+1));
  lock_release (&fs_lock);
}

/* Remove system call. */
void
sys_remove (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  check_ptr2 (*(user_ptr + 1));
  *user_ptr++;

  lock_acquire (&fs_lock);
  f->eax = filesys_remove ((const char *)*user_ptr);
  lock_release (&fs_lock);
}

/* A file descriptor, for binding a file handle to a file. */
struct file_descriptor
  {
    struct list_elem elem;      /* List element. */
    struct file *file;          /* File. */
    int handle;                 /* File handle. */
  };

/* Open system call. */
void
sys_open (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  check_ptr2 (*(user_ptr + 1));
  *user_ptr++;
  // char *kfile = copy_in_string (ufile);
  // struct file_descriptor *fd;
  // int handle = -1;

  lock_acquire (&fs_lock);
  struct file * file_opened = filesys_open((const char *)*user_ptr);
  lock_release (&fs_lock);
  struct thread * t = thread_current();
  if (file_opened)
  {
    struct thread_file *thread_file_temp = malloc(sizeof(struct thread_file));
    thread_file_temp->fd = t->file_fd++;
    thread_file_temp->file = file_opened;
    list_push_back (&t->files, &thread_file_temp->file_elem);
    f->eax = thread_file_temp->fd;
  } 
  else
  {
    f->eax = -1;
  }

  // fd = malloc (sizeof *fd);
  // if (fd != NULL)
  //   {
  //     lock_acquire (&fs_lock);
  //     fd->file = filesys_open (kfile);
  //     if (fd->file != NULL)
  //       {
  //         struct thread *cur = thread_current ();
  //         handle = fd->handle = cur->next_handle++;
  //         list_push_front (&cur->fds, &fd->elem);
  //       }
  //     else
  //       free (fd);
  //     lock_release (&fs_lock);
  //   }

  // palloc_free_page (kfile);
  // return handle;
}

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with an
   open file. */
static struct file_descriptor *
lookup_fd (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds);
       e = list_next (e))
    {
      struct file_descriptor *fd;
      fd = list_entry (e, struct file_descriptor, elem);
      if (fd->handle == handle)
        return fd;
    }

  thread_exit ();
}

/* Filesize system call. */
static int
sys_filesize (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  int size;

  lock_acquire (&fs_lock);
  size = file_length (fd->file);
  lock_release (&fs_lock);

  return size;
}

/* Read system call. */
static int
sys_read (int handle, void *udst_, unsigned size)
{
  uint8_t *udst = udst_;
  struct file_descriptor *fd;
  int bytes_read = 0;

  fd = lookup_fd (handle);
  while (size > 0)
    {
      /* How much to read into this page? */
      size_t page_left = PGSIZE - pg_ofs (udst);
      size_t read_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Read from file into page. */
      if (handle != STDIN_FILENO)
        {
          if (!page_lock (udst, true))
            thread_exit ();
          lock_acquire (&fs_lock);
          retval = file_read (fd->file, udst, read_amt);
          lock_release (&fs_lock);
          page_unlock (udst);
        }
      else
        {
          size_t i;

          for (i = 0; i < read_amt; i++)
            {
              char c = input_getc ();
              if (!page_lock (udst, true))
                thread_exit ();
              udst[i] = c;
              page_unlock (udst);
            }
          bytes_read = read_amt;
        }

      /* Check success. */
      if (retval < 0)
        {
          if (bytes_read == 0)
            bytes_read = -1;
          break;
        }
      bytes_read += retval;
      if (retval != (off_t) read_amt)
        {
          /* Short read, so we're done. */
          break;
        }

      /* Advance. */
      udst += retval;
      size -= retval;
    }

  return bytes_read;
}

/* Write system call. */
static int
sys_write (int handle, void *usrc_, unsigned size)
{
  uint8_t *usrc = usrc_;
  struct file_descriptor *fd = NULL;
  int bytes_written = 0;

  /* Lookup up file descriptor. */
  if (handle != STDOUT_FILENO)
    fd = lookup_fd (handle);

  while (size > 0)
    {
      /* How much bytes to write to this page? */
      size_t page_left = PGSIZE - pg_ofs (usrc);
      size_t write_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Write from page into file. */
      if (!page_lock (usrc, false))
        thread_exit ();
      lock_acquire (&fs_lock);
      if (handle == STDOUT_FILENO)
        {
          putbuf ((char *) usrc, write_amt);
          retval = write_amt;
        }
      else
        retval = file_write (fd->file, usrc, write_amt);
      lock_release (&fs_lock);
      page_unlock (usrc);

      /* Handle return value. */
      if (retval < 0)
        {
          if (bytes_written == 0)
            bytes_written = -1;
          break;
        }
      bytes_written += retval;

      /* If it was a short write we're done. */
      if (retval != (off_t) write_amt)
        break;

      /* Advance. */
      usrc += retval;
      size -= retval;
    }

  return bytes_written;
}

/* Seek system call. */
static int
sys_seek (int handle, unsigned position)
{
  struct file_descriptor *fd = lookup_fd (handle);

  lock_acquire (&fs_lock);
  if ((off_t) position >= 0)
    file_seek (fd->file, position);
  lock_release (&fs_lock);

  return 0;
}

/* Tell system call. */
static int
sys_tell (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  unsigned position;

  lock_acquire (&fs_lock);
  position = file_tell (fd->file);
  lock_release (&fs_lock);

  return position;
}

/* Close system call. */
static int
sys_close (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  lock_acquire (&fs_lock);
  file_close (fd->file);
  lock_release (&fs_lock);
  list_remove (&fd->elem);
  free (fd);
  return 0;
}

/* Binds a mapping id to a region of memory and a file. */
struct mapping
  {
    struct list_elem elem;      /* List element. */
    int handle;                 /* Mapping id. */
    struct file *file;          /* File. */
    uint8_t *base;              /* Start of memory mapping. */
    size_t page_cnt;            /* Number of pages mapped. */
  };

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with a
   memory mapping. */
static struct mapping *
lookup_mapping (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings);
       e = list_next (e))
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      if (m->handle == handle)
        return m;
    }

  thread_exit ();
}

/* Remove mapping M from the virtual address space,
   writing back any pages that have changed. */
static void
unmap (struct mapping *m)
{
/* add code here */
  list_remove(&m->elem);
  for(int i = 0; i < m->page_cnt; i++)
  {
    //Pages written by the process are written back to the file...
    if (pagedir_is_dirty(thread_current()->pagedir, m->base + (PGSIZE * i)))
    {
      lock_acquire(&fs_lock);
      file_write_at(m->file, m->base + (PGSIZE * i), PGSIZE, (PGSIZE * i)); // Check 3rd parameter
      lock_release(&fs_lock);
    }
  }
  for(int i = 0; i < m->page_cnt; i++)
  {
    page_deallocate(m->base + (PGSIZE * i));
  }
}

/* Mmap system call. */
static int
sys_mmap (int handle, void *addr)
{
  struct file_descriptor *fd = lookup_fd (handle);
  struct mapping *m = malloc (sizeof *m);
  size_t offset;
  off_t length;

  if (m == NULL || addr == NULL || pg_ofs (addr) != 0)
    return -1;

  m->handle = thread_current ()->next_handle++;
  lock_acquire (&fs_lock);
  m->file = file_reopen (fd->file);
  lock_release (&fs_lock);
  if (m->file == NULL)
    {
      free (m);
      return -1;
    }
  m->base = addr;
  m->page_cnt = 0;
  list_push_front (&thread_current ()->mappings, &m->elem);

  offset = 0;
  lock_acquire (&fs_lock);
  length = file_length (m->file);
  lock_release (&fs_lock);
  while (length > 0)
    {
      struct page *p = page_allocate ((uint8_t *) addr + offset, false);
      if (p == NULL)
        {
          unmap (m);
          return -1;
        }
      p->private = false;
      p->file = m->file;
      p->file_offset = offset;
      p->file_bytes = length >= PGSIZE ? PGSIZE : length;
      offset += p->file_bytes;
      length -= p->file_bytes;
      m->page_cnt++;
    }

  return m->handle;
}

/* Munmap system call. */
static int
sys_munmap (int mapping)
{
/* add code here */
  unmap(lookup_mapping(mapping));
  return 0;
}

/* On thread exit, close all open files and unmap all mappings. */
void
syscall_exit (void)
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds); e = next)
    {
      struct file_descriptor *fd = list_entry (e, struct file_descriptor, elem);
      next = list_next (e);
      lock_acquire (&fs_lock);
      file_close (fd->file);
      lock_release (&fs_lock);
      free (fd);
    }

  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings);
       e = next)
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      next = list_next (e);
      unmap (m);
    }
}

/* Handle the special situation for thread */
void 
exit_special (void)
{
  thread_current()->st_exit = -1;
  thread_exit ();
}

/* New method to check the address and pages to pass test sc-bad-boundary2, execute */
void * 
check_ptr2(const void *vaddr)
{ 
  /* Judge address */
  if (!is_user_vaddr(vaddr))
  {
    exit_special ();
  }
  /* Judge the page */
  void *ptr = pagedir_get_page (thread_current()->pagedir, vaddr);
  if (!ptr)
  {
    exit_special ();
  }
  /* Judge the content of page */
  uint8_t *check_byteptr = (uint8_t *) vaddr;
  for (uint8_t i = 0; i < 4; i++) 
  {
    if (get_user(check_byteptr + i) == -1)
    {
      exit_special ();
    }
  }

  return ptr;
}