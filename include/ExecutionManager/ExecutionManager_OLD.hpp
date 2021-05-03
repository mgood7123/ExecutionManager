//
// Created by brothercomplex on 28/10/19.
//

#ifndef THREAD_EXECUTIONMANAGER_H
#define THREAD_EXECUTIONMANAGER_H

#include <unistd.h>
#include <vector>
#include <cassert>
#include <stack>
#include "../stack/stack.hpp"

class Thread {
public:
    thread_safe<long int> pid { -1 }; // process itself may be 0, init
    thread_safe<Stack> stack { Stack() };
    thread_safe<bool> died { false };
    thread_safe<int> returnCode { 0 };
    thread_safe<bool> suspend { false };
    thread_safe<bool> killable { false};
    thread_safe<int> status { 0 };
    thread_safe<int> signal = -1;
    thread_safe<bool> core_dumped { false };
    thread_safe<bool> reaped { false };
    thread_safe<bool> reapable { false };
    thread_safe<bool> terminated_by_user { false };
    thread_safe<bool> killed_by_user { false };
    const static class {
    public:
        enum {
            UNDEFINED,
            RUNNING,
            STOPPED,
            DEAD,
            EXITED,
            KILLED_BY_SIGNAL
        };
    } StatusList; // cannot be modified, no need to thread_safeize
};

struct QTS {
    thread_safe<bool> createSuspended { false };
    thread_safe<size_t> stack_size { 0 };
    // TODO: function pointer template specilization: https://godbolt.org/z/h3kE3D
    thread_safe<std::function<int(void*)>> f { nullptr };
    thread_safePointer<void *> arg { nullptr };
    thread_safePointer<Thread *> t { nullptr };
    thread_safe<bool> push { false };
};

class ExecutionManager_OLD {
public:
    thread_safe<bool> debug { false };
    thread_safe<bool> close { false };

    thread_safePointer<Thread *> thread { nullptr };
    thread_safe<bool> isRunning { false };
    thread_safe<std::vector<thread_safePointer<struct QTS *>>> QTS_VECTOR { std::vector<thread_safePointer<struct QTS *>>() };
    thread_safe<bool> errorChecking(thread_safePointer<struct QTS *> q);
    static thread_safePointer<void *> sendRequest(int request, thread_safePointer<void *> package);
    void handleRequest(thread_safePointer<ExecutionManager_OLD *> EX);

    thread_safe<int> REQUEST_request { -1  };
    thread_safePointer<void *> REQUEST_package { nullptr };
    thread_safe<bool> REQUEST_upload { false };
    thread_safe<bool> REQUEST_uploadNotComplete { false };
    thread_safe<bool> REQUEST_processing { false };
    thread_safe<bool> REQUEST_processingNotComplete { false };
    thread_safePointer<void *> REQUEST_reply = { nullptr };
    thread_safe<Stack> REQUEST_stack = { Stack() };

    static const class {
        public:
            enum {
                threadCreate,
                threadJoin,
                threadJoinUntilStopped,
                threadWaitUntilStopped,
                threadWaitUntilRunning,
                threadWaitUntilRunningAndJoin,
                threadWaitUntilRunningAndJoinUntilStopped,
                threadPause,
                threadResume,
                threadResumeAndJoin,
                threadTerminate,
                threadKill
            };
    } REQUEST_ID;

    // threads
    thread_safePointer<Thread *> threadNew(thread_safe<size_t> stack_size, thread_safe<std::function<int(void*)>> f, thread_safePointer<void *> arg);
    thread_safePointer<Thread *> threadNew(thread_safe<std::function<int(void*)>> f, thread_safePointer<void *> arg);
    thread_safePointer<Thread *> threadNew(thread_safe<size_t> stack_size, thread_safe<std::function<void()>> f);
    thread_safePointer<Thread *> threadNew(thread_safe<std::function<void()>> f);
    thread_safePointer<Thread *> threadNew(thread_safe<bool> createSuspended, size_t stack_size, thread_safe<std::function<int(void*)>> f, thread_safePointer<void *> arg);
    thread_safePointer<Thread *> threadNewSuspended(thread_safe<size_t> stack_size, thread_safe<std::function<int(void*)>> f, thread_safePointer<void *> arg);
    thread_safePointer<Thread *> threadNewSuspended(thread_safe<std::function<int(void*)>> f, thread_safePointer<void *> arg);
    thread_safePointer<Thread *> threadNewSuspended(thread_safe<size_t> stack_size, thread_safe<std::function<void()>> f);
    thread_safePointer<Thread *> threadNewSuspended(thread_safe<std::function<void()>> f);
    static void threadJoin(thread_safePointer<Thread *> t);
    static void threadJoinUntilStopped(thread_safePointer<Thread *> t);
    static void threadWaitUntilStopped(thread_safePointer<Thread *> t);
    static void threadWaitUntilRunning(thread_safePointer<Thread *> t);
    static void threadWaitUntilRunningAndJoin(thread_safePointer<Thread *> t);
    static void threadWaitUntilRunningAndJoinUntilStopped(thread_safePointer<Thread *> t);
    static bool threadIsRunning(thread_safePointer<Thread *> t);
    static bool threadIsStopped(thread_safePointer<Thread *> t);
    static void threadPause(thread_safePointer<Thread *> t);
    static void threadResume(thread_safePointer<Thread *> t);
    static void threadResumeAndJoin(thread_safePointer<Thread *> t);
    static void threadTerminate(thread_safePointer<Thread *> t);
    static void threadKill(thread_safePointer<Thread *> t);
};



void executionManager_Terminate();

thread_safePointer<ExecutionManager_OLD *> executionManager_Current;

void executionManager_SetExecutionManager_OLD(thread_safePointer<ExecutionManager_OLD *> executionManager);
thread_safePointer<ExecutionManager_OLD *> executionManager_GetExecutionManager_OLD();

void setExecutionManager_OLD(thread_safePointer<ExecutionManager_OLD *> executionManager);
thread_safePointer<ExecutionManager_OLD *> getExecutionManager_OLD();

Thread * threadNew(void (*f)());
Thread * threadNew(std::function<int(void*)> f, void * arg);
Thread * threadNew(size_t stack_size, void (*f)());
Thread * threadNew(size_t stack_size, std::function<int(void*)> f, void * arg);
Thread * threadNewSuspended(void (*f)());
Thread * threadNewSuspended(std::function<int(void*)> f, void * arg);
Thread * threadNewSuspended(size_t stack_size, void (*f)());
Thread * threadNewSuspended(size_t stack_size, std::function<int(void*)> f, void * arg);

void threadJoin(Thread * t);
void threadJoinUntilStopped(Thread * t);
void threadWaitUntilStopped(Thread * t);
void threadWaitUntilRunning(Thread * t);
void threadWaitUntilRunningAndJoin(Thread * t);
void threadWaitUntilRunningAndJoinUntilStopped(Thread * t);
bool threadIsRunning(Thread * t);
bool threadIsStopped(Thread * t);
void threadPause(Thread * t);
void threadResume(Thread * t);
void threadResumeAndJoin(Thread * t);
void threadInfo(Thread * t);
void threadTerminate(Thread * t);
void threadKill(Thread * t);

void threadInfo(Thread * t);

#endif //THREAD_EXECUTIONMANAGER_H
