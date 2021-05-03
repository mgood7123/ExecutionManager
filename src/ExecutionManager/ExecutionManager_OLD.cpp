/*

// NOTE: users should use threadNew to create new threads, everything else in this will be internal

// TODO: implement tid lookup and return obtain
// NOTE: this will be tricky as the thread needs to be recycled when its return code is obtained
// if another thread starts with the same id due to pid recycling, and the previous thread return code has not been obtained
// then this is UB, it is unclear how this could be worked around given intentional abuse of pid recycling

// we cannot manually obtain the return code via the table index as the system releases all resources upon thread completion
*/

//
// Created by brothercomplex on 28/10/19.
//

#include <ExecutionManager_OLD.hpp>
#include <thread_safe/Log.hpp>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <sched.h>
#include "../../include/ExecutionManager/Thread.hpp"


const char * boolToString(bool value) {
    return value ? "true" : "false";
}

int ExecutionManager_OLD_instance(void * arg);

void executionManager_SetExecutionManager_OLD(thread_safePointer<class ExecutionManager_OLD *> executionManager) {
    executionManager_Current.store(executionManager.load());
}

void setExecutionManager_OLD(thread_safePointer<class ExecutionManager_OLD *> executionManager) {
    executionManager_SetExecutionManager_OLD(executionManager);
}

thread_safePointer<class ExecutionManager_OLD *> executionManager_GetExecutionManager_OLD() {
    return executionManager_Current;
}

thread_safePointer<class ExecutionManager_OLD *> getExecutionManager_OLD() {
    return executionManager_GetExecutionManager_OLD();
}

void executionManager_Terminate() {
    LOG_INFO("terminating execution manager");
    executionManager_Current.load()->close.store(true);
    ExecutionManager_OLD::threadJoin(executionManager_Current.load()->thread);
    LOG_INFO("terminated execution manager");
}

int suspended(void * arg) {
    thread_safePointer<struct QTS *> q { static_cast<struct QTS *>(arg) };
    assert(q.load() != nullptr);
    assert(q.load()->t.load() != nullptr);
    q.load()->t.load()->died.store(false);
    q.load()->t.load()->killable.store(false);
    thread_safe<long int> pid { getpid() };
    if (executionManager_Current.load()->debug.load()) LOG_INFO("STOPPING THREAD %d", pid.load());
    q.load()->t.load()->status.store(Thread::StatusList.STOPPED);
    kill(pid.load(), SIGSTOP);
    q.load()->t.load()->status.store(Thread::StatusList.RUNNING);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("RESUMED THREAD %d", pid.load());
    assert(q.load()->f.load() != nullptr);
    assert(q.load()->arg.load() != nullptr);
    assert(q.load()->t.load() != nullptr);

    // main
    thread_safe<int> R { q.load()->f.load()(q.load()->arg.load()) };

    q.load()->t.load()->killable.store(true);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("while (q.load()->t.load()->killable) THREAD %d", pid.load());
    while (q.load()->t.load()->killable.load()) {
        sleep(1);
        LOG_INFO("waiting for killable to be false");
    }
    LOG_INFO("killable is false");
    if (executionManager_Current.load()->debug.load()) LOG_INFO("STOPPING THREAD %d, R = %d", pid.load(), R.load());
    q.load()->t.load()->status.store(Thread::StatusList.STOPPED);
    kill(pid.load(), SIGSTOP);
    q.load()->t.load()->status.store(Thread::StatusList.RUNNING);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("RESUMED THREAD %d, R = %d, TERMINATING THREAD %d", pid.load(), R.load(), pid.load());
    return R.load();
}

thread_safePointer<Thread *> threadNew_(thread_safePointer<struct QTS *> q) {
    if (executionManager_Current.load()->debug.load()) LOG_INFO("threadNew_");
    if (executionManager_Current.load()->debug.load()) LOG_INFO("q.load()->t = new Thread()");
    q.load()->t.store(new Thread());
    if (executionManager_Current.load()->debug.load()) LOG_INFO("clone q.load()->t = %p", q.load()->t.load());
    q.load()->t.load()->suspend.store(q.load()->createSuspended.load());
    q.load()->t.load()->stack.load().alloc(q.load()->stack_size.load() == 0 ? 4096 : q.load()->stack_size.load());
    q.load()->t.load()->pid.store(clone(suspended, q.load()->t.load()->stack.load().top.load(), CLONE_VM|SIGCHLD, q.load()));
    if (executionManager_Current.load()->debug.load()) LOG_INFO("clone q.load()->t.load()->pid.load() = %d", q.load()->t.load()->pid.load());
    if (q.load()->t.load()->pid.load() == -1) {
        if (executionManager_Current.load()->debug.load()) perror("clone");
        q.load()->t.load()->stack.load().free();
    } else {
        if (q.load()->push.load()) {
            executionManager_Current.load()->QTS_VECTOR.load().push_back(q);
            if (executionManager_Current.load()->debug.load()) LOG_INFO("returning 1 %p", executionManager_Current.load()->QTS_VECTOR.load().back().load()->t.load());
            return executionManager_Current.load()->QTS_VECTOR.load().back().load()->t;
        } else {
            if (executionManager_Current.load()->debug.load()) LOG_INFO("returning 2 %p", q.load()->t.load());
            return q.load()->t;
        }
    }
    if (executionManager_Current.load()->debug.load()) LOG_INFO("returning 3 nullptr");
    thread_safePointer<Thread *> NULLPOINTER { nullptr };
    return NULLPOINTER;
}

thread_safePointer<Thread *> QINIT() {
    if (executionManager_Current.load()->isRunning.load()) return executionManager_Current.load()->thread;
    if (executionManager_Current.load()->debug.load()) LOG_INFO("initializing QINIT()");
    executionManager_Current.load()->isRunning.store(true);
    // we cannot use threadnew_() as killable will never be triggered

    if (executionManager_Current.load()->debug.load()) LOG_INFO("q.load()->t = new Thread()");
    thread_safePointer<struct QTS *> q { new struct QTS };
    q.load()->t.store(new Thread());
    if (executionManager_Current.load()->debug.load()) LOG_INFO("q.load()->t = %p", q.load()->t.load());
    q.load()->t.load()->stack.load().alloc(4096);
    auto fn = ExecutionManager_OLD_instance;
    void *child_stack = q.load()->t.load()->stack.load().top.load();
    int flags = CLONE_VM|SIGCHLD;
    void *arg = executionManager_Current.load();
    if (executionManager_Current.load()->debug.load()) LOG_INFO("calling clone(%p, %p, %zu, %p)", fn, child_stack, flags, arg);
    q.load()->t.load()->pid.store(clone(fn, child_stack, flags, arg));
    if (q.load()->t.load()->pid.load() == -1) {
        if (executionManager_Current.load()->debug.load()) perror("clone");
        q.load()->t.load()->stack.load().free();
        if (executionManager_Current.load()->debug.load()) {
            LOG_INFO("failed to initialize QINIT()");
            LOG_INFO("returning 3 nullptr");
        }
        thread_safePointer<Thread *> NULLPOINTER { nullptr };
        return NULLPOINTER;
    } else {
        kill(q.load()->t.load()->pid.load(), SIGSTOP);
        if (executionManager_Current.load()->debug.load()) LOG_INFO("vector stored %s", boolToString(executionManager_Current.load()->QTS_VECTOR.stored));
        executionManager_Current.load()->QTS_VECTOR.load().push_back(q);
        executionManager_Current.load()->thread.store(executionManager_Current.load()->QTS_VECTOR.load().back().load()->t.load());
        kill(q.load()->t.load()->pid.load(), SIGCONT);
        atexit(executionManager_Terminate);
        if (executionManager_Current.load()->debug.load()) LOG_INFO("initialized QINIT()");
        return executionManager_Current.load()->thread;
    }
}

thread_safePointer<void *> ExecutionManager_OLD::sendRequest(int request, thread_safePointer<void *> package) {
    executionManager_Current.load()->REQUEST_request.store(request);
    executionManager_Current.load()->REQUEST_package.store(package.load());
    if (executionManager_Current.load()->debug.load()) LOG_INFO("uploading");
    executionManager_Current.load()->REQUEST_upload.store(true);
    executionManager_Current.load()->REQUEST_uploadNotComplete.store(true);
    executionManager_Current.load()->REQUEST_processingNotComplete.store(true);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("while(executionManager_Current.load()->REQUEST.uploadNotComplete.load()) start");
    while(executionManager_Current.load()->REQUEST_uploadNotComplete.load());
    if (executionManager_Current.load()->debug.load()) LOG_INFO("while(executionManager_Current.load()->REQUEST.uploadNotComplete.load()) done");
    if (executionManager_Current.load()->debug.load()) LOG_INFO("while(executionManager_Current.load()->REQUEST.processingNotComplete.load()) start");
    while(executionManager_Current.load()->REQUEST_processingNotComplete.load());
    if (executionManager_Current.load()->debug.load()) LOG_INFO("while(executionManager_Current.load()->REQUEST.processingNotComplete.load()) done");
    if (executionManager_Current.load()->debug.load()) LOG_INFO("upload complete");
    if (executionManager_Current.load()->debug.load()) LOG_INFO("executionManager_Current.load()->REQUEST.reply = %p", executionManager_Current.load()->REQUEST_reply.load());
    return executionManager_Current.load()->REQUEST_reply;
}

int staticHandleRequest(thread_safePointer<class ExecutionManager_OLD *> arg) {
    thread_safePointer<class ExecutionManager_OLD *> R { arg.load() } ; //reinterpret_cast<class thread_safePointer<class ExecutionManager_OLD *> >(arg);
    if (R.load()->REQUEST_request.load() != -1) {
        assert(R.load()->REQUEST_package.load() != nullptr);
        switch (R.load()->REQUEST_request.load()) {
            case ExecutionManager_OLD::REQUEST_ID.threadCreate:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadCreate");
                if (executionManager_Current.load()->debug.load()) LOG_INFO("adding new thread");
                R.load()->REQUEST_reply.store(threadNew_(R.load()->REQUEST_package.recast<QTS *>()).load());
                assert(R.load()->REQUEST_reply.load() != nullptr);
                R.load()->REQUEST_package.store(nullptr);
                if (executionManager_Current.load()->debug.load()) LOG_INFO("R.load()->REQUEST.reply = %p", R.load()->REQUEST_reply.load());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("added new thread");
                if (executionManager_Current.load()->debug.load()) LOG_INFO("waiting for thread stop");
                R.load()->threadWaitUntilStopped(R.load()->REQUEST_reply.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("thread has stopped");
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadCreate");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadJoin:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadJoin");
                R.load()->threadJoin(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadJoin");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadJoinUntilStopped:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadJoinUntilStopped");
                R.load()->threadJoinUntilStopped(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadJoinUntilStopped");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadWaitUntilStopped:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadWaitUntilStopped");
                R.load()->threadWaitUntilStopped(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadWaitUntilStopped");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadWaitUntilRunning:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadWaitUntilRunning");
                R.load()->threadWaitUntilRunning(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadWaitUntilRunning");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadWaitUntilRunningAndJoin:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadWaitUntilRunningAndJoin");
                R.load()->threadWaitUntilRunningAndJoin(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadWaitUntilRunningAndJoin");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadWaitUntilRunningAndJoinUntilStopped:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadWaitUntilRunningAndJoinUntilStopped");
                R.load()->threadWaitUntilRunningAndJoinUntilStopped(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadWaitUntilRunningAndJoinUntilStopped");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadPause:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadPause");
                R.load()->threadPause(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadPause");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadResume:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadResume");
                R.load()->threadResume(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadResume");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadResumeAndJoin:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadResumeAndJoin");
                R.load()->threadResumeAndJoin(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadResumeAndJoin");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadTerminate:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadTerminate");
                R.load()->threadTerminate(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: threadTerminate");
                break;
            case ExecutionManager_OLD::REQUEST_ID.threadKill:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request: threadKill");
                R.load()->threadKill(R.load()->REQUEST_package.recast<Thread *>());
                if (executionManager_Current.load()->debug.load()) LOG_INFO("request complete: tthreadKill");
                break;
            default:
                if (executionManager_Current.load()->debug.load()) LOG_INFO("unknown request");
                abort();
                break;
        }
    }
    R.load()->REQUEST_request.store(-1);
    R.load()->REQUEST_stack.load().free();
    if (executionManager_Current.load()->debug.load()) LOG_INFO("R.load()->REQUEST.processing.store(false);");
    R.load()->REQUEST_processing.store(false);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("R.load()->REQUEST.processingNotComplete.store(false);");
    R.load()->REQUEST_processingNotComplete.store(false);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("return 0");
    return 0;
}

void ExecutionManager_OLD::handleRequest(thread_safePointer<class ExecutionManager_OLD *> EX) {
    if (executionManager_Current.load()->debug.load()) LOG_INFO("REQUEST.processing.store(true);");
    REQUEST_processing.store(true);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("REQUEST.upload.store(false);");
    REQUEST_upload.store(false);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("REQUEST.uploadNotComplete.store(false);");
    REQUEST_uploadNotComplete.store(false);
    staticHandleRequest(EX);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("return");
}

thread_safe<bool> ExecutionManager_OLD::errorChecking(thread_safePointer<struct QTS *> q) {
    thread_safe<bool> r { false };
    if (q.load()->t == nullptr) r.store(true); // skip non existant threads
    if (q.load()->t.load()->died.load()) r.store(true); // skip died threads
    return r;
}

int ExecutionManager_OLD_instance(void * arg) {
    // without class, we get error: ‘ExecutionManager_OLD’ does not name a type
    thread_safePointer<class ExecutionManager_OLD *> ExecutionManager_OLD { reinterpret_cast<class ExecutionManager_OLD * >(arg) };

    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("starting Execution Manager");

    pid_t self = ExecutionManager_OLD.load()->thread.load()->pid.load();
    assert(self == ExecutionManager_OLD.load()->thread.load()->pid.load());
    assert(ExecutionManager_OLD.load()->QTS_VECTOR.load().size() != 0);
    assert(ExecutionManager_OLD.load()->QTS_VECTOR.load()[0] != nullptr);
    assert(self == ExecutionManager_OLD.load()->QTS_VECTOR.load()[0].load()->t.load()->pid.load());
    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("EXECUTION MANAGER!");
    thread_safe<bool> e { false };
    while(true) {
        if (e.load()) break;
        thread_safe<int> i { 0 };
        while(i.load() < ExecutionManager_OLD.load()->QTS_VECTOR.load().size()) {
            if (ExecutionManager_OLD.load()->close.load()) {
                if (i.load() == 0) {
                    if (ExecutionManager_OLD.load()->QTS_VECTOR.load().size() == 1) {
                        e.store(true);
                        break;
                    }
                }
                thread_safePointer<struct QTS *> q = ExecutionManager_OLD.load()->QTS_VECTOR.load()[i.load()];
                LOG_INFO("terminating thread %d", q.load()->t.load()->pid.load());
                threadTerminate(q.load()->t.load());
                continue;
            };
            if (ExecutionManager_OLD.load()->REQUEST_upload.load() && !ExecutionManager_OLD.load()->REQUEST_processing.load()) {
                if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("if (ExecutionManager_OLD.load()->REQUEST.upload && !ExecutionManager_OLD.load()->REQUEST.processing)");
                if (executionManager_Current.load()->debug.load()) LOG_INFO("handling request");
                ExecutionManager_OLD.load()->handleRequest(ExecutionManager_OLD);
                if (executionManager_Current.load()->debug.load()) LOG_INFO("handled request");
            }
            if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("ALIVE");

            // skip self
            if (i.load() == 0) { i++; continue; }

            // obtain a thread
            thread_safePointer<struct QTS *> q = ExecutionManager_OLD.load()->QTS_VECTOR.load()[i.load()];
//            threadInfo(q.load()->t);
            // error checking
//            if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("error checking");
            if (ExecutionManager_OLD.load()->errorChecking(q.load()).load()) { LOG_INFO("error occured, skipping"); i++; continue; }

            assert(q.load()->t.load()->pid.load() != self);

            // check if thread is running
//            if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("threadIsRunning(q.load()->t)");
            if (ExecutionManager_OLD.load()->threadIsRunning(q.load()->t.load())) {
//                if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("threadIsRunning(q.load()->t) true");
//                if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("THREAD %d running", q.load()->t.load()->pid.load());
                // cleanup thread if needed
                if (q.load()->t.load()->killable.load()) {
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("THREAD %d is killable", q.load()->t.load()->pid.load());
                    q.load()->t.load()->killable.store(false);
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("THREAD %d threadWaitUntilStopped", q.load()->t.load()->pid.load());
                    ExecutionManager_OLD.load()->threadWaitUntilStopped(q.load()->t);
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("THREAD %d stopped", q.load()->t.load()->pid.load());
                    ExecutionManager_OLD.load()->threadResumeAndJoin(q.load()->t);
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("THREAD %d has DIED: return code: %d", q.load()->t.load()->pid.load(), q.load()->t.load()->returnCode.load());
                    q.load()->t.load()->died.store(true);
                } else {
//                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("THREAD %d is not killable", q.load()->t.load()->pid.load());
                }
//                if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("q.load()->t.load()->reapable");
                if (q.load()->t.load()->reapable.load()) {
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("q.load()->t.load()->reapable true");
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("reaping pid %d", q.load()->t.load()->pid.load());
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("erasing ExecutionManager_OLD.load()->QTS_VECTOR.load()[%d]", i.load());
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("ExecutionManager_OLD.load()->QTS_VECTOR.load().size() is %d",
                           ExecutionManager_OLD.load()->QTS_VECTOR.load().size());
                    ExecutionManager_OLD.load()->QTS_VECTOR.load().erase(ExecutionManager_OLD.load()->QTS_VECTOR.load().begin() + i.load());
                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("ExecutionManager_OLD.load()->QTS_VECTOR.load().size() is %d",
                           ExecutionManager_OLD.load()->QTS_VECTOR.load().size());
                    q.load()->t.load()->reaped.store(true);
                } else {
//                    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("q.load()->t.load()->reapable false");
                }
            } else {
//                if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("threadIsRunning(q.load()->t) false");
            }
            i++;
        }
    }
    if (ExecutionManager_OLD.load()->debug.load()) LOG_INFO("exiting Execution Manager");
    ExecutionManager_OLD.load()->QTS_VECTOR.load().clear();
    executionManager_Current.store(nullptr);
    return 0;
}

thread_safePointer<Thread *> ExecutionManager_OLD::threadNew(thread_safe<bool> createSuspended, size_t stack_size, thread_safe<std::function<int(void*)>> f, thread_safePointer<void *> arg) {
    thread_safePointer<struct QTS *> qts { new struct QTS };
    qts.load()->createSuspended.store(createSuspended.load());
    qts.load()->stack_size.store(stack_size);
    qts.load()->f.store(f.load());
    qts.load()->arg.store(arg.load());
    qts.load()->push.store(true);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("thread_safePointer<Thread *> x = static_cast<Thread*>(sendRequest(ExecutionManager_OLD::REQUEST_ID.threadCreate, qts)); starting");
    thread_safePointer<Thread *> x { sendRequest(ExecutionManager_OLD::REQUEST_ID.threadCreate, qts.load()).recast<Thread *>() };
    if (executionManager_Current.load()->debug.load()) LOG_INFO("thread_safePointer<Thread *> x = static_cast<Thread*>(sendRequest(ExecutionManager_OLD::REQUEST_ID.threadCreate, qts)); complete");
    if (executionManager_Current.load()->debug.load()) LOG_INFO("x = %p", x.load());
    assert(x.load() != nullptr);
    if (executionManager_Current.load()->debug.load()) LOG_INFO("createSuspended: %s", boolToString(createSuspended.load()));
    if (!createSuspended.load()) {
        if (executionManager_Current.load()->debug.load()) LOG_INFO("resuming from threadNew");
        sendRequest(ExecutionManager_OLD::REQUEST_ID.threadResume, x.load());
        if (executionManager_Current.load()->debug.load()) LOG_INFO("resumed from threadNew");
    }
    return x;
}

void ExecutionManager_OLD::threadJoin(thread_safePointer<Thread *> t) {
    if (executionManager_Current.load()->debug.load()) LOG_INFO("joining");
    if (t.load()->pid.load() != -1) {
        siginfo_t info;
        for (;;) {
            if (waitid(P_PID, t.load()->pid.load(), &info, WEXITED | WSTOPPED) == -1) {
                if (executionManager_Current.load()->debug.load()) perror("waitid");
                return;
            }
            thread_safe<bool> con { false };
            switch (info.si_code) {
                case CLD_EXITED:
                    t.load()->status.store(Thread::StatusList.EXITED);
                    t.load()->returnCode.store(info.si_status);
                    t.load()->reapable.store(true);
                    break;
                case CLD_KILLED:
                    t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                    t.load()->signal.store(info.si_status);
                    t.load()->reapable.store(true);
                    break;
                case CLD_DUMPED:
                    t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                    t.load()->signal.store(info.si_status);
                    t.load()->core_dumped.store(true);
                    t.load()->reapable.store(true);
                    break;
                case CLD_STOPPED:
                    con.store(true);
                    break;
                default:
                    t.load()->status.store(Thread::StatusList.UNDEFINED);
                    break;
            }
            if (!con.load()) break;
        }
    }
}

void ExecutionManager_OLD::threadJoinUntilStopped(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WEXITED|WSTOPPED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_EXITED:
                t.load()->status.store(Thread::StatusList.EXITED);
                t.load()->returnCode.store(info.si_status);
                t.load()->reapable.store(true);
                break;
            case CLD_KILLED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->reapable.store(true);
                break;
            case CLD_DUMPED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->core_dumped.store(true);
                t.load()->reapable.store(true);
                break;
            case CLD_STOPPED:
                t.load()->status.store(Thread::StatusList.STOPPED);
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

void ExecutionManager_OLD::threadWaitUntilStopped(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        siginfo_t info;
        if (executionManager_Current.load()->debug.load()) LOG_INFO("waiting for pid %d", t.load()->pid.load());
        if (waitid(P_PID, t.load()->pid.load(), &info, WSTOPPED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        if (executionManager_Current.load()->debug.load()) LOG_INFO("waited for pid %d", t.load()->pid.load());
        switch (info.si_code) {
            case CLD_KILLED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->reapable.store(true);
                break;
            case CLD_DUMPED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->core_dumped.store(true);
                t.load()->reapable.store(true);
                break;
            case CLD_STOPPED:
                t.load()->status.store(Thread::StatusList.STOPPED);
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

void ExecutionManager_OLD::threadWaitUntilRunning(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WCONTINUED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_CONTINUED:
                t.load()->status.store(Thread::StatusList.RUNNING);
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

void ExecutionManager_OLD::threadWaitUntilRunningAndJoin(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WCONTINUED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_CONTINUED:
                t.load()->status.store(Thread::StatusList.RUNNING);
                for (;;) {
                    if (waitid(P_PID, t.load()->pid.load(), &info, WEXITED | WSTOPPED) == -1) {
                        if (executionManager_Current.load()->debug.load()) perror("waitid");
                        return;
                    }
                    thread_safe<bool> con { false };
                    switch (info.si_code) {
                        case CLD_EXITED:
                            t.load()->status.store(Thread::StatusList.EXITED);
                            t.load()->returnCode.store(info.si_status);
                            t.load()->reapable.store(true);
                            break;
                        case CLD_KILLED:
                            t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                            t.load()->signal.store(info.si_status);
                            t.load()->reapable.store(true);
                            break;
                        case CLD_DUMPED:
                            t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                            t.load()->signal.store(info.si_status);
                            t.load()->core_dumped.store(true);
                            t.load()->reapable.store(true);
                            break;
                        case CLD_STOPPED:
                            con.store(true);
                            break;
                        default:
                            t.load()->status.store(Thread::StatusList.UNDEFINED);
                            break;
                    }
                    if (!con.load()) break;
                }
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

void ExecutionManager_OLD::threadWaitUntilRunningAndJoinUntilStopped(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WCONTINUED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_CONTINUED:
                t.load()->status.store(Thread::StatusList.RUNNING);
                if (waitid(P_PID, t.load()->pid.load(), &info, WEXITED|WSTOPPED) == -1) {
                    if (executionManager_Current.load()->debug.load()) perror("waitid");
                    return;
                }
                switch (info.si_code) {
                    case CLD_EXITED:
                        t.load()->status.store(Thread::StatusList.EXITED);
                        t.load()->returnCode.store(info.si_status);
                        t.load()->reapable.store(true);
                        break;
                    case CLD_KILLED:
                        t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                        t.load()->signal.store(info.si_status);
                        t.load()->reapable.store(true);
                        break;
                    case CLD_DUMPED:
                        t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                        t.load()->signal.store(info.si_status);
                        t.load()->core_dumped.store(true);
                        t.load()->reapable.store(true);
                        break;
                    case CLD_STOPPED:
                        t.load()->status.store(Thread::StatusList.STOPPED);
                        break;
                    default:
                        t.load()->status.store(Thread::StatusList.UNDEFINED);
                        break;
                }
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

bool ExecutionManager_OLD::threadIsStopped(thread_safePointer<Thread *> t) {
    return t.load()->status.load() == Thread::StatusList.STOPPED;
}

bool ExecutionManager_OLD::threadIsRunning(thread_safePointer<Thread *> t) {
    return t.load()->status.load() == Thread::StatusList.RUNNING;
}

void ExecutionManager_OLD::threadPause(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        if (kill(t.load()->pid.load(), SIGSTOP) == -1) if (executionManager_Current.load()->debug.load()) perror("kill");
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WSTOPPED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_KILLED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->reapable.store(true);
                break;
            case CLD_DUMPED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->core_dumped.store(true);
                t.load()->reapable.store(true);
                break;
            case CLD_STOPPED:
                t.load()->status.store(Thread::StatusList.STOPPED);
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

void ExecutionManager_OLD::threadResume(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        if (kill(t.load()->pid.load(), SIGCONT) == -1) if (executionManager_Current.load()->debug.load()) perror("kill");
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WCONTINUED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_CONTINUED:
                t.load()->status.store(Thread::StatusList.RUNNING);
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

void ExecutionManager_OLD::threadResumeAndJoin(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        if (kill(t.load()->pid.load(), SIGCONT) == -1) if (executionManager_Current.load()->debug.load()) perror("kill");
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WCONTINUED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_CONTINUED:
                t.load()->status.store(Thread::StatusList.RUNNING);
                for (;;) {
                    if (waitid(P_PID, t.load()->pid.load(), &info, WEXITED | WSTOPPED) == -1) {
                        if (executionManager_Current.load()->debug.load()) perror("waitid");
                        return;
                    }
                    thread_safe<bool> con { false };
                    switch (info.si_code) {
                        case CLD_EXITED:
                            t.load()->status.store(Thread::StatusList.EXITED);
                            t.load()->returnCode.store(info.si_status);
                            t.load()->reapable.store(true);
                            break;
                        case CLD_KILLED:
                            t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                            t.load()->signal.store(info.si_status);
                            t.load()->reapable.store(true);
                            break;
                        case CLD_DUMPED:
                            t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                            t.load()->signal.store(info.si_status);
                            t.load()->core_dumped.store(true);
                            t.load()->reapable.store(true);
                            break;
                        case CLD_STOPPED:
                            con.store(true);
                            break;
                        default:
                            t.load()->status.store(Thread::StatusList.UNDEFINED);
                            break;
                    }
                    if (!con.load()) break;
                }
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

void ExecutionManager_OLD::threadTerminate(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        if (kill(t.load()->pid.load(), SIGTERM) == -1) if (executionManager_Current.load()->debug.load()) perror("kill");
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WSTOPPED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_KILLED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->terminated_by_user.store(true);
                break;
            case CLD_DUMPED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->core_dumped.store(true);
                t.load()->terminated_by_user.store(true);
                break;
            case CLD_STOPPED:
                t.load()->status.store(Thread::StatusList.STOPPED);
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

void ExecutionManager_OLD::threadKill(thread_safePointer<Thread *> t) {
    if (t.load()->pid.load() != -1) {
        if (kill(t.load()->pid.load(), SIGKILL) == -1) if (executionManager_Current.load()->debug.load()) perror("kill");
        siginfo_t info;
        if (waitid(P_PID, t.load()->pid.load(), &info, WSTOPPED) == -1) {
            if (executionManager_Current.load()->debug.load()) perror("waitid");
            return;
        }
        switch (info.si_code) {
            case CLD_KILLED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->killed_by_user.store(true);
                break;
            case CLD_DUMPED:
                t.load()->status.store(Thread::StatusList.KILLED_BY_SIGNAL);
                t.load()->signal.store(info.si_status);
                t.load()->core_dumped.store(true);
                t.load()->killed_by_user.store(true);
                break;
            case CLD_STOPPED:
                t.load()->status.store(Thread::StatusList.STOPPED);
                break;
            default:
                t.load()->status.store(Thread::StatusList.UNDEFINED);
                break;
        }
    }
}

int helperVoid(void * v) {
    reinterpret_cast<void (*)()>(v)();
    return 0;
}

Thread * threadNew(bool createSuspended, size_t stack_size, std::function<int(void*)> f, void * arg) {
    if (executionManager_Current.load() == nullptr) {
        LOG_ALWAYS_FATAL("error: an execution manager must be set, use setExecutionManager_OLD(YourExecutionManager_OLD); to set one");
        abort();
    }
    QINIT();
    return executionManager_Current.load()->threadNew(createSuspended, stack_size, f, arg).load();
}

Thread * threadNew(void (*f)()) {
    return threadNew(0, helperVoid, reinterpret_cast<void*>(f));
}
Thread * threadNew(std::function<int(void*)> f, void * arg) {
    return threadNew(false, 0, f, arg);
}
Thread * threadNew(size_t stack_size, void (*f)()) {
    return threadNew(stack_size, helperVoid, reinterpret_cast<void*>(f));
}
Thread * threadNew(size_t stack_size, std::function<int(void*)> f, void * arg) {
    return threadNew(false, stack_size, f, arg);
}
Thread * threadNewSuspended(void (*f)()) {
    return threadNew(0, helperVoid, reinterpret_cast<void*>(f));
}

Thread * threadNewSuspended(std::function<int(void*)> f, void * arg) {
    return threadNew(false, 0, f, arg);
}

Thread * threadNewSuspended(size_t stack_size, void (*f)()) {
    return threadNew(stack_size, helperVoid, reinterpret_cast<void*>(f));
}

Thread * threadNewSuspended(size_t stack_size, std::function<int(void*)> f, void * arg) {
    return threadNew(false, stack_size, f, arg);
}

void threadJoin(Thread * t) {
    while(!t->died.load());
//    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadJoin, t);
}

void threadJoinUntilStopped(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadJoinUntilStopped, t);
}

void threadWaitUntilStopped(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadWaitUntilStopped, t);
}

void threadWaitUntilRunning(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadWaitUntilRunning, t);
}

void threadWaitUntilRunningAndJoin(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadWaitUntilRunningAndJoin, t);
}

void threadWaitUntilRunningAndJoinUntilStopped(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadWaitUntilRunningAndJoinUntilStopped, t);
}

bool threadIsStopped(Thread * t) {
    return ExecutionManager_OLD::threadIsStopped(t);
}

bool threadIsRunning(Thread * t) {
    return ExecutionManager_OLD::threadIsRunning(t);
}

void threadPause(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadPause, t);
}

void threadResume(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadResume, t);
}

void threadResumeAndJoin(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadResumeAndJoin, t);
}

void threadTerminate(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadTerminate, t);
}

void threadKill(Thread * t) {
    ExecutionManager_OLD::sendRequest(ExecutionManager_OLD::REQUEST_ID.threadKill, t);
}

const char * statusToString(const int & status) {
    switch (status) {
        case Thread::StatusList.UNDEFINED:
            return "UNDEFINED";
        case Thread::StatusList.RUNNING:
            return "running";
        case Thread::StatusList.STOPPED:
            return "stopped";
        case Thread::StatusList.DEAD:
            return "dead";
        case Thread::StatusList.EXITED:
            return "exited";
        case Thread::StatusList.KILLED_BY_SIGNAL:
            return "killed by signal";
        default:
            return "unknown";
    }
}

void threadInfo(Thread * t) {
    LOG_INFO("THREAD INFO:");
    thread_safe<bool> threadExists { t != nullptr };
    LOG_INFO("    thread exists = %s", boolToString(threadExists.load()));
    if (threadExists.load()) {
        LOG_INFO("    STACK INFO:");
        thread_safe<bool> stackExists { t->stack.load().stack != nullptr };
        LOG_INFO("        stack exists = %s", boolToString(stackExists.load()));
        if (stackExists.load()) {
            LOG_INFO("        address = %p", t->stack.load().stack.load());
            LOG_INFO("        top = %p", t->stack.load().top.load());
            LOG_INFO("        size = %ld", t->stack.load().size.load());
            LOG_INFO("        direction = %s", t->stack.load().getStackDirectionAsString());
        }
        LOG_INFO("    pid = %d", t->pid.load());
        LOG_INFO("    killable = %s", boolToString(t->killable.load()));
        LOG_INFO("    initially suspended = %s", boolToString(t->suspend.load()));
        LOG_INFO("    dead = %s", boolToString(t->died.load()));
        LOG_INFO("    status = %s", statusToString(t->status.load()));
        LOG_INFO("    signal = %d", t->signal.load());
        LOG_INFO("    returnCode = %d", t->returnCode.load());
        LOG_INFO("    core dumped = %s", boolToString(t->core_dumped.load()));
        LOG_INFO("    reapable = %s", boolToString(t->reapable.load()));
        LOG_INFO("    reaped = %s", boolToString(t->reaped.load()));
        LOG_INFO("    terminated by user = %s", boolToString(t->terminated_by_user.load()));
        LOG_INFO("    killed by user = %s", boolToString(t->killed_by_user.load()));
    }
}
