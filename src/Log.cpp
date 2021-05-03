//
//  Log.cpp
//  RealTimeThreadCommunicator
//
//  Created by Matthew Good on 24/3/21.
//

#include "Log.hpp"

std::mutex lock;

int LOG_INFO(const char* format) {
    lock.lock();
    int len = fprintf(stdout, "%s\n", format);
    fflush_unlocked(stdout);
    lock.unlock();
    return len;
}

int LOG_ERROR(const char* format) {
    lock.lock();
    int len = fprintf(stderr, "%s\n", format);
    fflush_unlocked(stderr);
    lock.unlock();
    return len;
}

void LOG_ALWAYS_FATAL(const char* format) {
    lock.lock();
    fprintf(stderr, "%s\n", format);
    fflush_unlocked(stderr);
    lock.unlock();
    abort();
}
