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



### 3.4 Denying Writes to Executables



## 4. 相关数据结构分析说明

### 4.1 Process Termination Messages



### 4.2 Argument Passing



### 4.3 System Calls



### 4.4 Denying Writes to Executables



## 5. 相关函数流程图分析说明

### 5.1 Process Termination Messages



### 5.2 Argument Passing



### 5.3 System Calls



### 5.4 Denying Writes to Executables



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