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
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    ASSERT_NE(e.this_thread, nullptr);
}

TEST(ExecutionManager_Core, initTerminate) {
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    e.terminate();
    ASSERT_EQ(e.this_thread, nullptr);
}

TEST(ExecutionManager_Core, createThread) {
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    Thread * t = e.createThread(test, nullptr);
    e.joinThread(t);
}

TEST(ExecutionManager_Core, createThreadNoJoin) {
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    Thread * t = e.createThread(test, nullptr);
}

TEST(ExecutionManager_Core, createThreadTerminate) {
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    Thread * t = e.createThread(test, nullptr);
    e.terminate();
}

TEST(ExecutionManager_Core, createThreadSuspended) {
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    Thread * t = e.createThreadSuspended(test, nullptr);
    e.joinThread(t);
}

TEST(ExecutionManager_Core, createThreadSuspendedNoJoin) {
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    Thread * t = e.createThreadSuspended(test, nullptr);
}

TEST(ExecutionManager_Core, createThreadSuspendedTerminate) {
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    Thread * t = e.createThreadSuspended(test, nullptr);
    e.terminate();
}

void SEGFAULT() {
    int * s = nullptr;
    *s = 0;
}

TEST(ExecutionManager_Core, Intentional_Segmentation_Fault) {
    ExecutionManager e(true, ExecutionManager::DEFAULT_STACK_SIZE);
    Thread * t = e.createThread(SEGFAULT);
    e.terminate();
}
