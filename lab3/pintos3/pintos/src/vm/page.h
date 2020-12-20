#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/off_t.h"

struct page 
  {
    struct thread *thread;
    void *addr;
    bool r_only;
    bool dirty;
    struct hash_elem elem;
    struct frame *frame;

    block_sector_t sector;
    
    bool private;
    struct file *file;
    off_t file_offset;
    size_t file_bytes;
  };

bool page_lock (const void *, bool w_write);
bool page_unlock (const void *);
void page_exit (void);
void page_destroy (struct hash_elem *);
struct page *get_page_with_addr (const void *);
struct page *page_allocate (void *, bool r_only);
void page_deallocate (void *vaddr);
bool page_in (void *fault_addr);
bool page_out (struct page *);
bool is_page_accessed (struct page *);

hash_hash_func page_hash;
hash_less_func page_less;

#endif
