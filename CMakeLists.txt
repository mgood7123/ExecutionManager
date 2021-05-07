cmake_minimum_required(VERSION 3.4.1)

project(ExecutionManager)

include_directories(include)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT COMMAND testBuilder_build)
    add_subdirectory(testBuilder)
    testBuilder_set_current_working_directory_to_default_binary_directory()
endif()

add_subdirectory(populate_gtest)

# Now simply link against gtest or gtest_main as needed. Eg

testBuilder_add_source(Log src/Log.cpp)
testBuilder_build_shared_library(Log)

testBuilder_add_source(Stack src/stack/stack.cpp)
testBuilder_build_shared_library(Stack)
add_subdirectory(tests/stack)

testBuilder_add_source(ExecutionManager src/ExecutionManager/Thread.cpp)
testBuilder_add_source(ExecutionManager src/ExecutionManager/ExecutionManager.cpp)
testBuilder_add_library(ExecutionManager Stack)
testBuilder_add_library(ExecutionManager Log)
testBuilder_add_library(ExecutionManager atomic)
testBuilder_build_shared_library(ExecutionManager)
add_subdirectory(tests/ExecutionManager)


set(ExecutionManager_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
