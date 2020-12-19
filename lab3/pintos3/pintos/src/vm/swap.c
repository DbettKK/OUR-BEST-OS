#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* The swap device. */
static struct block *s_block;

/* Used swap pages. */
static struct bitmap *s_bitmap;

/* Protects s_bitmap. */
static struct lock s_lock;

/* Number of sectors per page. */
static const size_t P_SECTORS = PGSIZE / BLOCK_SECTOR_SIZE;
/* Sets up swap. */
void
swap_init (void)
{
  s_block = block_get_role (BLOCK_SWAP);

  if (s_block == NULL)
    {
      PANIC("cannot init the swap block");
      NOT_REACHED();
    }
  else
    {
      size_t s_size = block_size (s_block) / P_SECTORS;
      s_bitmap = bitmap_create (s_size);
    }
    
  lock_init (&s_lock);
}

/* Swaps in page P, which must have a locked frame
   (and be swapped out). */
void
swap_in (struct page *p)
{
  for (size_t i = 0; i < P_SECTORS; i++)
    block_read (s_block, p->sector + i, p->frame->base + i * BLOCK_SECTOR_SIZE);// 

  bitmap_reset (s_bitmap, p->sector / P_SECTORS);
  p->sector = (block_sector_t) -1;
}

/* Swaps out page P, which must have a locked frame. */
bool
swap_out (struct page *p)
{
  lock_acquire (&s_lock);
  size_t slot = bitmap_scan_and_flip (s_bitmap, 0, 1, false);
  lock_release (&s_lock);

  if (slot == BITMAP_ERROR)
    return false;

  p->sector = slot * P_SECTORS;

  // Write out page sectors
  /* add code here */
  for (size_t i = 0; i < P_SECTORS; i++)
    block_write(s_block, p->sector + i, p->frame->base + (i * BLOCK_SECTOR_SIZE));

  p->private = false;
  p->file = NULL;
  p->file_offset = 0;
  p->file_bytes = 0;

  return true;
}