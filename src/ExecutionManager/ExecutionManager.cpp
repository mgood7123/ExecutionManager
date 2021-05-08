//
// Created by totalcontrol on 2/05/21.
//

#include <ExecutionManager/ExecutionManager.hpp>
#include <sys/wait.h>
#include <unistd.h> // sleep
#include <cstdio>
#include <Log.hpp>
#include <cerrno>
#include <optional>

extern const char * boolToString(bool value);

int ExecutionManager::instance(void *arg) {
    ExecutionManager * executionManager = reinterpret_cast<class ExecutionManager *>(arg);
    if (executionManager->DEBUG) LOG_INFO("START MAIN EXECUTOR");
    executionManager->running = true;
    while (executionManager->running) {
//        remove element from a vector
//        for (int i = 0; i < v.size(); i++) {
//            if (v[i] == element) v.erase(v.begin() + i);
//        }
    }
    if (executionManager->DEBUG) LOG_INFO("FINISH MAIN EXECUTOR");
    return 0;
}

int ExecutionManager::wrapperThread(void * args) {
    ThreadCreationInfo * threadCreationInfo = ThreadCreationInfo::unwrap(args);
    ExecutionManager * executionManager = static_cast<ExecutionManager *>(threadCreationInfo->executionManager);
    // wait for pid to be set
    while(threadCreationInfo->thread.pid == -1);
    executionManager->stopThread(&threadCreationInfo->thread);
    return threadCreationInfo->main(threadCreationInfo->arg);
}

int ExecutionManager::invoker(void *args) {
    std::function<int()> * function = static_cast<std::function<int()>*>(args);
    return function->operator()();
}

Thread * ExecutionManager::createThread(ThreadCreationInfo *threadCreationInfo) {
    threadCreationInfo->executionManager = this;
    threadCreationInfo->wrap(wrapperThread);
    if (threadCreationInfo->clone() == -1) return nullptr;
    threads.push_back(threadCreationInfo);
    return &threadCreationInfo->thread;
}

Thread *ExecutionManager::createThreadInternal(size_t stack_size, int (*main)(void *), void *arg) {
    ThreadCreationInfo * threadCreationInfo = new ThreadCreationInfo(stack_size, main, arg);
    threadCreationInfo->setDebug(DEBUG.load());
    Thread * thread = createThread(threadCreationInfo);
    if (thread == nullptr) delete threadCreationInfo;
    return thread;
}

void ExecutionManager::setDebug(bool value) {
    DEBUG = value;
}

void ExecutionManager::setDebug(Thread * thread, bool value) {
    thread->stack.setDebug(value);
}

void ExecutionManager::init() {
    this_thread_creation_info = nullptr;
    this_thread = createThread(default_stack_size, instance, this);
    if (this_thread == nullptr) {
        LOG_ALWAYS_FATAL("failed to create main execution manager thread");
    }
    this_thread_creation_info = threads[0];
    while (!running);
    if (DEBUG) LOG_INFO("EXECUTION THREAD IS RUNNING");
}

ExecutionManager::ExecutionManager() {
    init();
}

ExecutionManager::ExecutionManager(size_t default_stack_size) {
    this->default_stack_size = default_stack_size;
    init();
}

ExecutionManager::ExecutionManager(bool debug, size_t default_stack_size) {
    DEBUG = debug;
    this->default_stack_size = default_stack_size;
    init();
}

void ExecutionManager::terminate() {
    if (running) {
        running = false;
        for (ThreadCreationInfo * t : threads) {
            Thread * t_ = &t->thread;
            if (t_ != this_thread) {
                joinThread(t_);
                delete t;
            }
        }
        joinThread(this_thread);
        this_thread = nullptr;
        delete this_thread_creation_info;
        this_thread_creation_info = nullptr;
        threads.clear();
    }
}

ExecutionManager::~ExecutionManager() {
    if (DEBUG) LOG_INFO("destructor called for ExecutionManager");
    terminate();
}

const char * statusToString(const int & status) {
    switch (status) {
        case Thread::StatusList::UNDEFINED:
            return "UNDEFINED";
        case Thread::StatusList::RUNNING:
            return "running";
        case Thread::StatusList::STOPPED:
            return "stopped";
        case Thread::StatusList::DEAD:
            return "dead";
        case Thread::StatusList::EXITED:
            return "exited";
        case Thread::StatusList::KILLED_BY_SIGNAL:
            return "killed by signal";
        default:
            return "unknown";
    }
}

void ExecutionManager::threadInfo(Thread * t) {
    LOG_INFO("THREAD INFO:");
    bool threadExists = t != nullptr;
    LOG_INFO("    thread exists = %s", boolToString(threadExists));
    if (threadExists) {
        LOG_INFO("    STACK INFO:");
        bool stackExists = t->stack.stack != nullptr;
        LOG_INFO("        stack exists = %s", boolToString(stackExists));
        if (stackExists) {
            LOG_INFO("        address = %p", t->stack.stack);
            LOG_INFO("        top = %p", t->stack.top);
            LOG_INFO("        size = %ld", t->stack.size);
            LOG_INFO("        direction = %s", t->stack.getStackDirectionAsString());
        }
        LOG_INFO("    pid = %d", t->pid.load());
        LOG_INFO("    killable = %s", boolToString(t->killable));
        LOG_INFO("    initially suspended = %s", boolToString(t->suspend));
        LOG_INFO("    has started at lease once = %s", boolToString(t->suspend_started));
        LOG_INFO("    dead = %s", boolToString(t->died));
        LOG_INFO("    status = %s", statusToString(t->status));
        LOG_INFO("    signal = %d", t->signal.load());
        LOG_INFO("    returnCode = %d", t->returnCode.load());
        LOG_INFO("    core dumped = %s", boolToString(t->core_dumped));
        LOG_INFO("    reapable = %s", boolToString(t->reapable));
        LOG_INFO("    reaped = %s", boolToString(t->reaped));
        LOG_INFO("    terminated by user = %s", boolToString(t->terminated_by_user));
        LOG_INFO("    killed by user = %s", boolToString(t->killed_by_user));
    }
}

void ExecutionManager::joinThread(Thread *thread) {
    if (DEBUG) {
        LOG_INFO("JOINING");
        threadInfo(thread);
    }
    if (thread == nullptr) return;
    if (thread->suspend && !thread->suspend_started) continueThread(thread);
    switch (thread->status) {
        case Thread::StatusList::DEAD:
        case Thread::StatusList::KILLED_BY_SIGNAL:
        case Thread::StatusList::EXITED:
            return;
        default:
            waitForExit(thread);
    }
    if (DEBUG) LOG_INFO("JOINED");
}

void ExecutionManager::sendSignal(pid_t pid, Thread *thread, int signal) {
    if (thread == nullptr) return;

    if (pid == -1) {
        LOG_ERROR("internal_error (sendSignal): thread pid has been killed or is owned by a different process");
        return;
    }

    if (kill(pid, signal) == -1) {
        LOG_ERROR("internal_error (sendSignal:kill): thread pid has been killed or is owned by a different process");
        thread->status = Thread::StatusList::EXITED;
    }
}

void ExecutionManager::sendSignal(Thread *thread, int signal) {
    if (thread == nullptr) return;
    sendSignal(thread->pid.load(), thread, signal);
}

void ExecutionManager::waitForStop(Thread *thread) {
    if (thread == nullptr) return;
    switch (thread->status) {
        case Thread::StatusList::DEAD:
        case Thread::StatusList::KILLED_BY_SIGNAL:
        case Thread::StatusList::EXITED:
            return;
    }
    int pid = thread->pid.load();

    if (pid == -1) {
        LOG_ERROR("internal_error (pid): thread pid has been killed or is owned by a different process");
        return;
    }

    if (DEBUG) LOG_INFO("waiting for STOP pid %d", pid);
    int status;
    if (waitpid(pid, &status, WSTOPPED|WCONTINUED) != -1) {
        if (WIFEXITED(status)) {
            thread->status = Thread::StatusList::EXITED;
            thread->returnCode = WEXITSTATUS(status);
            thread->reapable = true;
        } else if (WIFSIGNALED(status)) {
            thread->status = Thread::StatusList::KILLED_BY_SIGNAL;
            thread->signal = WTERMSIG(status);
#ifdef WCOREDUMP
            thread->core_dumped = WCOREDUMP(status);
#endif
            thread->reapable = true;
        } else if (WIFSTOPPED(status)) {
            thread->status = Thread::StatusList::STOPPED;
            thread->signal = WSTOPSIG(status);
        } else if (WIFCONTINUED(status)) {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: STOPPED, got state: CONTINUE", pid);
            threadInfo(thread);
        } else {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: CONTINUE, got state: UNKNOWN",
                      pid);
            threadInfo(thread);
        }
    } else {
        if (errno == ECHILD) {
            LOG_ERROR(
                    "internal_error (waitpid): thread pid has been killed or is owned by a different process");
            thread->status = Thread::StatusList::EXITED;
        } else {
            perror("internal_error (waitpid)");
        }
    }
}

void ExecutionManager::waitForContinue(Thread *thread) {
    if (thread == nullptr) return;
    switch (thread->status) {
        case Thread::StatusList::DEAD:
        case Thread::StatusList::KILLED_BY_SIGNAL:
        case Thread::StatusList::EXITED:
            return;
    }
    int pid = thread->pid.load();

    if (pid == -1) {
        LOG_ERROR("internal_error (pid): thread pid has been killed or is owned by a different process");
        return;
    }

    if (DEBUG) LOG_INFO("waiting for CONTINUE pid %d", pid);
    int status;
    if (waitpid(pid, &status, WCONTINUED|WSTOPPED) != -1) {
        if (WIFEXITED(status)) {
            thread->status = Thread::StatusList::EXITED;
            thread->returnCode = WEXITSTATUS(status);
            thread->reapable = true;
        } else if (WIFSIGNALED(status)) {
            thread->status = Thread::StatusList::KILLED_BY_SIGNAL;
            thread->signal = WTERMSIG(status);
#ifdef WCOREDUMP
            thread->core_dumped = WCOREDUMP(status);
#endif
            thread->reapable = true;
        } else if (WIFCONTINUED(status)) {
            while (thread->status != Thread::StatusList::RUNNING);
        } else if (WIFSTOPPED(status)) {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: CONTINUE, got state: STOPPED",
                      pid);
            threadInfo(thread);
        } else {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: CONTINUE, got state: UNKNOWN",
                      pid);
            threadInfo(thread);
        }
    } else {
        if (errno == ECHILD) {
            LOG_ERROR(
                    "internal_error (waitpid): thread pid has been killed or is owned by a different process");
            thread->status = Thread::StatusList::EXITED;
        } else {
            perror("internal_error (waitpid)");
        }
    }
}

void ExecutionManager::waitForExit(Thread *thread) {
    if (thread == nullptr) return;
    switch (thread->status) {
        case Thread::StatusList::DEAD:
        case Thread::StatusList::KILLED_BY_SIGNAL:
        case Thread::StatusList::EXITED:
            return;
    }
    int pid = thread->pid.load();

    if (pid == -1) {
        LOG_ERROR("internal_error (waitForExit): thread pid has been killed or is owned by a different process");
        return;
    }

    if (DEBUG) LOG_INFO("waiting for EXIT pid %d", pid);
    int status;
    if (waitpid(pid, &status, WCONTINUED|WSTOPPED) != -1) {
        if (WIFEXITED(status)) {
            thread->status = Thread::StatusList::EXITED;
            thread->returnCode = WEXITSTATUS(status);
            thread->reapable = true;
        } else if (WIFSIGNALED(status)) {
            thread->status = Thread::StatusList::KILLED_BY_SIGNAL;
            thread->signal = WTERMSIG(status);
#ifdef WCOREDUMP
            thread->core_dumped = WCOREDUMP(status);
#endif
            thread->reapable = true;
        } else if (WIFCONTINUED(status)) {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: EXITED, got state: CONTINUE", pid);
            threadInfo(thread);
        } else if (WIFSTOPPED(status)) {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: EXITED, got state: STOPPED", pid);
            threadInfo(thread);
        } else {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: EXITED, got state: UNKNOWN", pid);
            threadInfo(thread);
        }
    } else {
        if (errno == ECHILD) {
            LOG_ERROR(
                    "internal_error (waitpid): thread pid has been killed or is owned by a different process");
            thread->status = Thread::StatusList::EXITED;
        } else {
            perror("internal_error (waitpid)");
        }
    }
}

void ExecutionManager::waitForStopOrExit(Thread *thread) {
    if (thread == nullptr) return;
    switch (thread->status) {
        case Thread::StatusList::DEAD:
        case Thread::StatusList::KILLED_BY_SIGNAL:
        case Thread::StatusList::EXITED:
            return;
    }
    int pid = thread->pid.load();

    if (pid == -1) {
        LOG_ERROR("internal_error (waitForStopOrExit): thread pid has been killed or is owned by a different process");
        return;
    }

    if (DEBUG) LOG_INFO("waiting for STOP OR EXIT pid %d", pid);
    int status;
    if (waitpid(pid, &status, WCONTINUED|WSTOPPED) != -1) {
        if (WIFEXITED(status)) {
            thread->status = Thread::StatusList::EXITED;
            thread->returnCode = WEXITSTATUS(status);
            thread->reapable = true;
        } else if (WIFSIGNALED(status)) {
            thread->status = Thread::StatusList::KILLED_BY_SIGNAL;
            thread->signal = WTERMSIG(status);
#ifdef WCOREDUMP
            thread->core_dumped = WCOREDUMP(status);
#endif
            thread->reapable = true;
        } else if (WIFSTOPPED(status)) {
            thread->status = Thread::StatusList::STOPPED;
            thread->signal = WSTOPSIG(status);
        } else if (WIFCONTINUED(status)) {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: STOPPED || EXITED, got state: CONTINUE", pid);
            threadInfo(thread);
        } else {
            LOG_ERROR("internal_error (waitpid: thread pid %d): expected state: STOPPED || EXITED, got state: UNKNOWN", pid);
            threadInfo(thread);
        }
    } else {
        if (errno == ECHILD) {
            LOG_ERROR(
                    "internal_error (waitpid): thread pid has been killed or is owned by a different process");
            thread->status = Thread::StatusList::EXITED;
        } else {
            perror("internal_error (waitpid)");
        }
    }
}

void ExecutionManager::stopThread(Thread *thread) {
    if (thread == nullptr) return;
    pid_t pid = getpid();
    pid_t thread_pid = thread->pid;

    if (thread_pid == -1) {
        LOG_ERROR("internal_error (stopThread): thread pid has been killed or is owned by a different process");
        return;
    }

    if (DEBUG) LOG_INFO("STOP THREAD: %d", thread_pid);
    if (pid != thread_pid) {
        sendSignal(thread_pid, thread, SIGSTOP);
        waitForStop(thread);
    } else {
        // update our status
        thread->status = Thread::StatusList::STOPPED;
        // we are sending a signal to ourself, so we must exist
        // no need to check call sendSignal for error
        if (DEBUG) LOG_INFO("STOPPING SELF: %d", thread_pid);
        kill(pid, SIGSTOP); // this needs to be waited on
        if (DEBUG) LOG_INFO("CONTINUED SELF: %d", thread_pid);
        // update our status
        if (thread->suspend) thread->suspend_started = true;
        thread->status = Thread::StatusList::RUNNING;
    }
}

void ExecutionManager::continueThread(Thread *thread) {
    if (thread == nullptr) return;
    pid_t pid = getpid();
    pid_t thread_pid = thread->pid;
    if (pid == thread_pid) {
        // we are already in a continued state
        return;
    }

    if (thread_pid == -1) {
        LOG_ERROR("internal_error (continueThread): thread pid has been killed or is owned by a different process");
        return;
    }

    if (DEBUG) LOG_INFO("CONTINUE THREAD: %d", thread_pid);
    sendSignal(thread_pid, thread, SIGCONT);
    waitForContinue(thread);
}
