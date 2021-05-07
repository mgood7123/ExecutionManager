//
// Created by totalcontrol on 2/05/21.
//

#ifndef EXECUTIONMANAGER_EXECUTIONMANAGER_HPP
#define EXECUTIONMANAGER_EXECUTIONMANAGER_HPP

#include <vector>
#include "Thread.hpp"
#include <functional>
#include <Log.hpp>
#include <chrono>

class ExecutionManager {
    static int instance(void * arg);

    Thread * createThread(ThreadCreationInfo * threadCreationInfo);
public:
    std::atomic<bool> DEBUG { false };

    template< class Rep, class Period, class Predicate >
    bool timeout(const std::chrono::duration<Rep, Period>& rel_time, Predicate pred) {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = start + rel_time;
        bool status = true;
        do {
            status = !pred();
            if (status == false) break;
        } while(std::chrono::high_resolution_clock::now() < end);
        return status;
    }

    static constexpr bool timed_out = true;

    std::atomic<bool> running { false };
    Thread *this_thread;
    std::vector<ThreadCreationInfo *> threads;

    ExecutionManager();
    ~ExecutionManager();

    void terminate();

    std::function<int()> invoke;

    static int invoker(void * args);

    static int wrapperThread(void * args);

    Thread * createThreadInternal(size_t stack_size, int (*main)(void *), void * arg);

    template<typename F, typename...Args> struct is_callable {
        template<typename F2, typename...Args2> static constexpr std::true_type
        test(decltype(std::declval<F2>()(std::declval<Args2>()...)) *) { return {}; }

        template<typename F2, typename...Args2> static constexpr std::false_type
        test(...) { return {}; }

        static constexpr bool value = decltype(test<F, Args...>(nullptr))::value;
    };

#define TEMPLATE_IS_CALLABLE is_callable<Function, Arguments...>::value
#define TEMPLATE_IS_RETURN_TYPE(TYPE) std::is_same<decltype(std::declval<Function&>()(std::declval<Arguments>()...)), TYPE>::value
#define TEMPLATE_IF(X) typename std::enable_if<X>::type* = nullptr
#define TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(TYPE) template<typename Function, typename... Arguments, TEMPLATE_IF(TEMPLATE_IS_CALLABLE && TEMPLATE_IS_RETURN_TYPE(TYPE))>
#define TEMPLATE_STORE_INVOKE_VOID invoke = [=] { func(parameters...); return 0; };
#define TEMPLATE_STORE_INVOKE_INT invoke = [=] { return func(parameters...); };

    TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(void)
    Thread * createThread(Function func, Arguments... parameters) {
        TEMPLATE_STORE_INVOKE_VOID
        Thread * thread = createThreadInternal(default_stack_size, invoker, &invoke);
        waitForStop(thread);
        continueThread(thread);
        return thread;
    }

    TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(int)
    Thread * createThread(Function func, Arguments... parameters) {
        TEMPLATE_STORE_INVOKE_INT
        Thread * thread = createThreadInternal(default_stack_size, invoker, &invoke);
        waitForStop(thread);
        continueThread(thread);
        return thread;
    }

    TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(void)
    Thread * createThread(size_t stack_size, Function func, Arguments... parameters) {
        TEMPLATE_STORE_INVOKE_VOID
        Thread * thread = createThreadInternal(stack_size, invoker, &invoke);
        waitForStop(thread);
        continueThread(thread);
        return thread;
    }

    TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(int)
    Thread * createThread(size_t stack_size, Function func, Arguments... parameters) {
        TEMPLATE_STORE_INVOKE_INT
        Thread * thread = createThreadInternal(stack_size, invoker, &invoke);
        waitForStop(thread);
        continueThread(thread);
        return thread;
    }

    TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(void)
    Thread * createThreadSuspended(Function func, Arguments... parameters) {
        TEMPLATE_STORE_INVOKE_VOID
        Thread * thread = createThreadInternal(default_stack_size, invoker, &invoke);
        thread->suspend = true;
        thread->suspend_started = false;
        waitForStop(thread);
        return thread;
    }

    TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(int)
    Thread * createThreadSuspended(Function func, Arguments... parameters) {
        TEMPLATE_STORE_INVOKE_INT
        Thread * thread = createThreadInternal(default_stack_size, invoker, &invoke);
        thread->suspend = true;
        thread->suspend_started = false;
        waitForStop(thread);
        return thread;
    }

    TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(void)
    Thread * createThreadSuspended(size_t stack_size, Function func, Arguments... parameters) {
        TEMPLATE_STORE_INVOKE_VOID
        Thread * thread = createThreadInternal(stack_size, invoker, &invoke);
        thread->suspend = true;
        thread->suspend_started = false;
        waitForStop(thread);
        return thread;
    }

    TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE(int)
    Thread * createThreadSuspended(size_t stack_size, Function func, Arguments... parameters) {
        TEMPLATE_STORE_INVOKE_INT
        Thread * thread = createThreadInternal(stack_size, invoker, &invoke);
        thread->suspend = true;
        thread->suspend_started = false;
        waitForStop(thread);
        return thread;
    }

#undef TEMPLATE_STORE_INVOKE_INT
#undef TEMPLATE_STORE_INVOKE_VOID
#undef TEMPLATE_EXPECT_FUNCTION_RETURN_TYPE
#undef TEMPLATE_IF
#undef TEMPLATE_IS_RETURN_TYPE
#undef TEMPLATE_IS_CALLABLE

    static void threadInfo(Thread * t);

    void joinThread(Thread *thread);
    void sendSignal(pid_t pid, Thread *thread, int signal);
    void sendSignal(Thread *thread, int signal);
    void waitForStop(Thread *thread);
    void waitForContinue(Thread *thread);
    void waitForStopOrExit(Thread *thread);
    void waitForExit(Thread *thread);
    void stopThread(Thread *thread);
    void continueThread(Thread *thread);

    static constexpr int default_stack_size = 4096;
};

#endif //EXECUTIONMANAGER_EXECUTIONMANAGER_HPP
