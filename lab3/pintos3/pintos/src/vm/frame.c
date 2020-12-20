#include <stdio.h>
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

static struct frame *frames;
static struct lock vm_sc_lock;
static size_t hand;
static size_t f_count;

/* frame初始化. */
void
frame_init (void) 
{
  void *base;
  lock_init (&vm_sc_lock);
  
  frames = malloc (sizeof *frames * init_ram_pages);

  while ( (base = palloc_get_page (PAL_USER)) != NULL ) 
    {
      f_count++;
      struct frame *f = &frames[f_count];
      lock_init (&f->lock);
      f->base = base;
      f->page = NULL;
      f->pinned = true;
    }
}

/* Allocate and lock a frame for page. */
struct frame *
frame_alloc_and_lock (struct page *page)
{
  lock_acquire (&vm_sc_lock);

  /* 找个free frame. */
  for (size_t i = 0; i < f_count; i++)
    {
      struct frame *f = &frames[i];
      if (!lock_try_acquire (&f->lock))
        continue;
      if (f->page == NULL)
        {
          f->page = page;
          lock_release (&vm_sc_lock);
          return f;
        } 
      lock_release (&f->lock);
    }

  /* No free frame.  Find a frame to evict. */
  for (size_t i = 0; i < f_count * 2; i++) 
    {
      /* Get a frame. */
      struct frame *f = &frames[hand];
      if (++hand >= f_count)
        hand = 0;

      if (!lock_try_acquire (&f->lock) && (&f->pinned))
        continue;

      if (f->page == NULL) 
        {
          f->page = page;
          lock_release (&vm_sc_lock);
          return f;
        } 

      if (is_page_accessed (f->page)) 
        {
          lock_release (&f->lock);
          continue;
        }
          
      lock_release (&vm_sc_lock);
      
      /* Evict this frame. */
      if (!page_out (f->page))
        {
          lock_release (&f->lock);
          return NULL;
        }

      f->page = page;
      return f;
    }

  lock_release (&vm_sc_lock);
  return NULL;
}

/* Locks P's frame into memory, if it has one.
   Upon return, p->frame will not change until P is unlocked. */
void
frame_lock (struct page *p) 
{
  /* A frame can be asynchronously removed, but never inserted. */
  struct frame *f = p->frame;
  if (f != NULL) 
    {
      lock_acquire (&f->lock);
      if (f != p->frame)
          lock_release (&f->lock);
    }
}

/* Releases frame F for use by another page.
   F must be locked for use by the current process.
   Any data in F is lost. */
void
frame_free (struct frame *f)
{
  f->page = NULL;
  lock_release (&f->lock);
}
