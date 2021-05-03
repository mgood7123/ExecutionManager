//
// Created by brothercomplex on 6/11/19.
//

#include <stack/stack.hpp>
#include <gtest/gtest.h>

TEST(Stack_Core, Basic) {
    Stack s;
    ASSERT_EQ(s.stack, nullptr);
    ASSERT_EQ(s.top, nullptr);
    ASSERT_EQ(s.size, 0);
    ASSERT_EQ(s.direction, 0);
    s.alloc(100);
    ASSERT_NE(s.stack, nullptr);
    ASSERT_NE(s.top, nullptr);
    ASSERT_EQ(s.size, 100);
    ASSERT_NE(s.direction, 0);
    s.dealloc();
    ASSERT_EQ(s.stack, nullptr);
    ASSERT_EQ(s.top, nullptr);
    ASSERT_EQ(s.size, 0);
    ASSERT_EQ(s.direction, 0);
}