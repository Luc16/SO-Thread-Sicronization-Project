#include "lightswitch.h"

LightSwitch::LightSwitch(){
    pthread_mutex_init(&m_mutex, nullptr);
}

LightSwitch::~LightSwitch(){
    pthread_mutex_destroy(&m_mutex);
}


void LightSwitch::lock(sem_t* semaphore){
    pthread_mutex_lock(&m_mutex);
        m_counter++;
        if (m_counter == 1) {
            sem_wait(semaphore);
        }
    pthread_mutex_unlock(&m_mutex);
}

void LightSwitch::unlock(sem_t* semaphore){
    pthread_mutex_lock(&m_mutex);
        m_counter--;
        if (m_counter == 0) {
            sem_post(semaphore);
        }
    pthread_mutex_unlock(&m_mutex);
}

