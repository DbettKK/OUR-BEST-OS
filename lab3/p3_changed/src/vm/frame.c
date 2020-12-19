#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static struct frame *frames;
static size_t f_count;

static struct lock vm_sc_lock;
static size_t hand;

/* Initialize the frame manager. */
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

/* Tries to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */
struct frame *
frame_alloc_and_lock (struct page *page)
{
  lock_acquire (&vm_sc_lock);

  /* Find a free frame. */
  for (int i = 0; i < f_count; i++)
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
  for (int i = 0; i < f_count * 2; i++) 
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

      if (page_accessed_recently (f->page)) 
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