# Pintos-Project2-分析文档

## 1. 小组相关

|  姓名  |   学号   | 贡献百分比 |
| :----: | :------: | :--------: |
| 张涵珂 | 18373734 |    25%     |
|  杨壮  | 17376193 |    25%     |
|  裴昱  | 17375244 |    25%     |
| 杨瑞羿 | 17241055 |    25%     |

## 2. 所分析代码通过测试用例情况

![proj2](.\pics\proj2.png)

## 3. 相关函数调用关系图以及功能说明

### 3.1 Process Termination Messages

#### 3.1.1 相关函数调用关系图

> 由于本题目仅对`void process_exit(void)`函数进行改动，整体过程可以参考3.2.1的相关函数调用关系图。

#### 3.1.2 功能说明

`void process_exit (void);`

> 用于释放本进程的资源，并且在此打印本题目中所要求的退出信息，详细说明如下所示。

```c
/* 释放当前进程的资源. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* 打印题目所要求的退出信息 */
  printf ("%s: exit(%d)\n",cur->name, cur->st_exit);
    
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
  {
    /* Correct ordering here is crucial.  We must set
        cur->pagedir to NULL before switching page directories,
        so that a timer interrupt can't switch back to the
        process page directory.  We must activate the base page
        directory before destroying the process's page
        directory, or our active page directory will be one
        that's been freed (and cleared). */
    cur->pagedir = NULL;
    pagedir_activate (NULL);
    pagedir_destroy (pd);
  }
}
```

### 3.2 Argument Passing

#### 3.2.1 相关函数调用关系图

![Argu](.\pics\Argu.png)

#### 3.2.2 功能说明

`tid_t process_execute (const char *file_name);`

> 用于从`file_name`创建一个用于运行用户程序的新线程。返回值是新进程的TID，当线程不能被创建的时候，返回值是`TID_ERROR`。具体内容如下：

```c
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  tid_t tid;
  /* 对file_name做两个拷贝，否则caller和load()之间会有竞争。 */
  char *fn_copy = malloc(strlen(file_name)+1);
  char *fn_copy2 = malloc(strlen(file_name)+1);
  strlcpy (fn_copy, file_name, strlen(file_name)+1);
  strlcpy (fn_copy2, file_name, strlen(file_name)+1);


  /* 创建一个用于执行可执行文件的新进程。 */
  char * save_ptr;//用于存储strtok_r的字符串的数组指针
  fn_copy2 = strtok_r (fn_copy2, " ", &save_ptr);
  tid = thread_create (fn_copy2, PRI_DEFAULT, start_process, fn_copy);//创建线程
  free (fn_copy2);//fn_copy2已经完成了使命，将其释放掉

  if (tid == TID_ERROR){//若创建线程失败，则报错
    free (fn_copy);//释放掉fn_copy2
    return tid;
  }

  /* 获取信号量，等待子进程结束执行 */
  sema_down(&thread_current()->sema);
  if (!thread_current()->success) return TID_ERROR;

  return tid;
}
```



`void push_argument (void **esp, int argc, int argv[]);`

> 根据栈顶指针`esp`和参数数量`argc`两个参数将`argv[]`中的参数压入栈中，具体说明请参考以下注释：

```c
void
push_argument (void **esp, int argc, int argv[]){
  *esp = (int)*esp & 0xfffffffc;//栈对齐
  *esp -= 4;
  *(int *) *esp = 0;//首先放入一个0，防止没有参数的情况出现
  for (int i = argc - 1; i >= 0; i--)//将参数逆序压入栈中
  {
    *esp -= 4;
    *(int *) *esp = argv[i];
  }
  *esp -= 4;
  *(int *) *esp = (int) *esp + 4;//让esp指针指向新的栈顶
  *esp -= 4;
  *(int *) *esp = argc;//将argc压入栈中
  *esp -= 4;
  *(int *) *esp = 0;//将返回地址压入栈中
}
```



`static void start_process (void *file_name_);`

> 在本函数中，interrupt frame将会完成初始化，若初始化成功，则加载可执行文件，分离参数并且传递运行，若初始化失败，则直接退出线程。我们在这个函数中结合`push_argument()`实现了参数分割的任务，具体功能如下所示：

```c
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  char *fn_copy=malloc(strlen(file_name)+1);
  strlcpy(fn_copy,file_name,strlen(file_name)+1);

  /* 初始化intr_frame并加载可执行文件. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  char *token, *save_ptr;
  file_name = strtok_r (file_name, " ", &save_ptr);
  success = load (file_name, &if_.eip, &if_.esp);

  if (success){//若初始化成功，则计算参数总数和参数的规格
    int argc = 0;
    int argv[50];//由题目可知，参数的数量不会多于50个
    for (token = strtok_r (fn_copy, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
      if_.esp -= (strlen(token)+1);//用户栈向低地址增长，所以是减
      memcpy (if_.esp, token, strlen(token)+1);
      argv[argc++] = (int) if_.esp;//提取参数
    }
    push_argument (&if_.esp, argc, argv);//将参数压入用户栈中
    /* 将exec_status记录到父进程的success中，并且释放信号量。 */
    thread_current ()->parent->success = true;
    sema_up (&thread_current ()->parent->sema);
  }
    
  else{//若初始化失败，则退出线程
    thread_current ()->parent->success = false;
    sema_up (&thread_current ()->parent->sema);
    thread_exit ();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}
```

### 3.3 System Calls

#### 3.3.1 相关函数调用关系图



#### 3.3.2 功能说明

`sys_seek`

>  获取用户栈指针，在判断调用是否合法后取出文件描述符，在文件列表中查找文件位置，并用file_seek函数保存文件位置，详细功能如下：

```c
/* Do system seek, by calling the function file_seek() in filesystem */
void 
sys_seek(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp; //获取用户栈指针
  check_ptr2 (user_ptr + 5); //调用check_ptr2()函数判断当前调用是否合法
  *user_ptr++; //通过用户栈指针从用户栈中取出要查找的文件的文件描述符fd
  struct thread_file *file_temp = find_file_id (*user_ptr); //调用file_find_id函数传入此fd作为参数返回查询结果
  if (file_temp) //找到且返回了该文件（即该文件对应的结构体）
  {
    acquire_lock_f (); //获取锁
    file_seek (file_temp->file, *(user_ptr+1)); //调用file_seek()函数获取文件指针并保存文件位置
    release_lock_f (); //释放锁
  }
}
```

`sys_tell`

> 获取用户栈指针，在判断调用是否合法后取出文件描述符，在文件列表中查找文件位置，如果找到，将结果返回到eax，未找到则将eax置为-1，详细功能如下：

```c
/* Do system tell, by calling the function file_tell() in filesystem */
void 
sys_tell (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp; //获取用户栈指针
  check_ptr2 (user_ptr + 1); //调用check_ptr2()函数判断当前调用是否合法
  *user_ptr++; //通过用户栈指针从用户栈中取出要查找的文件的文件描述符fd
  struct thread_file *thread_file_temp = find_file_id (*user_ptr); //调用file_find_id函数传入此fd作为参数返回查询结果
  if (thread_file_temp) //找到了文件
  {
    acquire_lock_f (); //获取锁
    f->eax = file_tell (thread_file_temp->file); //调用file_tell()函数返回文件位置并返回结果到eax中
    release_lock_f (); //释放锁
  }else{
    f->eax = -1; //没找到，将返回值eax置为-1后返回-
  }
}
```

`sys_close`

> 获取用户栈指针，在判断调用是否合法后取出文件描述符，在文件列表中查找文件位置，如果找到，说明文件打开，用file_close函数关闭文件，并将文件从线程列表中移除，详细功能如下

```c
/* Do system close, by calling the function file_close() in filesystem */
void 
sys_close (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp; //获取用户栈指针
  check_ptr2 (user_ptr + 1); //调用check_ptr2()函数判断当前调用是否合法
  *user_ptr++; //通过用户栈指针从用户栈中取出要查找的文件的文件描述符fd
  struct thread_file * opened_file = find_file_id (*user_ptr);//调用file_find_id函数传入此fd作为参数返回查询结果
  if (opened_file)//找到了该文件
  {
    acquire_lock_f ();//获取锁
    file_close (opened_file->file); //调用file_close()函数关闭文件
    release_lock_f (); //释放锁
    /* Remove the opened file from the list */
    list_remove (&opened_file->file_elem);//将该文件从线程列表中移除
    /* Free opened files */
    free (opened_file);//释放该文件
  }
}
```



### 3.4 Denying Writes to Executables

#### 3.4.1 相关函数调用关系图



#### 3.4.2 功能说明
<file.c>

`file_deny_write`

> 通过改变file中deny_write的bool值为true使其在打开后不可写
>
> 同时还将inode中deny_write_node的值加1，以便后续allow_write判断
>
> file_allow_write:通过改变file中deny_write的bool值为false使文件在关闭后恢复可写
>
> 同时还将inode中deny_write_node的值减1

```c
/* Prevents write operations on FILE's underlying inode
   until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file) 
{
  ASSERT (file != NULL); /*judge whether file is null*/
  if (!file->deny_write)  /*judge the value of deny_write, which belongs to file*/
    {
      file->deny_write = true;  /*ensure that deny_write is true*/
      inode_deny_write (file->inode);
      /*call function inode_deny_write to add deny_write_node with 1*/
    }
}

/* Re-enables write operations on FILE's underlying inode.
   (Writes might still be denied by some other file that has the
   same inode open.) */
void
file_allow_write (struct file *file) 
{
  ASSERT (file != NULL);/*judge whether file is null*/
  if (file->deny_write)  /*judge the value of deny_write, which belongs to file*/
    {
      file->deny_write = false; /*ensure that deny_write is false*/
      inode_allow_write (file->inode);
      /*call function inode_deny_write to substract deny_write_node by 1*/
    }
}
```

<inode.c>

`inode_deny_write`

> 将deny_write_cnt+1,同时保证deny_write_cnt<=open_cnt，保证逻辑关系为：在打开文件后禁止写入

`inode_allow_write`

> 将deny_write_cnt-1,同时保证deny_write_cnt > 0，而且deny_write_cnt<=open_cnt，保证逻辑关系为：在关上文件后恢复可写入，而且原来文件状态是打开且禁止写入的

```c
/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++; /*add deny_write_cnt with 1*/
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0); /*ensure deny_write_cnt is positive*/
  ASSERT (inode->deny_write_cnt <= inode->open_cnt); 
    /*ensure the value above is smaller than number of openers*/
  inode->deny_write_cnt--; /*substract deny_write_cnt with 1*/
}
```




## 4. 相关数据结构分析说明

### 4.1 Process Termination Messages

> 我们在`threads/thread.c`的`struct thread`结构中增加了`int st_exit`参数，用于保存进程退出时的状态。

### 4.2 Argument Passing

> 在本任务中，我们没有增加新的数据结构。

### 4.3 System Calls

```c
struct thread_file
{
    int fd;
    struct file* file;
    struct list_elem file_elem;
};
struct thread
{
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;  
    /* 添加的 */
	struct list childs;                 /* The list of childs */
    struct child * thread_child;        /* Store the child of this thread */
    int st_exit;                        /* Exit status */
    struct semaphore sema;              /* Control the child process's logic, finish parent waiting for child */
    bool success;                       /* Judge whehter the child's thread execute successfully */
    struct thread* parent;              /* Parent thread of the thread */
    
    /* Structure for Task3 */
    struct list files;                  /* List of opened files */
    int file_fd;                        /* File's descriptor */
    struct file * file_owned;           /* The file opened */

};
struct child
{
    tid_t tid;                           /* tid of the thread */
    bool isrun;                          /* whether the child's thread is run successfully */
    struct list_elem child_elem;         /* list of children */
    struct semaphore sema;               /* semaphore to control waiting */
    int store_exit;                      /* the exit status of child thread */
};
```



### 4.4 Denying Writes to Executables



## 5. 相关函数流程图分析说明

### 5.1 Process Termination Messages

`void process_exit (void);`

<img src=".\pics\process_exit.png" alt="process_exit" style="zoom:67%;" />

### 5.2 Argument Passing

`tid_t process_execute (const char *file_name);`

<img src=".\pics\process_execute.png" alt="process_execute" style="zoom:67%;" />

`void push_argument (void **esp, int argc, int argv[]);`

<img src=".\pics\push_argument.png" alt="process_execute" style="zoom:67%;" />

`static void start_process (void *file_name_);`

<img src=".\pics\start_process.png" alt="process_execute" style="zoom:67%;" />

### 5.3 System Calls

`sys_remove()`

<img src=".\pics\proj2\流程图\3系统调用\sys_remove.png" alt="sys_remove流程图" style="zoom:50%;" />

`sys_open()`

<img src=".\pics\proj2\流程图\3系统调用\sys_open.png" alt="sys_open流程图" style="zoom:50%;" />

`sys_filesize()`

<img src=".\pics\proj2\流程图\3系统调用\sys_filesize.png" alt="sys_open流程图" style="zoom:50%;" />

`sys_read()`

<img src=".\pics\proj2\流程图\3系统调用\sys_read.png" alt="sys_open流程图" style="zoom:50%;" />

`sys_write()`

<img src=".\pics\proj2\流程图\3系统调用\sys_write.png" alt="sys_open流程图" style="zoom:50%;" />

`sys_seek()`

<img src=".\pics\proj2\流程图\3系统调用\sys_seek.png" alt="sys_seek流程图" style="zoom:100%;" />

`sys_tell()`

<img src=".\pics\proj2\流程图\3系统调用\sys_tell.png" alt="sys_tell流程图" style="zoom:100%;" />

`sys_close()`

<img src=".\pics\proj2\流程图\3系统调用\sys_close.png" alt="sys_close流程图" style="zoom:100%;" />

### 5.4 Denying Writes to Executables
`file_deny_write()`

<img src=".\pics\proj2\流程图\4拒绝写入可执行文件\file_deny_write.png" alt="file_deny_write流程图" style="zoom:100%;" />

`file_allow_write()`

<img src=".\pics\proj2\流程图\4拒绝写入可执行文件\file_allow_write.png" alt="file_allow_write流程图" style="zoom:100%;" />


## 6. 设计文档问题解答

### 6.1 Argument Passing

#### 6.1.1 Data Structures

A1: Copy here the declaration of each new or changed `struct' or
`struct' member, global or static variable, `typedef', or enumeration.  Identify the purpose of each in 25 words or less.

> 在进程终止消息任务的设计中，我们向`threads/thread.c`的`struct thread`中添加一个`int st_exit`，用于存储进程退出时的状态。
>
> 在参数传递的设计中，我们没有增加新的数据结构。

#### 6.1.2 Algorithms

A2: Briefly describe how you implemented argument parsing.  How do you arrange for the elements of argv[] to be in the right order? How do you avoid overflowing the stack page?

> 经过对代码内容的阅读可知，`process_execute`提供了`file_name`这一参数，在该参数中包含了文件名和参数，要传递参数，则首先要对其进行参数分离，所以我们引入了`strtok_r()`这个线程安全的字符串分割函数对其进行分割。为保证合适的分割时机，我们继续观察了pintos的用户进程执行逻辑：在`process_execute`创建进程之后，pintos并不会直接运行可执行文件，而是进入了`start_process()`对interrupt frame完成初始化，并运行了用于分配内存的`load()`函数，在`load()`函数之后才会对栈进行初始化`setup_stack()`。所以，我们在`load()`函数执行之后再进行字符串分割和压栈的操作。为达成传递参数的目的，我们额外创建了`push_argument()`函数，用于处理字符串并将其压入栈中，具体的逻辑可以参考对`push_argument()`函数的功能说明。
>
> 为保证对于参数的正确解析，我们先使用`strtok_r()`对输入值进行分割，得到`argc`的值然后把参数存储到`argv[]`数组中，然后将`argc`和`argv[]`作为参数传递给`push_argument()`，将其逆序压入栈中，这样就可以保证参数的顺序是正确的。

#### 6.1.3 Rationale

A3: Why does Pintos implement strtok_r() but not strtok()?

> 因为当在多线程调用`strtok()`的情况下，`strtok()`随时都可以被中断，它所使用的全局变量导致了最终输出的结果是不确定的。而`strtok_r()`所使用的`save_ptr()`指针则可以保证在多线程调用的情况下的线程安全性。

A4: In Pintos, the kernel separates commands into a executable name and arguments.  In Unix-like systems, the shell does this separation.  Identify at least two advantages of the Unix approach.

> 1. Unix方式比较可靠，即使shell崩溃了也不会对内核乃至整个系统的运行造成巨大影响。
> 2. Unix方式减少了内核的不必要的功能，使得维护起来更加方便更加模块化。
> 3. shell所具有的权限较小，即使遭受恶意攻击也不会过度影响到系统安全，具有很高的安全性。

### 6.2 System Calls

#### 6.2.1 Data Structures

B1: Copy here the declaration of each new or changed `struct' or
`struct' member, global or static variable, `typedef', or enumeration.  Identify the purpose of each in 25 words or less.

创建了一个child的struct，用于描述子进程。

```c
struct child
{
    tid_t tid;                           /* tid of the thread */
    bool isrun;                          /* whether the child's thread is run successfully */
    struct list_elem child_elem;         /* list of children */
    struct semaphore sema;               /* semaphore to control waiting */
    int store_exit;                      /* the exit status of child thread */
};
```

在thread结构体中新加入了一些成员，用于更好的在系统调用中使用thread

```c
struct list childs;                 /* The list of childs */
struct child * thread_child;        /* Store the child of this thread */
int st_exit;                        /* Exit status */
struct semaphore sema;              /* Control the child process's logic, finish parent waiting for child */
bool success;                       /* Judge whehter the child's thread execute successfully */
struct thread* parent;              /* Parent thread of the thread */
```

我们还创建一个结构体thread_file用于描述进程打开的文件

```c
struct thread_file{
    int fd;
    struct file* file;
    struct list_elem file_elem;
};
```

B2: Describe how file descriptors are associated with open files. Are file descriptors unique within the entire OS or just within a single process?



#### 6.2.2 Algorithms

B3: Describe your code for reading and writing user data from the kernel.



B4: Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel.  What is the least and the greatest possible number of inspections of the page table (e.g. calls to pagedir_get_page()) that might result?  What about for a system call that only copies 2 bytes of data?  Is there room for improvement in these numbers, and how much?



B5: Briefly describe your implementation of the "wait" system call and how it interacts with process termination.



B6: Any access to user program memory at a user-specified address can fail due to a bad pointer value.  Such accesses must cause the process to be terminated.  System calls are fraught with such accesses, e.g. a "write" system call requires reading the system call number from the user stack, then each of the call's three arguments, then an arbitrary amount of user memory, and any of these can fail at any point.  This poses a design and error-handling problem: how do you best avoid obscuring the primary function of code in a morass of error-handling?  Furthermore, when an error is detected, how do you ensure that all temporarily allocated resources (locks, buffers, etc.) are freed?  In a few paragraphs, describe the strategy or strategies you adopted for managing these issues.  Give an example.




#### 6.2.3 Synchronization

B7: The "exec" system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading.  How does your code ensure this?  How is the load success/failure status passed back to the thread that calls "exec"?



B8: Consider parent process P with child process C.  How do you ensure proper synchronization and avoid race conditions when P calls wait(C) before C exits?  After C exits?  How do you ensure that all resources are freed in each case?  How about when P terminates without waiting, before C exits?  After C exits?  Are there any special cases?



#### 6.2.4 Rationale

B9: Why did you choose to implement access to user memory from the kernel in the way that you did?



B10: What advantages or disadvantages can you see to your design for file descriptors?



B11: The default tid_t to pid_t mapping is the identity mapping. If you changed it, what advantages are there to your approach?
