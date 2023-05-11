#include <pthread.h>
#include <semaphore.h>

#pragma once

class LightSwitch
{
private:
    int m_counter = 0;
    pthread_mutex_t m_mutex;
    pthread_cond_t count_is_zero;

public:
    LightSwitch();
    ~LightSwitch();

    void lock(sem_t* semaphore);
    void unlock(sem_t* semaphore);
};

