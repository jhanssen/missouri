#ifndef UTIL_H
#define UTIL_H

#ifdef OS_Windows
#include <Windows.h>
#else
#include <pthread.h>
#endif

class Thread
{
public:
    Thread() {}
    virtual ~Thread() {}
    
    void start();
    void join();
    
protected:
    virtual void run() = 0;
    
private:
#ifdef OS_Windows
    HANDLE thread;
    friend DWORD WINAPI threadStart(LPVOID lpParam);
#else
    pthread_t thread;
    friend void* threadStart(void* arg);
#endif
};

class Mutex
{
public:
    Mutex();
    ~Mutex();
    
    void lock();
    void unlock();

private:
#ifdef OS_Windows
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};

class MutexLocker
{
public:
    MutexLocker(Mutex* mutex) : m(mutex), locked(true) { m->lock(); }
    ~MutexLocker() { if (locked) m->unlock(); }
    
    void unlock() { if (!locked) return; locked = false; m->unlock(); }
    void relock() { if (locked) return; locked = true; m->lock(); }
    
private:
    Mutex* m;
    bool locked;
};

inline Mutex::Mutex()
{
#ifdef OS_Windows
    InitializeCriticalSection(&mutex);
#else
    pthread_mutex_init(&mutex, 0);
#endif
}

inline Mutex::~Mutex()
{
#ifdef OS_Windows
    DeleteCriticalSection(&mutex);
#else
    pthread_mutex_destroy(&mutex);
#endif
}

inline void Mutex::lock()
{
#ifdef OS_Windows
    EnterCriticalSection(&mutex);
#else
    pthread_mutex_lock(&mutex);
#endif
}

inline void Mutex::unlock()
{
#ifdef OS_Windows
    LeaveCriticalSection(&mutex);
#else
    pthread_mutex_unlock(&mutex);
#endif
}

#endif
