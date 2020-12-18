# 小组相关

|  姓名  |   学号   | 贡献百分比 |
| :----: | :------: | :--------: |
| 张涵珂 | 18373734 |    25%     |
|  杨壮  | 17376193 |    25%     |
|  裴昱  | 17375244 |    25%     |
| 杨瑞羿 | 17241055 |    25%     |

#### Github截图

xxx

# 通过用例情况

![通过用例情况](pics\proj3.png)

# 需求分析

## Paging



## Stack Growth



## Memory Mapped Files



## Accessing User Memory

#### 1. 概述

和project2中系统调用时检查用户栈指针一样，内核必须非常小心，因为用户可以传递空指针，指向未映射的虚拟内存的指针或指向内核虚拟地址空间的指针（位于上方`PHYS_BASE`）。通过终止有问题的进程并释放其资源，必须拒绝所有这些类型的无效指针，从而不会损害内核或其他正在运行的进程。

#### 2. 题目分析

有两种合理的方法可以正确地执行此操作。

第一种方法是验证用户提供的指针的有效性，然后取消引用它。通过 `userprog/pagedir.c`和`threads/vaddr.h`中的函数进行处理即可完成。第二种方法则是仅检查用户指针是否指向下方`PHYS_BASE`，然后取消引用。无效的用户指针将导致“页面错误”，在 `userprog/exception.c`中的`page_fault()`进行相关修复即可。该技术通常更快，因为它利用了处理器的MMU。我们本次则是使用第二种方法来完成。具体可参考下方功能设计。

# 功能设计

## Paging



## Stack Growth



## Memory Mapped Files



## Accessing User Memory

为了验证用户指针的有效性，在每次系统调用之前我们都通过上文所述的第二种方法实现对用户指针有效性的验证。具体实现可参考下面代码。

```c
static void syscall_handler (struct intr_frame *f) {
    ...
    copy_in (&call_nr, f->esp, sizeof call_nr);
    ...
}
static void copy_in (void *dst_, const void *usrc_, size_t size) {
  ...
      if (!page_lock (usrc, false))
        thread_exit ();
  ...
}

bool page_lock (const void *addr, bool will_write) {
  struct page *p = page_for_addr (addr);
  if (p == NULL || (p->read_only && will_write))
    return false;

  ...
}

static struct page * page_for_addr (const void *address) {
  if (address < PHYS_BASE){
      ...
  }
  return NULL;
}
```

> `syscall_handler()->copy_in()->page_lock()->page_for_addr()->page_lock()->copy_in()->thread_exit()`

我们的逻辑就是通过在syscall_handler()函数中，进行用户指针的一个处理，如果用户指针没问题，则可以通过其指针访问内存获取对应数据，如果存在问题，则最终会在page_for_addr()函数中返回NULL，导致page_lock()函数页返回false，并且进而导致在copy_in()函数中因为用户指针的无效导致的进程退出，即thread_exit()，从而完成一个用户指针的校验。总流程可参考上面一行函数的执行过程。

# 相关问题的回答

 ## PAGE TABLE MANAGEMENT

### DATA STRUCTURES

A1: Copy here the declaration of each new or changed `struct `or`struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

> 

### ALGORITHMS

A2: In a few paragraphs, describe your code for accessing the data stored in the SPT about a given page.

> 

A3: How does your code coordinate accessed and dirty bits between kernel and user virtual addresses that alias a single frame, or alternatively how do you avoid the issue?

> 

### SYNCHRONIZATION

A4: When two user processes both need a new frame at the same time, how are races avoided?

> 

### RATIONALE

A5: Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?

> 

##  PAGING TO AND FROM DISK

### DATA STRUCTURES

B1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

> 

### ALGORITHMS

B2: When a frame is required but none is free, some frame must be evicted.  Describe your code for choosing a frame to evict.

> 

B3: When a process P obtains a frame that was previously used by a process Q, how do you adjust the page table (and any other data structures) to reflect the frame Q no longer has?

> 

B4: Explain your heuristic for deciding whether a page fault for an invalid virtual address should cause the stack to be extended into the page that faulted.

> 

### SYNCHRONIZATION

B5: Explain the basics of your VM synchronization design.  In particular, explain how it prevents deadlock.  (Refer to the textbook for an explanation of the necessary conditions for deadlock.)

> 

B6: A page fault in process P can cause another process Q's frame to be evicted.  How do you ensure that Q cannot access or modify the page during the eviction process?  How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

> 

B7: Suppose a page fault in process P causes a page to be read from the file system or swap.  How do you ensure that a second process Q cannot interfere by e.g. attempting to evict the frame while it is still being read in?

> 

B8: Explain how you handle access to paged-out pages that occur during system calls.  Do you use page faults to bring in pages (as in user programs), or do you have a mechanism for "locking" frames into physical memory, or do you use some other design?  How do you gracefully handle attempted accesses to invalid virtual addresses?

> 

### RATIONALE

B9: A single lock for the whole VM system would make synchronization easy, but limit parallelism.  On the other hand, using many locks complicates synchronization and raises the possibility for deadlock but allows for high parallelism.  Explain where your design falls along this continuum and why you chose to design it this way.

> 

## MEMORY MAPPED FILES

### DATA STRUCTURES

C1: Copy here the declaration of each new or changed `struct` or`struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

>```c
>// userprog/syscall.c
>struct mapping {
>    struct list_elem elem;      /* List element. */
>    int handle;                 /* Mapping id. */
>    struct file *file;          /* File. */
>    uint8_t *base;              /* Start of memory mapping. */
>    size_t page_cnt;            /* Number of pages mapped. */
>};
>```
>
>mapping结构体，用于在文件映射时进行记录相应的内存基地址以及映射的文件等等内容。

### ALGORITHMS

C2: Describe how memory mapped files integrate into your virtual memory subsystem.  Explain how the page fault and eviction processes differ between swap pages and other pages.

> 一个线程包含其打开的所有文件，且结合上文所说的mapping结构体即可实现。内存映射文件封装在mapping结构体中。每个映射都包含对其在内存中的地址的引用，以及它映射的文件。每个线程都包含映射到该线程的所有文件的列表，这些文件可用于管理哪些文件直接存在于内存中。否则，包含内存映射文件信息的页面的管理方式与其他页面相同。
>
> 不同之处在于：对于属于内存映射文件的页，页错误和逐出过程稍微存在不同。收回时，与文件无关的页将被移动到交换分区，而不管该页是否脏。收回时，只有在修改内存映射文件页时，才能将其写回文件。否则，不需要写入。而对于内存映射文件，交换分区是完全避免的。

C3: Explain how you determine whether a new file mapping overlaps any existing segment.

> 我们只有在找到空闲且未映射的页时，才会分配用于新文件映射的页。page_allocated()函数可以访问所有现有的文件映射，并将拒绝分配任何已占用的空间。page_allocate()函数只会分配空闲且未映射的页。它将尝试将新映射添加到页表，如果页已存在，它将取消映射并返回NULL。
>

### RATIONALE

C4: Mappings created with "mmap" have similar semantics to those of data demand-paged from executables, except that "mmap" mappings are written back to their original files, not to swap.  This implies that much of their implementation can be shared.  Explain why your implementation either does or does not share much of the code for the two situations.

> 任何页面，无论其来源如何，最终都将通过page_out()函数进行分页。唯一的区别是检查页面是否应该写回磁盘。如果页面被标记为私有，那么它会被交换到交换分区，否则它应该被写到磁盘上的文件中。
>