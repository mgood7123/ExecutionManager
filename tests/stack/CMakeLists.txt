cmake_minimum_required(VERSION 3.4.1)

testBuilder_add_source(stack_tests stack_general.cpp)
testBuilder_add_library(stack_tests gtest_main)
testBuilder_add_library(stack_tests Stack)
testBuilder_build(stack_tests EXECUTABLES)
