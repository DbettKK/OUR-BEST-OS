#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"

#define S_MAXSIZE 1048576

bool
install_page_in (struct page *p)
{
  p->frame = frame_alloc_and_lock (p);
  if (p->frame == NULL)
    return false;

  // 从frame拷贝数据
  if (p->sector != (block_sector_t) -1)
    {
      // 从swap读数据
      swap_in (p);
    }
  else if (p->file != NULL)
    {
      // 读文件
      off_t rb = file_read_at (p->file, p->frame->base, p->file_bytes, p->file_offset);
      off_t zb = PGSIZE - rb;
      memset (p->frame->base + rb, 0, zb);
    }
  else
      memset (p->frame->base, 0, PGSIZE);

  return true;
}

bool
page_in (void *fault_addr)
{
  struct page *p;
  bool success;

  if (thread_current()->pages == NULL)
    return false;

  p = get_page_with_addr (fault_addr);
  if (p == NULL)
    return false;

  frame_lock (p);
  if (p->frame == NULL)
    {
      if (!install_page_in (p))
        return false;
    }

  success = pagedir_set_page ( thread_current()->pagedir, p->addr, p->frame->base, !p->r_only);

  lock_release (&p->frame->lock);

  return success;
}

bool
page_out (struct page *p)
{
  bool dirty;
  bool ok = false;

  pagedir_clear_page(p->thread->pagedir, p->addr);
  dirty = pagedir_is_dirty(p->thread->pagedir, p->addr);
  if(!dirty)
    ok = true;
    
  if(!p->file)
    ok = swap_out(p);
  else
    if (dirty)
      if (!p->private)
          ok = file_write_at(p->file, p->frame->base, p->file_bytes, p->file_offset);
      else
          ok = swap_out(p);

  if(ok)
    p->frame = NULL;

  return ok;
}

void
page_exit (void)
{
  struct hash *h = thread_current()->pages;
  if (h != NULL)
    hash_destroy (h, page_destroy);
}

struct page *
get_page_with_addr (const void *address)
{
  if (address < PHYS_BASE)
    {
      struct page page;
      struct hash_elem *elem;

      /* find page. */
      page.addr = (void *) pg_round_down (address);
      elem = hash_find (thread_current()->pages, &page.elem);
      if (elem != NULL)
        return hash_entry (elem, struct page, elem);

      if((page.addr > PHYS_BASE - S_MAXSIZE) && (thread_current()->user_esp - 32 < address))
        return page_allocate(page.addr, false);
    }
  return NULL;
}

void
page_destroy (struct hash_elem *elem)
{
  struct page *p = hash_entry (elem, struct page, elem);
  frame_lock (p);
  if (p->frame)
    frame_free (p->frame);
  free (p);
}

bool
is_page_accessed (struct page *p)
{
  bool accessed;

  accessed = pagedir_is_accessed (p->thread->pagedir, p->addr);
  if (accessed)
    pagedir_set_accessed (p->thread->pagedir, p->addr, false);
  return accessed;
}

struct page *
page_allocate (void *vaddr, bool r_only)
{
  struct thread *t = thread_current();
  struct page *p = malloc (sizeof *p);
  if (p != NULL)
    {
      p->thread = thread_current();
      p->r_only = r_only;
      p->private = !r_only;
      p->file = NULL;
      p->file_offset = 0;
      p->file_bytes = 0;
      p->addr = pg_round_down (vaddr);
      p->frame = NULL;
      p->sector = (block_sector_t) -1;

      if (hash_insert (t->pages, &p->elem) != NULL)
        {
          free (p);
          p = NULL;
        }
    }
  return p;
}

void
page_deallocate (void *vaddr)
{
  struct page *p = get_page_with_addr (vaddr);

  frame_lock (p);
  if (p->frame)
    {
      struct frame *f = p->frame;
      if (p->file && !p->private)
        page_out (p);
      frame_free (f);
    }
  hash_delete (thread_current()->pages, &p->elem);
  free (p);
}

unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, elem);
  return ((uintptr_t) p->addr) >> PGBITS;
}

bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, elem);
  const struct page *b = hash_entry (b_, struct page, elem);

  return a->addr < b->addr;
}

bool
page_lock (const void *addr, bool w_write)
{
  struct page *p = get_page_with_addr (addr);
  if (p == NULL || (p->r_only && w_write))
    return false;

  frame_lock (p);
  if (p->frame == NULL)
    {
      bool is_in = install_page_in (p);
      bool is_page_set = pagedir_set_page (thread_current()->pagedir, p->addr, p->frame->base, !p->r_only);
      return is_in && is_page_set;
    }
  else
    return true;
}

bool
page_unlock (const void *addr)
{
  struct page *p = get_page_with_addr (addr);
  if(p == NULL)
    return false;
  
  lock_release (&p->frame->lock);
  return true;
}
