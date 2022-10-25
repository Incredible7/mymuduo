#include "CurrentThread.h"

namespace CurrentThread {
thread_local int t_cacheadTid = 0;

void cacheTid() {
    if (t_cacheadTid == 0) {
        t_cacheadTid = static_cast<pid_t>(::syscall(SYS_gettid));
    }
}
}  // namespace CurrentThread

// 解决不同进程下，线程id相同而导致无法通信的问题
// Linux下每个进程都有一个进程ID，类型pid_t，可以由getpid()获取
// POSIX线程也有线程id，类型pthread_t，可以由
// pthread_self()获取，线程id由线程库维护。
// 但是各个进程独立，所以会有不同进程中线程号相同节的情况。pthread_self()获取的是线程ID，仅在同一个进程中保证唯一。
// Linux中的POSIX线程库实现的线程其实也是一个进程（LWP），只是该进程与主进程（启动线程的进程）共享一些资源而已，比如代码段，数据段等。
// 有时候我们可能需要知道线程的真实pid。比如进程P1要向另外一个进程P2中的某个线程发送信号时，既不能使用P2的pid，更不能使用线程的pthread
// id，而只能使用该线程的真实pid，称为tid。
// 有一个函数gettid()可以得到tid，但glibc并没有实现该函数，只能通过Linux的系统调用syscall来获取。