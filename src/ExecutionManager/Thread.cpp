//
// Created by totalcontrol on 2/05/21.
//

#include <ExecutionManager/Thread.hpp>
#include <sys/wait.h> // SIGCHLD
#include <unistd.h> // getpid
#include <sched.h>
#include <cerrno>
#include <Log.hpp>
#include <ExecutionManager/ExecutionManager.hpp>

Thread::Thread() {};
ThreadCreationInfo::ThreadCreationInfo() {};

ThreadCreationInfo::ThreadCreationInfo(size_t stack_size, int (*main)(void *), void *arg) {
    this->stack_size = stack_size;
    this->main = main;
    this->arg = arg;
}

ThreadCreationInfo::~ThreadCreationInfo() {
    if (DEBUG) LOG_INFO("destructor called for ThreadCreationInfo");
}

void ThreadCreationInfo::setDebug(bool value) {
    DEBUG = value;
    thread.stack.setDebug(value);
}

int ThreadCreationInfo::clone() {
    thread.stack.alloc(stack_size);
    cloneArgs.fn = main;
    cloneArgs.child_stack = thread.stack.top;
    cloneArgs.arg = arg;
    cloneArgs.flags =
            // the calling process and the child process share the same file descriptor table.
            // Any file descriptor created by the calling process or by the child process is also
            // valid in the other process.
            // Similarly, if one of the processes closes a file descriptor, or changes its
            // associated flags (using the fcntl(2) F_SETFD operation), the other process is also
            // affected.
            // If a process sharing a file descriptor table calls execve(2), its file descriptor
            // table is duplicated (unshared)
            CLONE_FILES
            |
            // the caller and the child process share the same filesystem information.
            // This includes the root of the filesystem, the current working directory, and the
            // umask.
            // Any call to chroot(2), chdir(2), or umask(2) performed by the calling process or the
            // child process also affects the other process.
            CLONE_FS
            |
            // the new process shares an I/O context with the calling process.
            //
            // The I/O context is the I/O scope of the disk scheduler (i.e., what the I/O scheduler
            // uses to model scheduling of a process's I/O).
            // If processes share the same I/O context, they are treated as one by the I/O
            // scheduler.
            // As a consequence, they get to share disk time.
            // For some I/O schedulers, if two processes share an I/O context, they will be allowed
            // to interleave their disk access.
            // If several threads are doing I/O on behalf of the same process
            // (aio_read(3), for instance), they should employ CLONE_IO to get better I/O
            // performance.
            //
            // If the kernel is not configured with the CONFIG_BLOCK option, this flag is a no-op.
            CLONE_IO
            |
            // If CLONE_PTRACE is specified, and the calling process is being traced, then trace the
            // child also (see ptrace(2)).
            CLONE_PTRACE
            |
            // the  calling  process and the child process share the same table of signal handlers.
            // If the calling process or child process calls sigaction(2) to change the behavior
            // associated with a signal, the behavior is changed in the other process as well.
            // However, the calling process and child processes  still  have  distinct signal masks
            // and sets of pending signals.
            // So, one of them may block or unblock signals using sigprocmask(2) without affecting
            // the other process.
            CLONE_SIGHAND
            |
            // the calling process should be sent the signal SIGCHLD when the child process
            // terminates, which lets the calling process use the wait(2) call to wait for the child
            // process to exit.
            SIGCHLD
            |
            // the calling process and the child process run in the same memory space.
            // In particular, memory writes performed by the calling process or by the child process
            // are also visible in the other process.
            // Moreover, any memory mapping or unmapping performed with mmap(2) or munmap(2) by the
            // child or calling process also affects the other process.
            CLONE_VM;
    int pid = ::clone(cloneArgs.fn, cloneArgs.child_stack, cloneArgs.flags, cloneArgs.arg);
    int saved_errno = errno;
    thread.pid = pid;
    if (DEBUG) LOG_INFO("thread with pid: %d has created a thread with pid: %d", getpid(), pid);
    errno = saved_errno;
    if (pid == -1) {
        perror("internal_error (clone)");
        thread.stack.dealloc();
    }
    return pid;
}

void ThreadCreationInfo::wrap(int (*wrappedMain)(void *)) {
    wrapper.self = this;
    wrapper.main = main;
    wrapper.arg = arg;

    main = wrappedMain;
    arg = &wrapper;
}

ThreadCreationInfo * ThreadCreationInfo::unwrap(void *wrapped) {
    Wrapper * wrapper = static_cast<Wrapper *>(wrapped);
    ThreadCreationInfo * threadCreationInfo = wrapper->self;
    threadCreationInfo->main = wrapper->main;
    threadCreationInfo->arg = wrapper->arg;
    return threadCreationInfo;
}

const char * boolToString(bool value) {
    return value ? "true" : "false";
}