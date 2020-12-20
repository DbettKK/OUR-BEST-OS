#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <stdbool.h>
#include "threads/synch.h"

struct frame 
  {
    struct lock lock;
    struct page *page;
    void *base;
    bool pinned;
  };

struct frame *frame_alloc_and_lock (struct page *);

void frame_init (void);
void frame_lock (struct page *);
void frame_free (struct frame *);

#endif
