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



### 3.2 Argument Passing



### 3.3 System Calls

功能说明：
sys_seek

获取用户栈指针，在判断调用是否合法后取出文件描述符，在文件列表中查找文件位置，并用file_seek函数保存文件位置，详细功能如下

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

sys_tell

获取用户栈指针，在判断调用是否合法后取出文件描述符，在文件列表中查找文件位置，如果找到，将结果返回到eax，未找到则将eax置为-1，详细功能如下

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

sys_close

获取用户栈指针，在判断调用是否合法后取出文件描述符，在文件列表中查找文件位置，如果找到，说明文件打开，用file_close函数关闭文件，并将文件从线程列表中移除，详细功能如下

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
功能说明：
file.c

file_deny_write:

通过改变file中deny_write的bool值为true使其在打开后不可写

同时还将inode中deny_write_node的值加1，以便后续allow_write判断

file_allow_write:通过改变file中deny_write的bool值为false使文件在关闭后恢复可写

同时还将inode中deny_write_node的值减1

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

inode.c

inode_deny_write:

将deny_write_cnt+1,同时保证deny_write_cnt<=open_cnt，保证逻辑关系为：在打开文件后禁止写入

inode_allow_write:

将deny_write_cnt-1,同时保证deny_write_cnt > 0，而且deny_write_cnt<=open_cnt，保证逻辑关系为：在关上文件后恢复可写入，而且原来文件状态是打开且禁止写入的

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



### 4.2 Argument Passing



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
```



### 4.4 Denying Writes to Executables



## 5. 相关函数流程图分析说明

### 5.1 Process Termination Messages



### 5.2 Argument Passing



### 5.3 System Calls

sys_remove()：

<img src=".\pics\proj2\流程图\3系统调用\sys_remove.png" alt="sys_remove流程图" style="zoom:50%;" />

sys_open()：

<img src=".\pics\proj2\流程图\3系统调用\sys_open.png" alt="sys_open流程图" style="zoom:50%;" />

sys_filesize()：

<img src=".\pics\proj2\流程图\3系统调用\sys_filesize.png" alt="sys_open流程图" style="zoom:50%;" />

sys_read()：

<img src=".\pics\proj2\流程图\3系统调用\sys_read.png" alt="sys_open流程图" style="zoom:50%;" />

sys_write()：

<img src=".\pics\proj2\流程图\3系统调用\sys_write.png" alt="sys_open流程图" style="zoom:50%;" />

sys_seek()：

<img src=".\pics\proj2\流程图\3系统调用\sys_seek.png" alt="sys_seek流程图" style="zoom:50%;" />

sys_tell()：

<img src=".\pics\proj2\流程图\3系统调用\sys_tell.png" alt="sys_tell流程图" style="zoom:50%;" />

sys_close()：

<img src=".\pics\proj2\流程图\3系统调用\sys_close.png" alt="sys_close流程图" style="zoom:50%;" />

### 5.4 Denying Writes to Executables
file_deny_write()：

<img src=".\pics\proj2\流程图\4拒绝写入可执行文件\file_deny_write.png" alt="file_deny_write流程图" style="zoom:50%;" />

file_allow_write()：

<img src=".\pics\proj2\流程图\4拒绝写入可执行文件\file_allow_write.png" alt="file_allow_write流程图" style="zoom:50%;" />


## 6. 设计文档问题解答

### 6.1 Argument Passing

#### 6.1.1 Data Structures

A1: Copy here the declaration of each new or changed `struct' or
`struct' member, global or static variable, `typedef', or enumeration.  Identify the purpose of each in 25 words or less.



#### 6.1.2 Algorithms

A2: Briefly describe how you implemented argument parsing.  How do you arrange for the elements of argv[] to be in the right order? How do you avoid overflowing the stack page?



#### 6.1.3 Rationale

A3: Why does Pintos implement strtok_r() but not strtok()?



A4: In Pintos, the kernel separates commands into a executable name and arguments.  In Unix-like systems, the shell does this separation.  Identify at least two advantages of the Unix approach.



### 6.2 System Calls

#### 6.2.1 Data Structures

B1: Copy here the declaration of each new or changed `struct' or
`struct' member, global or static variable, `typedef', or enumeration.  Identify the purpose of each in 25 words or less.

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
