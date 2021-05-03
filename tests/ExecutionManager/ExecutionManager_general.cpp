//
// Created by brothercomplex on 6/11/19.
//

#include <ExecutionManager/ExecutionManager.hpp>
#include <gtest/gtest.h>

int test(void * arg) {
    LOG_INFO("boo!");
    return 0;
}

TEST(ExecutionManager_Core, store_and_invoke) {
    std::function<int()> invoke;
    invoke = [&] { LOG_INFO("EXECUTE"); return test(nullptr); };
    int x = ExecutionManager::invoker(&invoke);
    LOG_INFO("%d", x);
}

TEST(ExecutionManager_Core, init) {
    ExecutionManager e;
    ASSERT_NE(e.this_thread, nullptr);
}

TEST(ExecutionManager_Core, initTerminate) {
    ExecutionManager e;
    e.terminate();
    ASSERT_EQ(e.this_thread, nullptr);
}

TEST(ExecutionManager_Core, createThread) {
    ExecutionManager e;
    Thread * t = e.createThread(test, nullptr);
    e.joinThread(t);
}

TEST(ExecutionManager_Core, createThreadNoJoin) {
    ExecutionManager e;
    Thread * t = e.createThread(test, nullptr);
}

TEST(ExecutionManager_Core, createThreadTerminate) {
    ExecutionManager e;
    Thread * t = e.createThread(test, nullptr);
    e.terminate();
}

TEST(ExecutionManager_Core, createThreadSuspended) {
    ExecutionManager e;
    Thread * t = e.createThreadSuspended(test, nullptr);
    e.joinThread(t);
}

TEST(ExecutionManager_Core, createThreadSuspendedNoJoin) {
    ExecutionManager e;
    Thread * t = e.createThreadSuspended(test, nullptr);
}

TEST(ExecutionManager_Core, createThreadSuspendedTerminate) {
    ExecutionManager e;
    Thread * t = e.createThreadSuspended(test, nullptr);
    e.terminate();
}

//int tf(void * a) {
//    return 1;
//}
//
//TEST(ExecetionManager_Core, Single_Thread) {
//    ExecutionManager * e = new ExecutionManager();
//    setExecutionManager(e);
//    e->debug = true;
//    int a = 5;
//    LOG_INFO("executing thread");
//    Thread * x = threadNew(tf, &a);
//    LOG_INFO("executed thread");
//    LOG_INFO("joining thread");
//    threadJoin(x);
//    LOG_INFO("joined thread");
//}