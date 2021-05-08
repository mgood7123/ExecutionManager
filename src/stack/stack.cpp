#include <stack/stack.hpp>
#include <malloc.h>
#include <sys/mman.h>
#include <new>
#include <Log.hpp>

void Stack::setDebug(bool value) {
    DEBUG = value;
}

Stack::Stack() {}

Stack::Stack(std::size_t sz) {
    alloc(sz);
}

Stack::~Stack() {
    if (DEBUG) LOG_INFO("destructor called for Stack");
    dealloc();
}

void Stack::alloc(std::size_t sz) {
    if (DEBUG) LOG_INFO("allocating stack with a size of %zu", sz);
    stack = static_cast<char *>(mmap(NULL, sz, PROT_READ | PROT_WRITE,
                                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
    if (stack == MAP_FAILED) {
        stack = nullptr;
        throw std::bad_alloc();
    }
    direction = getStackDirection();
    top = direction == DIRECTION_UP ? stack : stack + sz;
    size = sz;
}

void Stack::dealloc() {
    if (DEBUG) LOG_INFO("deallocating stack %p with a size of %zu", stack, size);
    if (stack == nullptr) return;
    munmap(stack, size);
    stack = nullptr;
    top = nullptr;
    size = 0;
    direction = 0;
}

int Stack::getStackDirection(int *addr) {
    int fun_local;
    if (addr < &fun_local) return DIRECTION_UP;
    return DIRECTION_DOWN;
}

int Stack::getStackDirection() {
    int main_local;
    return getStackDirection(&main_local);
}

const char *  Stack::getStackDirectionAsString() {
    if (stack == nullptr) direction = getStackDirection();
    if (direction == DIRECTION_DOWN) return "Stack grows downwards";
    return "Stack grows upwards";
}