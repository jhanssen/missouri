#ifndef UTIL_H
#define UTIL_H

#ifdef OS_Windows
# include <Windows.h>
#else
# include <pthread.h>
#endif

class Thread
{
public:
    Thread();
    virtual ~Thread();

    void start();
    void join();

    static void yield();

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
    friend class WaitCondition;
};

class WaitCondition
{
public:
    WaitCondition();
    ~WaitCondition();

    void wait(Mutex* mutex, int timeout = -1);
    void signal();
    void broadcast();

private:
#ifdef OS_Windows
    CONDITION_VARIABLE cond;
#else
    pthread_cond_t cond;
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

inline WaitCondition::WaitCondition()
{
#ifdef OS_Windows
    InitializeConditionVariable(&cond);
#else
    pthread_cond_init(&cond, 0);
#endif
}

inline WaitCondition::~WaitCondition()
{
    // apparently there's no need to delete CONDITION_VARIABLEs on Windows
#ifndef OS_Windows
    pthread_cond_destroy(&cond);
#endif
}

inline void WaitCondition::wait(Mutex* mutex, int timeout)
{
#ifdef OS_Windows
    SleepConditionVariableCS(&cond, &mutex->mutex, timeout == -1 ? INFINITE : timeout);
#else
    if (timeout == -1)
        pthread_cond_wait(&cond, &mutex->mutex);
    else {
        timespec spec;
        spec.tv_sec = timeout / 1000;
        spec.tv_nsec = (timeout % 1000) * 1000000;
        pthread_cond_timedwait(&cond, &mutex->mutex, &spec);
    }
#endif
}

inline void WaitCondition::signal()
{
#ifdef OS_Windows
    WakeConditionVariable(&cond);
#else
    pthread_cond_signal(&cond);
#endif
}

inline void WaitCondition::broadcast()
{
#ifdef OS_Windows
    WakeAllConditionVariable(&cond);
#else
    pthread_cond_broadcast(&cond);
#endif
}

#endif
