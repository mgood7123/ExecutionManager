#ifndef STACK_STACK_HPP
#define STACK_STACK_HPP

#include <cstddef>

class Stack {
    int getStackDirection(int *addr);
    bool DEBUG = false;
public:
    void setDebug(bool value);
    static constexpr int DIRECTION_UP = 1;
    static constexpr int DIRECTION_DOWN = 2;

    Stack(std::size_t sz);
    ~Stack();

    Stack();
    Stack(const Stack&) = delete;
    Stack(Stack&&) = delete;
    Stack& operator=(const Stack&) = delete;
    Stack& operator=(Stack&&) = delete;

    void alloc(std::size_t sz);
    void dealloc();

    char * stack = nullptr; // start of stack buffer
    char * top = nullptr; // end of stack buffer
    std::size_t size = 0;
    int direction = 0;

    int getStackDirection();
    const char *getStackDirectionAsString();
};

#endif //STACK_STACK_HPP