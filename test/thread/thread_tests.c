#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib_thread.h>

typedef unsigned long long llu;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_model_t configured_thread_model = M_THREAD_MODEL_INVALID;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	unsigned long  usec;
	M_uint32      *count;
} sleeper_data_t;

static void *thread_sleeper(void *arg)
{
	sleeper_data_t *sd = (sleeper_data_t *)arg;
	M_thread_sleep(sd->usec);
	M_atomic_inc_u32(sd->count);
	return NULL;
}

static void *thread_innerd_inner(void *arg)
{
	sleeper_data_t *sd = (sleeper_data_t *)arg;
	M_thread_sleep(sd->usec);
	M_atomic_inc_u32(sd->count);
	return NULL;
}

static void *thread_innerd(void *arg)
{
	sleeper_data_t *sd = (sleeper_data_t *)arg;
	size_t i;

	for (i=0; i<5; i++) {
		M_thread_create(NULL, thread_innerd_inner, sd);
	}

	M_thread_sleep(sd->usec);
	M_atomic_inc_u32(sd->count);
	return NULL;
}

static void *thread_innerj(void *arg)
{
	sleeper_data_t  *sd = (sleeper_data_t *)arg;
	M_threadid_t     thread1;
	M_threadid_t     thread2;
	M_threadid_t     thread3;
	M_threadid_t     thread4;
	M_threadid_t     thread5;
	M_thread_attr_t *tattr;

	tattr   = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	thread1 = M_thread_create(tattr, thread_sleeper, sd);
	thread2 = M_thread_create(tattr, thread_sleeper, sd);
	thread3 = M_thread_create(tattr, thread_sleeper, sd);
	thread4 = M_thread_create(tattr, thread_sleeper, sd);
	thread5 = M_thread_create(tattr, thread_sleeper, sd);

	M_thread_attr_destroy(tattr);

	M_thread_join(thread1, NULL);
	M_thread_join(thread2, NULL);
	M_thread_join(thread3, NULL);
	M_thread_join(thread4, NULL);
	M_thread_join(thread5, NULL);

	M_thread_sleep(sd->usec);
	M_atomic_inc_u32(sd->count);
	return NULL;
}

typedef struct {
	unsigned long     usec;
	M_uint32         *count;
	M_uint32          expect;
	M_thread_mutex_t *mutex;
	M_bool            try;
	M_bool            try_fails;
} mutex_data_t;

static void *thread_mutex(void *arg)
{
	mutex_data_t *sd = (mutex_data_t *)arg;

	if (sd->try) {
		if (!M_thread_mutex_trylock(sd->mutex)) {
			ck_assert_msg(sd->try_fails, "mutex_trylock failed when it should have succeeded");
			return NULL;
		}
	} else {
		M_thread_mutex_lock(sd->mutex);
	}

	ck_assert_msg(*(sd->count) == sd->expect, "count (%u) != expect (%u)", sd->count, sd->expect);
	M_thread_sleep(sd->usec);
	M_atomic_inc_u32(sd->count);

	M_thread_mutex_unlock(sd->mutex);
	return NULL;
}

static void *thread_selfer(void *arg)
{
	M_threadid_t *id = (M_threadid_t *)arg;
	*id = M_thread_self();
	return NULL;
}

static void *thread_scheder(void *arg)
{
	M_uint32 *count = (M_uint32 *)arg;
	size_t    i;

	/* This is really because this test case sets thread priority and processor, so this may exit quickly */
	M_thread_sleep(50);

	for (i=0; i<5; i++) {
		M_atomic_inc_u32(count);
	}
	M_thread_yield(M_TRUE);
	M_thread_yield(M_FALSE);

	M_atomic_add_u32(count, 5);
	M_thread_yield(M_FALSE);
	M_thread_yield(M_TRUE);
	return NULL;
}

typedef struct {
	M_thread_mutex_t *mutex;
	M_thread_cond_t  *cond;
	M_uint32         *count;
	M_uint64          wait_msec;
} cond_data_t;

static void *thread_cond(void *arg)
{
	cond_data_t *sd = (cond_data_t *)arg;

	M_thread_mutex_lock(sd->mutex);
	if (sd->wait_msec > 0) {
		if (!M_thread_cond_timedwait(sd->cond, sd->mutex, sd->wait_msec)) {
			M_thread_mutex_unlock(sd->mutex);
			return NULL;
		}
	} else {
		M_thread_cond_wait(sd->cond, sd->mutex);
	}
	M_thread_mutex_unlock(sd->mutex);

	M_atomic_inc_u32(sd->count);

	return NULL;
}

typedef struct {
	M_thread_rwlock_t *rwlock;
	M_uint32          *count;
	unsigned long      usec;
	M_uint32           expect;
} rwlock_data_t;

static void *thread_rwlock_read(void *arg)
{
	rwlock_data_t *sd = (rwlock_data_t *)arg;

	M_thread_rwlock_lock(sd->rwlock, M_THREAD_RWLOCK_TYPE_READ);
	ck_assert_msg(*sd->count == sd->expect, "count (%u) != expect (%u)", *sd->count, sd->expect);
	M_thread_sleep(sd->usec);
	M_thread_rwlock_unlock(sd->rwlock);

	return NULL;
}

static void *thread_rwlock_write(void *arg)
{
	rwlock_data_t *sd = (rwlock_data_t *)arg;

	M_thread_rwlock_lock(sd->rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_atomic_inc_u32(sd->count);
	M_thread_sleep(sd->usec);
	M_thread_rwlock_unlock(sd->rwlock);

	return NULL;
}

typedef struct {
	unsigned long       usec;
	M_thread_tls_key_t  key;
	const char         *ptr;
} tls_data_t;

static void *thread_tls(void *arg)
{
	tls_data_t *sd = (tls_data_t *)arg;
	void       *ptr;

	if (sd->ptr == NULL) {
		ck_assert_msg(M_thread_tls_setspecific(sd->key, sd->ptr) == M_FALSE, "Set tls value on invalid key %llu", sd->key);
		return NULL;
	}

	ck_assert_msg(M_thread_tls_setspecific(sd->key, sd->ptr) == M_TRUE, "Cold not set tls value (%s) on key %llu", sd->ptr, sd->key);
	M_thread_sleep(sd->usec);
	ptr = M_thread_tls_getspecific(sd->key);
	ck_assert_msg(ptr == sd->ptr, "Value of key (%llu): %s != expected value: %s", sd->key, ptr, sd->ptr);

	return NULL;
}

typedef struct {
	M_uint32         *count;
	M_thread_mutex_t *mutex;
	M_list_u64_t     *seen_threads;
} task_data_t;

static void pool_task(void *arg)
{
	task_data_t *sd = (task_data_t *)arg;

	M_atomic_inc_u32(sd->count);

	M_thread_mutex_lock(sd->mutex);
	M_list_u64_insert(sd->seen_threads, M_thread_self());
	M_thread_mutex_unlock(sd->mutex);

	/* Try to make sure we're not going so fast no other thread wakes up to
	 * process a task.  A lot of minimum OS time slices are 15ms, so sleep for
	 * at least that */
	M_thread_sleep(15000);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_atomic)
{
	M_uint32 val;
	M_uint64 val64;

	/* cas32 */
	val = 0;
	ck_assert_msg(M_atomic_cas32(&val, 0, 1) && val == 1, "cas32 failed to set val");
	ck_assert_msg(M_atomic_cas32(&val, 1, 0) && val == 0, "cas32 failed to set val back");
	ck_assert_msg(!M_atomic_cas32(&val, 1, 0) && val == 0, "cas32 passed expected failure");

	/* cas64 */
	val64 = 0;
	ck_assert_msg(M_atomic_cas64(&val64, 0, 1) && val64 == 1, "cas64 failed to set val");
	ck_assert_msg(M_atomic_cas64(&val64, 1, 0) && val64 == 0, "cas64 failed to set val back");
	ck_assert_msg(!M_atomic_cas64(&val64, 1, 0) && val64 == 0, "cas64 passed expected failure");

	val = 0;
	ck_assert_msg(M_atomic_inc_u32(&val) == 0 && val == 1, "inc32 failed");
	ck_assert_msg(M_atomic_dec_u32(&val) == 1 && val == 0, "dec32 failed");

	val64 = 0;
	ck_assert_msg(M_atomic_inc_u64(&val64) == 0 && val64 == 1, "inc64 failed");
	ck_assert_msg(M_atomic_dec_u64(&val64) == 1 && val64 == 0, "dec64 failed");
}
END_TEST

START_TEST(check_verify_model)
{
	M_thread_model_t model;
	M_bool           ret;

	ret = M_thread_active_model(&model, NULL);

	ck_assert_msg(ret == M_TRUE, "No thread model active");
	ck_assert_msg(model == configured_thread_model, "configured thread model (%d) != model nin use (%d)", configured_thread_model, model);
}
END_TEST

START_TEST(check_cpu_cores)
{
	ck_assert_msg(M_thread_num_cpu_cores() > 0, "Unable to detect number of cpu cores");
}
END_TEST

START_TEST(check_sleeper)
{
#define NUM_SLEEPER_THREADS 100
	M_uint32       count = 0;
	size_t         i;
	sleeper_data_t sd1   = {
		1000000,
		&count
	};
	sleeper_data_t sd3   = {
		3000000,
		&count
	};
	sleeper_data_t sd5   = {
		5000000,
		&count
	};

	M_thread_create(NULL, thread_sleeper, &sd5);
	for (i=1; i<NUM_SLEEPER_THREADS; i++) {
		M_thread_create(NULL, thread_sleeper, (i%2==0)?&sd1:&sd3);
	}

	while (count < NUM_SLEEPER_THREADS) {
		M_thread_sleep(15000);
	}

	/* Attempt to wait for threads to actually fully shutdown as when count hits
	 * 100, threads may not have fully exited. */
	M_thread_sleep(15000);

	ck_assert_msg(M_thread_count() == 0, "Threads still reported as running: %llu", (llu)M_thread_count());
}
END_TEST

START_TEST(check_joiner)
{
	M_list_u64_t    *threads;
	M_threadid_t     thread;
	M_thread_attr_t *tattr;
	M_uint32         count = 0;
	size_t           i;
	size_t           len;
	sleeper_data_t   sd3   = {
		3000000,
		&count
	};
	sleeper_data_t   sd5   = {
		5000000,
		&count
	};

	threads = M_list_u64_create(M_LIST_U64_NONE);
	tattr   = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	thread = M_thread_create(tattr, thread_sleeper, &sd5);
	M_list_u64_insert(threads, thread);
	for (i=1; i<100; i++) {
		thread = M_thread_create(tattr, thread_sleeper, &sd3);
		M_list_u64_insert(threads, thread);
	}
	M_thread_attr_destroy(tattr);

	len = M_list_u64_len(threads);
	for (i=0; i<len; i++) {
		thread = (M_threadid_t)M_list_u64_at(threads, i);
		M_thread_join(thread, NULL);
	}
	M_list_u64_destroy(threads);

	ck_assert_msg(count == 100, "Not all threads ran: %u", count);
}
END_TEST

START_TEST(check_selfer)
{
	M_threadid_t        thread;
	M_thread_attr_t    *tattr;
	M_threadid_t        retid;

	tattr  = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	thread = M_thread_create(tattr, thread_selfer, &retid);
	M_thread_attr_destroy(tattr);
	M_thread_join(thread, NULL);

	ck_assert_msg(thread == retid, "ID from create != ID from M_thread_self");
}
END_TEST

START_TEST(check_sched)
{
	M_threadid_t     thread;
	M_thread_attr_t *tattr;
	M_uint32         count = 0;

	tattr  = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	M_thread_attr_set_priority(tattr, 1); /* wanted to check to see if this works */
	M_thread_attr_set_processor(tattr, 0);
	thread = M_thread_create(tattr, thread_scheder, &count);
	M_thread_attr_destroy(tattr);
	M_thread_join(thread, NULL);

	ck_assert_msg(count == 10, "scheder failure count: %u", count);
}
END_TEST

START_TEST(check_mutex)
{
	M_threadid_t      thread1;
	M_threadid_t      thread2;
	M_threadid_t      thread3;
	M_threadid_t      thread4;
	M_thread_mutex_t *mutex;
	M_thread_attr_t  *tattr;
	M_uint32          count  = 0;
	mutex_data_t      sdm1   = {
		3000000,
		&count,
		0,
		NULL,
		M_FALSE,
		M_FALSE
	};
	mutex_data_t       sdm2   = {
		3000000,
		&count,
		1,
		NULL,
		M_FALSE,
		M_FALSE
	};
	mutex_data_t       sdm3   = {
		3000000,
		&count,
		2,
		NULL,
		M_TRUE,
		M_TRUE
	};
	mutex_data_t       sdm4   = {
		3000000,
		&count,
		2,
		NULL,
		M_TRUE,
		M_FALSE
	};

	mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	sdm1.mutex = mutex;
	sdm2.mutex = mutex;
	sdm3.mutex = mutex;
	sdm4.mutex = mutex;

	tattr  = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	/* Start 3 thread. t1 will be given a chance to get the mutex lock.
 	 * t2 will wait until it can get the lock checking that count was
	 * set to 1 by t1. t3 expects to fail getting a trylock. */
	thread1 = M_thread_create(tattr, thread_mutex, &sdm1);
	M_thread_sleep(1000000);
	thread2 = M_thread_create(tattr, thread_mutex, &sdm2);
	thread3 = M_thread_create(tattr, thread_mutex, &sdm3);

	/* Wait for all threads to finish. */
	M_thread_join(thread1, NULL);
	M_thread_join(thread2, NULL);
	M_thread_join(thread3, NULL);

	/* Start t4. It will use a trylock but it should succeed. */
	thread4 = M_thread_create(tattr, thread_mutex, &sdm4);
	M_thread_join(thread4, NULL);

	/* Verify that the count is now 3. t1+t2+t4 = 3 */
	ck_assert_msg(count == 3, "Count != 3. count: %u\n", count);

	M_thread_attr_destroy(tattr);
	M_thread_mutex_destroy(mutex);
}
END_TEST


typedef struct M_spinlock_data {
	M_uint32            thread_count;
	M_uint32            spin_count;
	M_uint32            total;
	M_thread_spinlock_t spinlock;
	M_thread_mutex_t   *condlock;
	M_thread_cond_t    *parentcond;
	M_thread_cond_t    *threadcond;
} M_spinlock_data_t;

static void *thread_spinlock(void *arg)
{
	M_spinlock_data_t *data = arg;
	size_t             i;

	/* Tell the parent we've started */
	M_thread_mutex_lock(data->condlock);
	data->thread_count++;
	M_thread_cond_signal(data->parentcond);
	M_thread_cond_wait(data->threadcond, data->condlock);
	M_thread_mutex_unlock(data->condlock);

	/* Spin on lock until done */
	for (i=0; i<data->spin_count; i++) {
		M_uint32 myvar;

		M_thread_spinlock_lock(&data->spinlock);
		/* Read, Modify, Write -- do as separate ops! */
		myvar = data->total;
		myvar += 1;
		data->total = myvar;
		M_thread_spinlock_unlock(&data->spinlock);
	}
	return NULL;
}

START_TEST(check_spinlock)
{
	size_t            i;
#define SPINLOCK_THREAD_COUNT 8
	M_thread_attr_t  *tattr;
	M_threadid_t      thread[SPINLOCK_THREAD_COUNT];
	M_spinlock_data_t data = {
		0,     /* Number of threads started */
		100,   /* Number of times a thread should increment the counter */
		0,     /* Current counter */
		M_THREAD_SPINLOCK_STATIC_INITIALIZER,
		M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE),
		M_thread_cond_create(M_THREAD_CONDATTR_NONE),
		M_thread_cond_create(M_THREAD_CONDATTR_NONE)
	};

	/* Start threads */
	tattr  = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	for (i=0; i<SPINLOCK_THREAD_COUNT; i++) {
		thread[i] = M_thread_create(tattr, thread_spinlock, &data);
	}
	M_thread_attr_destroy(tattr);

	M_thread_mutex_lock(data.condlock);
	while (data.thread_count != SPINLOCK_THREAD_COUNT) {
		M_thread_cond_wait(data.parentcond, data.condlock);
	}

	/* All threads now started, wake them up */
	M_thread_cond_broadcast(data.threadcond);
	M_thread_mutex_unlock(data.condlock);

	/* Wait for all threads to finish */
	for (i=0; i<SPINLOCK_THREAD_COUNT; i++) {
		void *retval;
		M_thread_join(thread[i], &retval);
	}

	ck_assert_msg(data.total == SPINLOCK_THREAD_COUNT * data.spin_count, "Total != %u. total: %u\n", SPINLOCK_THREAD_COUNT * data.spin_count, data.total);
	M_thread_cond_destroy(data.parentcond);
	M_thread_cond_destroy(data.threadcond);
	M_thread_mutex_destroy(data.condlock);
}
END_TEST

START_TEST(check_cond_broadcast)
{
	M_threadid_t      thread1;
	M_threadid_t      thread2;
	M_threadid_t      thread3;
	M_threadid_t      thread4;
	M_thread_mutex_t *mutex;
	M_thread_cond_t  *cond;
	M_thread_attr_t  *tattr;
	M_uint32          count  = 0;
	cond_data_t       sd1    = {
		NULL,
		NULL,
		&count,
		0
	};
	cond_data_t       sd2    = {
		NULL,
		NULL,
		&count,
		1000
	};
	cond_data_t       sd3    = {
		NULL,
		NULL,
		&count,
		15000
	};

	mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	sd1.mutex = mutex;
	sd2.mutex = mutex;
	sd3.mutex = mutex;
	cond = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	sd1.cond = cond;
	sd2.cond = cond;
	sd3.cond = cond;

	tattr  = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	thread1 = M_thread_create(tattr, thread_cond, &sd1);
	M_thread_sleep(1000000);
	thread2 = M_thread_create(tattr, thread_cond, &sd1);
	thread3 = M_thread_create(tattr, thread_cond, &sd2);
	thread4 = M_thread_create(tattr, thread_cond, &sd3);
	M_thread_sleep(3000000);

	M_thread_mutex_lock(mutex);
	M_thread_cond_broadcast(cond);
	M_thread_mutex_unlock(mutex);

	/* Wait for all threads to finish. */
	M_thread_join(thread1, NULL);
	M_thread_join(thread2, NULL);
	M_thread_join(thread3, NULL);
	M_thread_join(thread4, NULL);

	ck_assert_msg(count == 3, "Count != 3. count: %u\n", count);

	M_thread_attr_destroy(tattr);
	M_thread_mutex_destroy(mutex);
	M_thread_cond_destroy(cond);
}
END_TEST

START_TEST(check_cond_signal)
{
	M_threadid_t      thread1;
	M_threadid_t      thread2;
	M_threadid_t      thread3;
	M_threadid_t      thread4;
	M_thread_mutex_t *mutex;
	M_thread_cond_t  *cond;
	M_thread_attr_t  *tattr;
	M_uint32          count  = 0;
	cond_data_t       sd1    = {
		NULL,
		NULL,
		&count,
		0
	};
	cond_data_t       sd2    = {
		NULL,
		NULL,
		&count,
		1000
	};
	cond_data_t       sd3    = {
		NULL,
		NULL,
		&count,
		15000
	};

	mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	sd1.mutex = mutex;
	sd2.mutex = mutex;
	sd3.mutex = mutex;
	cond = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	sd1.cond = cond;
	sd2.cond = cond;
	sd3.cond = cond;

	tattr  = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	thread1 = M_thread_create(tattr, thread_cond, &sd1);
	M_thread_sleep(1000000);
	thread2 = M_thread_create(tattr, thread_cond, &sd1);
	thread3 = M_thread_create(tattr, thread_cond, &sd2);
	thread4 = M_thread_create(tattr, thread_cond, &sd3);
	M_thread_sleep(3000000);

	M_thread_mutex_lock(mutex);
	M_thread_cond_signal(cond);
	M_thread_mutex_unlock(mutex);
	M_thread_sleep(1000000);
	ck_assert_msg(count == 1, "Count != 1. count: %u\n", count);

	M_thread_mutex_lock(mutex);
	M_thread_cond_signal(cond);
	M_thread_mutex_unlock(mutex);
	M_thread_sleep(1000000);
	ck_assert_msg(count == 2, "Count != 2. count: %u\n", count);

	M_thread_mutex_lock(mutex);
	M_thread_cond_signal(cond);
	M_thread_mutex_unlock(mutex);

	/* Wait for all threads to finish. */
	M_thread_join(thread1, NULL);
	M_thread_join(thread2, NULL);
	M_thread_join(thread3, NULL);
	M_thread_join(thread4, NULL);

	ck_assert_msg(count == 3, "Count != 3. count: %u\n", count);

	M_thread_attr_destroy(tattr);
	M_thread_mutex_destroy(mutex);
	M_thread_cond_destroy(cond);
}
END_TEST

START_TEST(check_rwlock)
{
	M_threadid_t       thread1;
	M_threadid_t       thread2;
	M_threadid_t       thread3;
	M_threadid_t       thread4;
	M_thread_rwlock_t *rwlock;
	M_thread_attr_t   *tattr;
	M_uint32           count  = 1;
	rwlock_data_t      sd1    = {
		NULL,
		&count,
		30,
		1,
	};
	rwlock_data_t      sd2    = {
		NULL,
		&count,
		30,
		1,
	};
	rwlock_data_t      sd3    = {
		NULL,
		&count,
		20000,
		0,
	};
	rwlock_data_t      sd4    = {
		NULL,
		&count,
		0,
		2,
	};

	rwlock     = M_thread_rwlock_create();
	sd1.rwlock = rwlock;
	sd2.rwlock = rwlock;
	sd3.rwlock = rwlock;
	sd4.rwlock = rwlock;

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	thread1 = M_thread_create(tattr, thread_rwlock_read, &sd1);
	M_thread_sleep(10);
	thread2 = M_thread_create(tattr, thread_rwlock_read, &sd2);
	M_thread_sleep(10000);
	thread3 = M_thread_create(tattr, thread_rwlock_write, &sd3);
	M_thread_sleep(10000);
	thread4 = M_thread_create(tattr, thread_rwlock_read, &sd4);

	/* Wait for all threads to finish. */
	M_thread_join(thread1, NULL);
	M_thread_join(thread2, NULL);
	M_thread_join(thread3, NULL);
	M_thread_join(thread4, NULL);

	M_thread_attr_destroy(tattr);
	M_thread_rwlock_destroy(rwlock);
}
END_TEST

START_TEST(check_tls)
{
	M_threadid_t       thread1;
	M_threadid_t       thread2;
	M_threadid_t       thread3;
	M_threadid_t       thread4;
	M_thread_tls_key_t tls_key;
	M_thread_attr_t   *tattr;
	tls_data_t         sd1    = {
		3000000,
		0,
		NULL,
	};
	tls_data_t         sd2    = {
		1000000,
		0,
		NULL,
	};
	tls_data_t         sd3    = {
		2000000,
		0,
		"XYZ",
	};
	tls_data_t         sd4    = {
		0,
		555,
		NULL,
	};

	tattr  = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	tls_key = M_thread_tls_key_create(M_free);
	sd1.key = tls_key;
	sd2.key = tls_key;
	sd1.ptr = M_strdup("ABC");
	sd2.ptr = M_strdup("123");

	tls_key = M_thread_tls_key_create(NULL);
	sd3.key = tls_key;

	thread1 = M_thread_create(tattr, thread_tls, &sd1);
	M_thread_sleep(1000);
	thread2 = M_thread_create(tattr, thread_tls, &sd2);
	M_thread_sleep(1000);
	thread3 = M_thread_create(tattr, thread_tls, &sd3);
	M_thread_sleep(1000);
	thread4 = M_thread_create(tattr, thread_tls, &sd4);

	/* Wait for all threads to finish. */
	M_thread_join(thread1, NULL);
	M_thread_join(thread2, NULL);
	M_thread_join(thread3, NULL);
	M_thread_join(thread4, NULL);

	M_thread_attr_destroy(tattr);
}
END_TEST

#define CHECK_POOL_THREAD_CNT 8
#define CHECK_POOL_QUEUE_CNT  CHECK_POOL_THREAD_CNT*2
#define CHECK_POOL_TASK_CNT   CHECK_POOL_THREAD_CNT*4
START_TEST(check_pool)
{
	M_threadpool_t        *pool;
	M_threadpool_parent_t *parent;
	void                  *args[CHECK_POOL_TASK_CNT];
	M_uint32               count = 0;
	task_data_t            sd    = {
		&count,
		NULL,
		NULL
	};
	size_t len;
	size_t i;

	pool   = M_threadpool_create(0, CHECK_POOL_THREAD_CNT, 0, CHECK_POOL_QUEUE_CNT);
	parent = M_threadpool_parent_create(pool);

	sd.mutex        = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	sd.seen_threads = M_list_u64_create(M_LIST_U64_SET);


	for (i=0; i<CHECK_POOL_TASK_CNT; i++)
		args[i] = &sd;
	M_threadpool_dispatch(parent, pool_task, args, CHECK_POOL_TASK_CNT);

	M_threadpool_parent_wait(parent);

	ck_assert_msg(count == CHECK_POOL_TASK_CNT, "count (%u) != %u", count, CHECK_POOL_TASK_CNT);
	len = M_list_u64_len(sd.seen_threads);
	ck_assert_msg(len == CHECK_POOL_THREAD_CNT, "Pool did not use all threads: %llu of %llu used", (llu)len, CHECK_POOL_THREAD_CNT);

	M_list_u64_destroy(sd.seen_threads);
	M_thread_mutex_destroy(sd.mutex);

	M_threadpool_parent_destroy(parent);
	M_threadpool_destroy(pool);
}
END_TEST

START_TEST(check_innerd)
{
	M_uint32       count = 0;
	size_t         i;
	sleeper_data_t sd    = {
		1000000,
		&count
	};

	for (i=0; i<15; i++) {
		M_thread_create(NULL, thread_innerd, &sd);
	}

	/* 15 threads each increment and spwan 5 threads that increment
 	 * gives us 90 total count. */
	while (count < 90) {
		M_thread_sleep(1000);
	}
	M_thread_sleep(5000000);

	ck_assert_msg(M_thread_count() == 0, "Threads still reported as running: %llu", (llu)M_thread_count());
}
END_TEST

START_TEST(check_innerj)
{
	M_threadid_t     thread1;
	M_threadid_t     thread2;
	M_threadid_t     thread3;
	M_threadid_t     thread4;
	M_threadid_t     thread5;
	M_thread_attr_t *tattr;
	M_uint32         count = 0;
	sleeper_data_t   sd    = {
		1000000,
		&count
	};

	tattr   = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	thread1 = M_thread_create(tattr, thread_innerj, &sd);
	thread2 = M_thread_create(tattr, thread_innerj, &sd);
	thread3 = M_thread_create(tattr, thread_innerj, &sd);
	thread4 = M_thread_create(tattr, thread_innerj, &sd);
	thread5 = M_thread_create(tattr, thread_innerj, &sd);

	M_thread_attr_destroy(tattr);

	M_thread_join(thread1, NULL);
	M_thread_join(thread2, NULL);
	M_thread_join(thread3, NULL);
	M_thread_join(thread4, NULL);
	M_thread_join(thread5, NULL);

	ck_assert_msg(count == 30, "Not all threads ran: %u", count);
}
END_TEST

static M_uint32 check_once_value = 0;
static void check_once_routine(M_uint64 flags)
{
	(void)flags;
	M_thread_sleep(100000); /* Try to cause race */
	check_once_value++;
}

static void *check_once_thread(void *arg)
{
	static M_thread_once_t once_control = M_THREAD_ONCE_STATIC_INITIALIZER;
	(void)arg;
	M_thread_once(&once_control, check_once_routine, 0);
	return NULL;
}

START_TEST(check_once)
{
#define CHECK_ONCE_THREADS 15
	size_t           i;
	M_thread_attr_t *tattr;
	M_threadid_t     thread[CHECK_ONCE_THREADS];

	tattr   = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);

	for (i=0; i<CHECK_ONCE_THREADS; i++)
		thread[i] = M_thread_create(tattr, check_once_thread, NULL);

	M_thread_attr_destroy(tattr);

	for (i=0; i<CHECK_ONCE_THREADS; i++) {
		void *retval;
		M_thread_join(thread[i], &retval);
	}

	ck_assert_msg(check_once_value == 1, "init routine ran more than once");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_thread_suite(M_thread_model_t model, const char *name)
{
	Suite *suite;
	TCase *tc;

	M_thread_init(model);
	configured_thread_model = model;

	suite = suite_create(name);

	tc = tcase_create("check_atomic");
	tcase_add_test(tc, check_atomic);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_verify_model");
	tcase_add_test(tc, check_verify_model);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_cpu_cores");
	tcase_add_test(tc, check_cpu_cores);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_sleeper");
	tcase_add_test(tc, check_sleeper);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_joiner");
	tcase_add_test(tc, check_joiner);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_selfer");
	tcase_add_test(tc, check_selfer);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_sched");
	tcase_add_test(tc, check_sched);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_mutex");
	tcase_add_test(tc, check_mutex);
	tcase_set_timeout(tc, 15);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_spinlock");
	tcase_add_test(tc, check_spinlock);
	tcase_set_timeout(tc, 30);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_cond_broadcast");
	tcase_add_test(tc, check_cond_broadcast);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_cond_signal");
	tcase_add_test(tc, check_cond_signal);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_rwlock");
	tcase_add_test(tc, check_rwlock);
	tcase_set_timeout(tc, 0);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_threadlocalstorage");
	tcase_add_test(tc, check_tls);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_pool");
	tcase_add_test(tc, check_pool);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_innerd");
	tcase_add_test(tc, check_innerd);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_innerj");
	tcase_add_test(tc, check_innerj);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_once");
	tcase_add_test(tc, check_once);
	suite_add_tcase(suite, tc);

	return suite;
}
