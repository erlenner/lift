# pragma once

#include <shared_mutex>
#include <cmath>
#include <assert.h>
#include "utils.h"

#define IS_SET(var,pos) ((var) & (1<<(pos)))

const int N_MUTEXES = 5;

// flags
const long POS              =  1 << 0;
const long DIR              =  1 << 1;
const long TASKS            =  1 << 2;
const long PENDING_TASKS    =  1 << 3;
const long OTHER_STATES     =  1 << 4;

inline void lock(std::shared_mutex* mutexes, long readWriteFlags, long readOnlyFlags = 0){
    for (int i=0; i<N_MUTEXES; ++i)
        if(IS_SET(readWriteFlags, i))
            mutexes[i].lock();
        else if(IS_SET(readOnlyFlags, i))
            mutexes[i].lock_shared();
}

inline void unlock(std::shared_mutex* mutexes, long readWriteFlags, long readOnlyFlags = 0){
    for (int i=N_MUTEXES-1; i>=0; --i)
        if(IS_SET(readWriteFlags, i))
            mutexes[i].unlock();
        else if(IS_SET(readOnlyFlags, i))
            mutexes[i].unlock_shared();
}

struct locker{

    locker(std::shared_mutex* mutexes, long readWriteFlags, long readOnlyFlags = 0)
    : mutexes(mutexes), readWriteFlags(readWriteFlags), readOnlyFlags(readOnlyFlags)
    { lock(mutexes, readWriteFlags, readOnlyFlags); }

    ~locker()
    { unlock(mutexes, readWriteFlags, readOnlyFlags); }

private:
    std::shared_mutex* mutexes;
    long readWriteFlags = 0;
    long readOnlyFlags = 0;
};



inline int bitPos(long flag){
    assert(flag != 0);
    int pos = 0;
    while (flag >>= 1)
        ++ pos;
    return pos;
}

template <typename T>
inline T readConcurrent(T& field, std::shared_mutex* lock_ptr){
    std::shared_lock<std::shared_mutex> lock(*lock_ptr);
    return field;
}

template <typename T>
inline void readConcurrent(T& value, T& field, std::shared_mutex* lock_ptr){
    std::shared_lock<std::shared_mutex> lock(*lock_ptr);
    value = field;
}

template <typename T>
inline T readConcurrent(T& field, std::shared_mutex* mutexes, long flag){
    std::shared_lock<std::shared_mutex> lock(mutexes[bitPos(flag)]);
    return field;
}

template <typename T>
inline void readConcurrent(T& value, T& field, std::shared_mutex* mutexes, long flag){
    std::shared_lock<std::shared_mutex> lock(mutexes[bitPos(flag)]);
    value = field;
}

template <typename T>
inline void writeConcurrent(T& field, T value, std::shared_mutex* lock_ptr){
    std::unique_lock<std::shared_mutex> lock(*lock_ptr);
    field = value;
}

template <typename T>
inline void writeConcurrent(T& field, T value, std::shared_mutex* mutexes, long flag){
    std::unique_lock<std::shared_mutex> lock(mutexes[bitPos(flag)]);
    field = value;
}

template <typename T>
inline void appendConcurrent(std::vector<T>& vector, T value, std::shared_mutex* lock_ptr){
    std::unique_lock<std::shared_mutex> lock(*lock_ptr);
    vector.push_back(value);
}

template <typename T>
inline void appendConcurrent(std::vector<T>& vector, T value, std::shared_mutex* mutexes, long flag){
    std::unique_lock<std::shared_mutex> lock(mutexes[bitPos(flag)]);
    vector.push_back(value);
}
