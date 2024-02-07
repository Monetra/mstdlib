/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Monetra Technologies, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "m_config.h"

#include <mstdlib/mstdlib_thread.h>
#include "m_thread_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_thread_rwlock {
    M_thread_mutex_t *mutex;
    M_thread_cond_t  *rd_cond; /* readers wait on this CV */
    M_thread_cond_t  *wr_cond; /* writers wait on this CV */
    ssize_t           lockcnt; /* lock count of RW lock >0 read locks  == 0 no locks  < 0 wr locks */
    size_t            num_rd;  /* number of readers waiting for RW lock */
    size_t            num_wr;  /* number of writers waiting for RW lock */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_rwlock_emu_lock_read(M_thread_rwlock_t *rwlock)
{
    M_thread_mutex_lock(rwlock->mutex);

    /* if write lock, or writer waiting on lock,
     * wait for the writer to complete and send a signal
     * to resume operations ... not forgetting to let
     * the writer know a reader is waiting
     */

    while (rwlock->lockcnt < 0 || rwlock->num_wr > 0) {
        /* increment so that writer knows reader is waiting */
        rwlock->num_rd++;

        /* cond_wait unlocks the mutex, and waits until a condition on
         * rd_cond occurs, then relocks the mutex
         */
        M_thread_cond_wait(rwlock->rd_cond, rwlock->mutex);

        /* decrement, we're no longer waiting */
        rwlock->num_rd--;
    }

    /* increment, positive number suggests read locks */
    rwlock->lockcnt++;
    M_thread_mutex_unlock(rwlock->mutex);
}

static void M_thread_rwlock_emu_lock_write(M_thread_rwlock_t *rwlock)
{
    M_thread_mutex_lock(rwlock->mutex);

    /* If there are locks, wait until a signal (condition) is
     * sent, and not forget to mark a writer as waiting */

    while (rwlock->lockcnt != 0) {
        rwlock->num_wr++;
        M_thread_cond_wait(rwlock->wr_cond, rwlock->mutex);
        rwlock->num_wr--;
    }

    /* < 0 is wr lock */
    rwlock->lockcnt = -1;
    M_thread_mutex_unlock(rwlock->mutex);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_thread_rwlock_t *M_thread_rwlock_emu_create(void)
{
    M_thread_rwlock_t *rwlock;

    rwlock = M_malloc_zero(sizeof(*rwlock));
    rwlock->mutex   = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
    rwlock->rd_cond = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
    rwlock->wr_cond = M_thread_cond_create(M_THREAD_CONDATTR_NONE);

    return rwlock;
}

void M_thread_rwlock_emu_destroy(M_thread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return;

    M_thread_mutex_destroy(rwlock->mutex);
    M_thread_cond_destroy(rwlock->rd_cond);
    M_thread_cond_destroy(rwlock->wr_cond);

    M_free(rwlock);
}

M_bool M_thread_rwlock_emu_lock(M_thread_rwlock_t *rwlock, M_thread_rwlock_type_t type)
{
    if (rwlock == NULL)
        return M_FALSE;

    if (type == M_THREAD_RWLOCK_TYPE_READ) {
        M_thread_rwlock_emu_lock_read(rwlock);
    } else {
        M_thread_rwlock_emu_lock_write(rwlock);
    }

    return M_TRUE;
}

M_bool M_thread_rwlock_emu_unlock(M_thread_rwlock_t *rwlock)
{
    size_t                 num_wr = 0;
    size_t                 num_rd = 0;
    ssize_t                lockcnt;

    if (rwlock == NULL)
        return M_FALSE;

    M_thread_mutex_lock(rwlock->mutex);
    lockcnt = rwlock->lockcnt;

    if (lockcnt > 0) {
        /* Read locks decrement this when unlocking */
        rwlock->lockcnt--;
        /* last reader releasing lock */
        if (rwlock->lockcnt == 0) {
            /* number of waiting writers */
            num_wr = rwlock->num_wr;
        }
        M_thread_mutex_unlock(rwlock->mutex);

        /* If writers are waiting send a signal to the next writer in line */
        if (num_wr > 0) {
            M_thread_cond_signal(rwlock->wr_cond);
        }
    } else if (lockcnt < 0) {
        /* Since only one writer can hold a lock, set it to unlocked */
        rwlock->lockcnt = 0;
        num_rd = rwlock->num_rd;
        num_wr = rwlock->num_wr;
        M_thread_mutex_unlock(rwlock->mutex);

        /* if there is another writer waiting, wake up the next one */
        if (num_wr > 0) {
            M_thread_cond_signal(rwlock->wr_cond);
        } else if (num_rd > 0) {
            /* otherwise, if there are readers waiting, wake THEM ALL up */
            M_thread_cond_broadcast(rwlock->rd_cond);
        }
    }

    return M_TRUE;
}
