#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <stdbool.h>
#include "threads/synch.h"

/* Frame struct. */
struct frame 
  {
    struct lock lock;           /* Prevent simultaneous access. */
    struct page *page;          /* Mapped process page, if any. */
    void *base;                 /* Kernel virtual base address. */
    bool pinned;
  };

struct frame *frame_alloc_and_lock (struct page *);

void frame_init (void);
void frame_lock (struct page *);
void frame_free (struct frame *);

#endif
