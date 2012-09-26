#include "Util.h"

#ifdef OS_Windows
DWORD WINAPI threadStart(LPVOID lpParam)
{
    Thread* t = static_cast<Thread*>(lpParam);
    t->run();
    return 0;
}
#else
void* threadStart(void* arg)
{
    Thread* t = static_cast<Thread*>(arg);
    t->run();
    return 0;
}
#endif

void Thread::start()
{
#ifdef OS_Windows
    thread = CreateThread(0, 0, threadStart, this, 0, 0);
#else
    pthread_create(&thread, 0, threadStart, this);
#endif
}

void Thread::join()
{
#ifdef OS_Windows
    WaitForSingleObject(thread, INFINITE);
#else
    void* ret;
    pthread_join(thread, &ret);
#endif
}
