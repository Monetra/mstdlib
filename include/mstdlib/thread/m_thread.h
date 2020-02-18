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

#ifndef __M_THREAD_H__
#define __M_THREAD_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_thread_common Common Threading Functions
 *  \ingroup m_thread
 *
 * Thread handling.
 *
 * System specific threading model as well as a cooperative threading model
 * is available. Cooperative should only be used on system that do not natively
 * support threads. Such as some embedded systems.
 *
 * By default threads are created in a detached state. M_thread_attr_t must be
 * used in order to have a thread created in a joinable state.
 *
 * Example:
 *
 * \code{.c}
 *     static M_uint32 count = 0;
 *
 *     static void td(void)
 *     {
 *         M_printf("Thread finished\n");
 *     }
 *
 *     static void *runner(void *arg)
 *     {
 *         M_thread_mutex_t *m = arg;
 *
 *         M_thread_mutex_lock(m);
 *         count++;
 *         M_thread_mutex_unlock(m);
 *     }
 *
 *     int main(int argc, char **argv)
 *     {
 *         M_threadid_t      t1;
 *         M_threadid_t      t2;
 *         M_thread_attr_t  *tattr;
 *         M_thread_mutex_t *m
 *
 *         M_thread_destructor_insert(td);
 *
 *         tattr   = M_thread_attr_create();
 *         M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *
 *         m = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
 *
 *         t1 = M_thread_create(tattr, runner, m);
 *         t2 = M_thread_create(tattr, runner, m);
 *
 *         M_thread_attr_destroy(tattr);
 *         M_thread_join(t1, NULL);
 *         M_thread_join(t2, NULL);
 *
 *         M_thread_mutex_destroy(m);
 *
 *         M_printf("count='%u'\n", count);
 *
 *         return 0;
 *     }
 * \endcode
 *
 */

/*! \addtogroup m_thread_common_main Thread System Initialization, Destruction, and Information
 *  \ingroup m_thread_common
 *
 *  Thread System Initialization, Destruction, and Information
 *
 *  @{
 */


/*! Thread model. */
typedef enum {
	M_THREAD_MODEL_INVALID = -1, /*!< Invalid/no model. */
	M_THREAD_MODEL_NATIVE  = 0,  /*!< System's native thread model. */
	M_THREAD_MODEL_COOP          /*!< Cooperative threads. */
} M_thread_model_t;


/*! Initialize the thread model (system).
 *
 * This should be called before any other thread function is used. This will initialize
 * the specified threading system. If this is not called before a thread function
 * is used then the native threading model will be automatically initialized.
 *
 * Only one thread model can be use at any given time.
 *
 * \param[in] model The thread model that should be used for threading.
 *
 * \return M_TRUE if the model was successfully initialized. Otherwise M_FALSE.
 *         This can fail if called after a model has already been initialized.
 */
M_API M_bool M_thread_init(M_thread_model_t model);


/*! Get the active thread model.
 *
 * \param[out] model      The active model.
 * \param[out] model_name The textual name of the model. This will provide descriptive information
 *                        such as what is the underlying native threading model.
 *
 * \return M_TRUE if a thread model is active otherwise M_FALSE.
 */
M_API M_bool M_thread_active_model(M_thread_model_t *model, const char **model_name);


/*! Adds a function to be called each time a thread finishes.
 *
 * Some libraries (OpenSSL in particular) keep their own per thread memory
 * store. This allows registering functions to be called to handle this situation.
 *
 * OpenSSL keeps a per-thread error state which must be cleaned up at thread
 * destruction otherwise it will leak memory like crazy. Wrap
 * ERR_remove_state(0); in a function that doesn't take any arugments, then
 * register the function and this problem is solved.
 *
 * Registered functions will be called in the order they were added.
 *
 * \param[in] destructor The function to register.
 *
 * \return M_TRUE if the function was added. Otherwise M_FALSE. This can fail
 *         if the function was already registered. A function can only be
 *         registered once.
 */
M_API M_bool M_thread_destructor_insert(void (*destructor)(void));


/*! Remove a function from the list of function to be called each time a thread finished.
 *
 * \param[in] destructor The function to remove.
 */
M_API void M_thread_destructor_remove(void (*destructor)(void));


/*! Thread-safe library cleanup.
 *
 *  Cleans up any initialized static/global members by the library.  Useful to
 *  be called at the end of program execution to free memory or other resources,
 *  especially if running under a leak checker such as Valgrind.
 *
 */
M_API void M_library_cleanup(void);


/*! Registers a callback to be called during M_library_cleanup().
 *
 *  There is no way to 'unregister' a callback, so it must be ensured the callback
 *  will remain valid until the end of program execution.
 *
 * \param[in] cleanup_cb Callback to call for cleanup
 * \param[in] arg        Optional argument to be passed to the callback.
 */
M_API void M_library_cleanup_register(void (*cleanup_cb)(void *arg), void *arg);


/*! Get the number of actively running threads.
 *
 * This count does not include the threads that have finished but are still joinable.
 *
 * \return Thread count.
 */
M_API size_t M_thread_count(void);


/*! Retrieve the count of CPU cores that are online and usable.  When using
 *  cooperative threading, only 1 cpu core is usable.
 *
 * \return count of cores or 0 on failure.
 */
M_API size_t M_thread_num_cpu_cores(void);


/*! @} */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_thread_common_create Thread Creation and Management
 *  \ingroup m_thread_common
 *
 *  Thread Creation and Management
 *
 *  @{
 */

/*! Thread id used to identify a thread.
 *
 * This can be compared with >, <, == and !=. */
typedef M_uintptr M_threadid_t;


/*! Thread attribute object used for thread creation. */
struct M_thread_attr;
typedef struct M_thread_attr M_thread_attr_t;




/*! Create and run a thread.
 *
 * Threads are created detached by default.
 * To create it joinable use a M_thread_attr_t and set
 * it to joinable.
 *
 * \param[in]     attr Thread creation attributes.
 * \param[in]     func The function to run.
 * \param[in,out] arg  Argument to pass to func.
 *
 * \return Threadid identifying the thread on success. Threadid will be 0 on failure.
 */
M_API M_threadid_t M_thread_create(const M_thread_attr_t *attr, void *(*func)(void *), void *arg);


/*! Wait for a thread to finish.
 *
 * Only threads that were created with the joinable attribute set to M_TRUE can be used with this function.
 *
 * \param[in]  id        The threadid to wait on.
 * \param[out] value_ptr The return value from the thread.
 *
 * \return M_TRUE if the thread was successfully joined. Otherwise M_FALSE.
 */
M_API M_bool M_thread_join(M_threadid_t id, void **value_ptr);


/*! Get the threadid of the running thread.
 *
 * \return The threadid.
 */
M_API M_threadid_t M_thread_self(void);


/*! Sleep for the specified amount of time.
 *
 * \param[in] usec Number of microseconds to sleep.
 */
M_API void M_thread_sleep(M_uint64 usec);


/*! Inform the scheduler that we want to relinquish the CPU and allow other threads to process.
 *
 * \param[in] force Force rescheduling of this thread. When M_FALSE the thread model will determine
 *                  if the thread needs to be rescheduled or not. A preemtive model will typically
 *                  ignore this call when M_FALSE and rely on its scheduler. A non-preemptive model
 *                  (COOP) will always yield.
 */
M_API void M_thread_yield(M_bool force);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a thread attribute object.
 *
 * \return Thread attribute object.
 */
M_API M_thread_attr_t *M_thread_attr_create(void);


/*! Destroy a thread attribute object.
 *
 * \param[in] attr Attribute object.
 */
M_API void M_thread_attr_destroy(M_thread_attr_t *attr);


/*! Get whether a given thread should be created joinable.
 *
 * \param[in] attr Attribute object.
 *
 * return M_TRUE if the thread should be joinable. Otherwise M_FALSE.
 */
M_API M_bool M_thread_attr_get_create_joinable(const M_thread_attr_t *attr);


/*! Get the stack size a given thread should use when created.
 *
 * This may not be used by all threading models.
 *
 * \param[in] attr Attribute object.
 *
 * return The requested stack size.
 */
M_API size_t M_thread_attr_get_stack_size(const M_thread_attr_t *attr);


/*! Minimum thread priority value */
#define M_THREAD_PRIORITY_MIN 1

/*! Normal thread priority value */
#define M_THREAD_PRIORITY_NORMAL 5

/*! Maximum thread priority value */
#define M_THREAD_PRIORITY_MAX 9

/*! Get the priority a given thread should be created with.
 *
 *  Thread priorities are 1-9, with 1 being the lowest priority and 9 being
 *  the highest.  The default value is 5.
 *
 * \param[in] attr Attribute object.
 *
 * \return The requested priority, or 0 on usage error.
 */
M_API M_uint8 M_thread_attr_get_priority(const M_thread_attr_t *attr);


/*! Set whether a given thread should be created joinable.
 *
 * The default is to create threads detached (not joinable) unless this is called
 * and set to M_TRUE.
 *
 * \param[in] attr Attribute object.
 * \param[in] val  The value to set.
 */
M_API void M_thread_attr_set_create_joinable(M_thread_attr_t *attr, M_bool val);


/*! Set the stack size a given thread should be created with.
 *
 * \param[in] attr Attribute object.
 * \param[in] val  The value to set.
 */
M_API void M_thread_attr_set_stack_size(M_thread_attr_t *attr, size_t val);


/*! Set the priority a given thread should be created with.
 *
 * \param[in] attr      Attribute object.
 * \param[in] priority  The priority to set.  Valid range is 1-9 with 1 being the
 *                      lowest priority and 9 being the highest.  The default value
 *                      is 5.  Some systems, like Linux, do not support thread scheduling
 *                      in relation to the process as a whole, but rather the system as
 *                      a whole, and therefore require RLIMIT_NICE to be configured on
 *                      the process in order to successfully increase a thread's priority
 *                      above '5'.
 * \return M_TRUE on success, or M_FALSE on usage error
 */
M_API M_bool M_thread_attr_set_priority(M_thread_attr_t *attr, M_uint8 priority);

/*! Get the currently assigned processor for thread.
 *
 * \param[in] attr   Attribute object
 * \return -1 if none specified, otherwise 0 to M_thread_num_cpu_cores()-1
 */
M_API int M_thread_attr_get_processor(const M_thread_attr_t *attr);

/*! Set the processor to assign the thread to run on (aka affinity).  The range is
 *  0 to M_thread_num_cpu_cores()-1.
 *
 * \param[in] attr         Attribute object
 * \param[in] processor_id -1 to unset prior value.
 *                         Otherwise 0 to M_thread_num_cpu_cores()-1 is the valid range.
 * \return M_TRUE on success, or M_FALSE on usage error */
M_API M_bool M_thread_attr_set_processor(M_thread_attr_t *attr, int processor_id);

/*! @} */



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*! \addtogroup m_thread_common_mutex Thread Mutexes (Locks/Critical Sections)
 *  \ingroup m_thread_common
 *
 *  Thread Mutexes (Locks/Critical Sections)
 *
 *  @{
 */

/*! Mutex. */
struct M_thread_mutex;
typedef struct M_thread_mutex M_thread_mutex_t;


/*! Mutex attributes.
 * Used for mutex creation. */
typedef enum {
	M_THREAD_MUTEXATTR_NONE      = 0,     /*!< None. */
	M_THREAD_MUTEXATTR_RECURSIVE = 1 << 0 /*!< Mutex is recursive. */
} M_thread_mutexattr_t;


/*! Mutex create.
 *
 * \param[in] attr M_thread_mutexattr_t attributes which control how the mutex should behave.
 *
 * \return Mutex on success otherwise NULL on error.
 */
M_API M_thread_mutex_t *M_thread_mutex_create(M_uint32 attr);


/*! Destroy a mutex.
 *
 * \param[in] mutex The mutex.
 */
M_API void M_thread_mutex_destroy(M_thread_mutex_t *mutex);


/*! Lock a mutex.
 *
 * This will block until the mutex can be locked.
 *
 * \param[in] mutex The mutex.
 *
 * \return M_TRUE if the function returns due to a successful mutex lock. Otherwise M_FALSE on error.
 *        This can fail for a number of reasons, for example:
 *        - The mutex was already locked by this thread.
 *        - The mutex is invalid.
 *        - The mutex has exceeded the maximum number of recursive locks.
 */
M_API M_bool M_thread_mutex_lock(M_thread_mutex_t *mutex);


/*! Try to lock the mutex.
 *
 * Does not block waiting to lock the mutex.
 *
 * \param[in] mutex The mutex.
 *
 * \return M_TRUE if the mutex was locked. Otherwise M_FALSE.
 */
M_API M_bool M_thread_mutex_trylock(M_thread_mutex_t *mutex);


/*! Unlock a locked mutex.
 *
 * \param[in] mutex The mutex.
 *
 * \return M_TRUE if the mutex was unlocked. Otherwise M_FALSE.
 */
M_API M_bool M_thread_mutex_unlock(M_thread_mutex_t *mutex);


/*! @} */


/*! \addtogroup m_thread_common_cond Thread Conditionals
 *  \ingroup m_thread_common
 *
 *  Thread Conditionals
 *
 *  @{
 */

/*! Conditional. */
struct M_thread_cond;
typedef struct M_thread_cond M_thread_cond_t;



/*! Conditional attributes.
 * Used for conditional creation. */
typedef enum {
	M_THREAD_CONDATTR_NONE = 0 /*!< None. */
} M_thread_condattr_t;



/*! Conditional create.
 *
 * \param[in] attr M_thread_condattr_t attributes which control how the conditional should behave.
 *
 * \return Conditional on success otherwise NULL on error.
 */
M_API M_thread_cond_t *M_thread_cond_create(M_uint32 attr);


/*! Destroy a conditional.
 *
 * \param[in] cond The conditional.
 */
M_API void M_thread_cond_destroy(M_thread_cond_t *cond);

/*! Wait on conditional with a timeout of now + millisec.
 *
 * \param[in]     cond     The conditional.
 * \param[in,out] mutex    The mutex to operate on.
 * \param[in]     millisec The amount of time wait from now in milliseconds.
 *
 * \return M_TRUE if the conditional was activated. M_FALSE on timeout or other error.
 *
 * \see M_thread_cond_wait
 * \see M_thread_cond_timedwait_abs
 */
M_API M_bool M_thread_cond_timedwait(M_thread_cond_t *cond, M_thread_mutex_t *mutex, M_uint64 millisec);


/*! Wait on conditional until a specified time.
 *
 * \param[in]     cond    The conditional.
 * \param[in,out] mutex   The mutex to operate on.
 * \param[in]     abstime Time to wait until.
 *
 * \return M_TRUE if the conditional was activated. M_FALSE on timeout or other error.
 *
 * \see M_thread_cond_wait
 * \see M_thread_cond_timedwait
 */
M_API M_bool M_thread_cond_timedwait_abs(M_thread_cond_t *cond, M_thread_mutex_t *mutex, const M_timeval_t *abstime);


/*! Wait on conditional
 *
 * Blocks the thread until the conditional is activated.
 *
 * The mutex must be locked before calling this function. This will unlock the mutex and block
 * on the conditional. When the conditional is activated the mutex will be locked.
 *
 * \param[in]     cond  The conditional.
 * \param[in,out] mutex The mutex to operate on.
 *
 * \return M_TRUE if the conditional was activated. M_FALSE on error.
 *
 * \see M_thread_cond_timedwait
 * \see M_thread_cond_timedwait_abs
 */
M_API M_bool M_thread_cond_wait(M_thread_cond_t *cond, M_thread_mutex_t *mutex);


/*! Activate all waiting conditionals.
 *
 * \param[in] cond  The conditional.
 */
M_API void M_thread_cond_broadcast(M_thread_cond_t *cond);


/*! Activate a waiting conditional (single).
 *
 * \param[in] cond  The conditional.
 */
M_API void M_thread_cond_signal(M_thread_cond_t *cond);

/*! @} */


/*! \addtogroup m_thread_common_rwlock Read/Write locks
 *  \ingroup m_thread_common
 *
 *  Read/Write locks
 *
 *  @{
 */

/*! Read/Write lock. */
struct M_thread_rwlock;
typedef struct M_thread_rwlock M_thread_rwlock_t;


/*! Read/Write lock, lock type. */
typedef enum {
	M_THREAD_RWLOCK_TYPE_READ = 0, /*!< Lock for read. */
	M_THREAD_RWLOCK_TYPE_WRITE     /*!< Lock for write. */
} M_thread_rwlock_type_t;


/*! Read/Write lock create.
 *
 * Read/Write locks allow multiple readers to be hold the lock at the same time. A
 * write lock will be allowed once all readers have released their locks.
 *
 * For new locks waiting writers are preferred. Meaning a if a writer is waiting
 * new read locks will not be given until all waiting writers has received and
 * released their locks.
 *
 * \return Read/Write lock on success otherwise NULL on error.
 */
M_API M_thread_rwlock_t *M_thread_rwlock_create(void);


/*! Destroy a read/write lock.
 *
 * \param[in] rwlock The lock.
 */
M_API void M_thread_rwlock_destroy(M_thread_rwlock_t *rwlock);


/*! Lock a read/write lock.
 *
 * The thread will block waiting to acquire the lock.
 *
 * \param[in] rwlock The lock.
 * \param[in] type   The type of lock to acquire.
 *
 * \return M_TRUE If the lock was acquired. Otherwise M_FALSE.
 */
M_API M_bool M_thread_rwlock_lock(M_thread_rwlock_t *rwlock, M_thread_rwlock_type_t type);


/*! Unlock a read/write lock.
 *
 * \param[in] rwlock The lock.
 *
 * \return M_TRUE If on success. Otherwise M_FALSE.
 */
M_API M_bool M_thread_rwlock_unlock(M_thread_rwlock_t *rwlock);


/*! @} */


/*! \addtogroup m_thread_common_tls Thread Local Storage
 *  \ingroup m_thread_common
 *
 *  Thread Local Storage
 *
 *  @{
 */


/*! Thread local storage key. */
typedef M_uint64 M_thread_tls_key_t;

/*! Create a key for storing data in thread local storage.
 *
 * \param destructor The destructor to call to destroy the stored value at the
 *                   returned key. Optional, use NULL if not needed.
 *
 * \return The key to use in tls.
 */
M_API M_thread_tls_key_t M_thread_tls_key_create(void (*destructor)(void *));


/*! Set the key for the current thread to the given value.
 *
 * \param[in] key   The key.
 * \param[in] value The value to store.
 *
 * \return M_TRUE if the value was stored. Otherwise M_FALSE.
 */
M_API M_bool M_thread_tls_setspecific(M_thread_tls_key_t key, const void *value);


/*! Get the value fro a given key.
 *
 * \param[in] key The key.
 *
 * \return The value or NULL if not value set/invalid key.
 */
M_API void *M_thread_tls_getspecific(M_thread_tls_key_t key);


/*! @} */



/*! \addtogroup m_thread_common_spinlock Spinlocks
 *  \ingroup m_thread_common
 *
 *  Spinlocks
 *
 *  @{
 */

/*! Public struct for spinlocks, so static initializers can be used */
typedef struct M_thread_spinlock {
	volatile M_uint32 current;
	volatile M_uint32 queue;
	M_threadid_t      threadid;
} M_thread_spinlock_t;

/*! Static initializer for spinlocks */
#define M_THREAD_SPINLOCK_STATIC_INITIALIZER { 0, 0, 0 }


/*! Lock a spinlock.
 *
 *  A spinlock is similar in usage to a mutex, but should NOT be used in place of a mutex.
 *  When in doubt, use a mutex instead, a spinlock is almost always the wrong thing to use.
 *  Spinlocks can be used protect areas of memory that are very unlikely to have high
 *  contention and should only be held for very short durations, or when the act of
 *  initializing a mutex might itself cause a race condition (such as during an initialization
 *  procedure as mutexes do not support static initializers).
 *
 *  When lock contention occurs on a spinlock, it will spin, consuming CPU, waiting
 *  for the lock to be released.  Spinlocks are purely implemented in userland
 *  using atomics.  The implementation uses 'tickets' to try to guarantee lock order
 *  in a first-come first-served manner, and has rudimentary backoff logic to attempt
 *  to reduce resource consumption during periods of high lock contention.
 *
 *  A spinlock variable must have been initialized using M_THREAD_SPINLOCK_STATIC_INITIALIZER
 *  and passed by reference into this function.  There is no initialization or
 *  destruction function.
 *
 *  \param[in] spinlock Spinlock initialized via M_THREAD_SPINLOCK_STATIC_INITIALIZER
 *                      and passed by reference.
 */
M_API void M_thread_spinlock_lock(M_thread_spinlock_t *spinlock);


/*! Unlock a spinlock
 *
 *  See M_thread_spinlock_lock() for more information.
 *
 *  \param[in] spinlock Spinlock initialized via M_THREAD_SPINLOCK_STATIC_INITIALIZER
 *                      and passed by reference.
 */
M_API void M_thread_spinlock_unlock(M_thread_spinlock_t *spinlock);


/*! @} */


/*! \addtogroup m_thread_common_once Threadsafe initialization helpers (Thread Once)
 *  \ingroup m_thread_common
 *
 *  Threadsafe initialization helpers (Thread Once)
 *
 *  @{
 */

/*! Public struct for M_thread_once, so static initializer can be used */
typedef struct M_thread_once {
	M_bool              initialized;
	M_thread_spinlock_t spinlock;
} M_thread_once_t;

/*! Static initializer for M_thread_once */
#define M_THREAD_ONCE_STATIC_INITIALIZER { M_FALSE, M_THREAD_SPINLOCK_STATIC_INITIALIZER }


/*! Ensure an initialization routine is performed only once, even if called from
 *  multiple threads simultaneously.
 *
 *  Performing initialization in a multi-threaded program can cause race conditions.
 *
 *  Take this code example:
 *    static int initialized = 0;
 *    if (!initialized) {
 *      init_routine();
 *      initialized = 1;
 *    }
 *
 *  If two threads where to enter this simultaneously, before init_routine() was
 *  complete, they would call it twice.  The above code example can be replaced
 *  with:
 *
 *  static M_thread_once_t initialized = M_THREAD_ONCE_STATIC_INITIALIZER;
 *  M_thread_once(&initialized, init_routine);
 *
 *  \param[in] once_control Once control variable passed by reference, and first set
 *                          to M_THREAD_ONCE_STATIC_INITIALIZER;
 *  \param[in] init_routine Initialization routine to be called if it has not yet
 *                          been called.
 *  \param[in] init_flags   Flags to be passed onto the initialization routine.
 *  \return M_TRUE if init routine was just run, M_FALSE if not run (previously run)
 */
M_API M_bool M_thread_once(M_thread_once_t *once_control, void (*init_routine)(M_uint64 flags), M_uint64 init_flags);


/*! Reset the once_control object back to an uninitialized state.  Useful to be
 *  called in a destructor so an initialization routine can be re-run.
 *
 *  \param[in] once_control Once control variable passed by reference, and first set
 *                          to M_THREAD_ONCE_STATIC_INITIALIZER;
 *  \return M_TRUE if reset, M_FALSE if not initialized.
 */
M_API M_bool M_thread_once_reset(M_thread_once_t *once_control);

/*! @} */

__END_DECLS

#endif /* __M_THREAD_H__ */
