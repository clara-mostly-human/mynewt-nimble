/*
 * macOS NPL: counting semaphore from a mutex and a condition variable
 * (macOS does not implement unnamed POSIX semaphores: sem_init/sem_timedwait).
 * Licensed to the Apache Software Foundation (ASF) under the Apache License 2.0,
 * like the Linux NPL this replaces.
 */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "os/os.h"
#include "nimble/nimble_npl.h"

ble_npl_error_t
ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens)
{
    if (!sem) {
        return BLE_NPL_INVALID_PARAM;
    }
    pthread_mutex_init(&sem->mtx, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->count = tokens;
    return BLE_NPL_OK;
}

ble_npl_error_t
ble_npl_sem_release(struct ble_npl_sem *sem)
{
    if (!sem) {
        return BLE_NPL_INVALID_PARAM;
    }
    pthread_mutex_lock(&sem->mtx);
    sem->count++;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mtx);
    return BLE_NPL_OK;
}

ble_npl_error_t
ble_npl_sem_pend(struct ble_npl_sem *sem, uint32_t timeout)
{
    ble_npl_error_t rc = BLE_NPL_OK;
    struct timespec deadline;

    if (!sem) {
        return BLE_NPL_INVALID_PARAM;
    }
    if (timeout != BLE_NPL_TIME_FOREVER) {
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += timeout / 1000;
        deadline.tv_nsec += (timeout % 1000) * 1000000;
        if (deadline.tv_nsec >= 1000000000) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000;
        }
    }
    pthread_mutex_lock(&sem->mtx);
    while (sem->count == 0) {
        int err;
        if (timeout == BLE_NPL_TIME_FOREVER) {
            err = pthread_cond_wait(&sem->cond, &sem->mtx);
        } else if (timeout == 0) {
            err = ETIMEDOUT;
        } else {
            err = pthread_cond_timedwait(&sem->cond, &sem->mtx, &deadline);
        }
        if (err == ETIMEDOUT) {
            rc = BLE_NPL_TIMEOUT;
            break;
        }
    }
    if (rc == BLE_NPL_OK) {
        sem->count--;
    }
    pthread_mutex_unlock(&sem->mtx);
    return rc;
}

uint16_t
ble_npl_sem_get_count(struct ble_npl_sem *sem)
{
    uint16_t count;
    assert(sem);
    pthread_mutex_lock(&sem->mtx);
    count = sem->count;
    pthread_mutex_unlock(&sem->mtx);
    return count;
}
