//
// Created by totalcontrol on 2/05/21.
//

#ifndef EXECUTIONMANAGER_THREAD_HPP
#define EXECUTIONMANAGER_THREAD_HPP

#include <stack/stack.hpp>
#include <atomic>

class Thread {
public:
    Stack stack;
    Thread();
    Thread(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread& operator=(Thread&&) = delete;
    std::atomic<long int> pid { -1 };
    std::atomic<bool> died { false };

    // this is limited to a range of 0 to 255
    std::atomic<int> returnCode { 0 };
    std::atomic<bool> suspend { false };
    std::atomic<bool> suspend_started { false };
    std::atomic<bool> killable { false};
    std::atomic<int> status { 0 };
    std::atomic<int> signal { -1 };
    std::atomic<bool> core_dumped { false };
    std::atomic<bool> reaped { false };
    std::atomic<bool> reapable { false };
    std::atomic<bool> terminated_by_user { false };
    std::atomic<bool> killed_by_user { false };
    class StatusList {
    public:
        static constexpr int UNDEFINED = 0;
        static constexpr int RUNNING = 1;
        static constexpr int STOPPED = 2;
        static constexpr int DEAD = 3;
        static constexpr int EXITED = 4;
        static constexpr int KILLED_BY_SIGNAL = 5;
    };
};

class ThreadCreationInfo {
    bool DEBUG = false;

public:
    ThreadCreationInfo();
    ~ThreadCreationInfo();
    ThreadCreationInfo(const ThreadCreationInfo&) = delete;
    ThreadCreationInfo(ThreadCreationInfo&&) = delete;
    ThreadCreationInfo& operator=(const ThreadCreationInfo&) = delete;
    ThreadCreationInfo& operator=(ThreadCreationInfo&&) = delete;

    void setDebug(bool value);
    ThreadCreationInfo(size_t stack_size, int (*main)(void *), void *arg);

    size_t stack_size = 0;
    int (*main)(void *) = nullptr;
    void * arg = nullptr;
    void * executionManager = nullptr;

    struct Wrapper {
        size_t stack_size = 0;
        Thread * thread = nullptr;
        int (*main)(void *) = nullptr;
        void * arg = nullptr;
        ThreadCreationInfo * self;
    } wrapper;

    Thread thread;

    struct {
        int (*fn)(void *) = nullptr;
        void *child_stack = nullptr;
        int flags = 0;
        void *arg = nullptr;
    } cloneArgs;

    int clone();

    void wrap(int (*wrappedMain)(void *));

    static ThreadCreationInfo * unwrap(void *wrapped);
};

void threadInfo(Thread * t);

#endif //EXECUTIONMANAGER_THREAD_HPP
