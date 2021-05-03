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

ThreadCreationInfo::ThreadCreationInfo(size_t stack_size, int (*main)(void *), void *arg) {
    this->stack_size = stack_size;
    this->main = main;
    this->arg = arg;
}

Thread::~Thread() {
    stack.dealloc();
}

int ThreadCreationInfo::clone() {
    thread.stack.alloc(stack_size);
    cloneArgs.fn = main;
    cloneArgs.child_stack = thread.stack.top;
    cloneArgs.flags = CLONE_VM|SIGCHLD;
    cloneArgs.arg = arg;
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