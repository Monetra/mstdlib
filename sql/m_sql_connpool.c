/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>
#include "base/m_defs_int.h"
#include "m_sql_int.h"


typedef struct {
	M_sql_conn_t        *conn;
	M_sql_driver_stmt_t *stmt;
} M_sql_stmt_cache_t;


typedef enum {
	M_SQL_CONN_INFO_NEW        = 0, /*!< Connection has never been attempted, or was shut down gracefully (e.g. idle) */
	M_SQL_CONN_INFO_UP         = 1, /*!< Connection is established, or in process of being established */
	M_SQL_CONN_INFO_FAILED     = 2, /*!< Connectivity failure, currently down */
} M_sql_conn_info_t;

/*! Holds sub-pool specific data */
typedef struct {
	M_llist_t         *conns;          /*!< List of idle M_sql_conn_t objects */
	size_t             used_conns;     /*!< Connections in-use, checked out by active processes */
	size_t             max_conns;      /*!< M_list_len(conns) + used_conns will always be <= this value */

	M_bool             is_initialized; /*!< Set to M_TRUE after first connection is brought up to prevent re-running a first connect sequence */
	M_sql_conn_info_t *info;           /*!< Array of connection status information, one per connection */
	M_time_t          *host_offline_t; /*!< Timestamp host was last attempted and determined to be bad.  Used for fallback */
	size_t             host_idx;       /*!< Index of current host being used.  Incremented on failure of current index or when load balancing.
	                                    *   May also revert when host_offline_t expires. */
	size_t             num_hosts;      /*!< Number of hosts referenced by connection string (for load balancing or failover) */
	size_t             num_waiters;    /*!< Count of waiters for an SQL connection to become idle */
	M_thread_cond_t   *cond;           /*!< Conditional used by waiters */
} M_sql_connpool_data_t;


struct M_sql_conn {
	M_timeval_t            start_tv;         /*!< Time connection was started (really before connect), but use this for reconnect_time_s too */
	M_timeval_t            last_used_tv;     /*!< Time connection was last used, this is used for max_idle_time_s */
	M_uint64               connect_time_ms;  /*!< This is how many millisecons the connection took to establish */
	size_t                 id;               /*!< ID of connection, (0 - max_conns] */
	size_t                 host_idx;         /*!< Host index currently connected to */
	M_bool                 in_trans;         /*!< M_TRUE if in an SQL transaction, M_FALSE if a single SQL query */
	M_sql_conn_state_t     state;            /*!< State of connection, used for forcing disconnects and tracking rollbacks */
	M_sql_driver_conn_t   *conn;             /*!< Driver-specific connection object */
	M_cache_strvp_t       *stmt_cache;       /*!< Client-side prepared statement cache */
	M_sql_connpool_t      *pool;             /*!< Pointer to parent pool */
	M_sql_connpool_data_t *pool_data;        /*!< Pointer to parent sub-pool (primary vs readonly) */
};


struct M_sql_connpool {
	M_thread_mutex_t        *lock;              /*!< Lock protecting pool object */
	M_sql_driver_t          *driver;            /*!< Pointer to sql driver registered */
	M_sql_driver_connpool_t *dpool;             /*!< Driver-specific pool object */
	M_bool                   started;           /*!< Whether the pool has been started yet or not */
	M_sql_trace_cb_t         trace_cb;          /*!< Trace callback */
	void                    *trace_cb_arg;      /*!< Argument passed to trace callback */

	M_sql_connpool_data_t    pool_primary;      /*!< Primary read/write pool */
	M_sql_connpool_data_t    pool_readonly;     /*!< Optional.  Read-only pool, max_conns == 0 if not used */

	char                    *sql_serverversion; /*!< SQL Server Name and Version */

	char                    *username;          /*!< User-supplied username */
	char                    *password;          /*!< User-supplied password */
	M_sql_connpool_flags_t   flags;             /*!< flags controlling behavior */

	M_time_t                 reconnect_time_s;  /*!< Time in seconds before forcing a reconnect to the DB server even
	                                             *   when the connection is good.  Used for possible rebalancing across
	                                             *   multiple servers, or fallback after a failure */
	M_time_t                 max_idle_time_s;   /*!< Time in seconds a connection is allowed to be idle before a forced
	                                             *   reconnect occurs.   Used to prevent trying to use stale connections
	                                             *   that might have been "forgotten" by a firewall */
	M_time_t                 fallback_s;        /*!< Time in seconds when a host is detected as down before it will be
	                                             *   eligible to be used again (unless all other hosts have failed) */

	M_rand_t                *rand;              /*!< Random state used for generating random ids and timers */

	M_hash_strvp_t          *group_insert;      /*!< Query -> Stmt reference for group insert optimization */
};


static M_thread_mutex_t *M_sql_lock      = NULL;   /*!< Global lock used for loaded driver list */
static M_hash_strvp_t   *M_sql_drivers   = NULL;   /*!< Hashtable of loaded drivers (name->M_sql_driver_t *) */
static M_thread_once_t   M_sql_init_once = M_THREAD_ONCE_STATIC_INITIALIZER; /*!< Thread once safety */


static void M_sql_destroy(void *arg)
{
	(void)arg;

	M_thread_mutex_destroy(M_sql_lock);
	M_sql_lock = NULL;

	M_hash_strvp_destroy(M_sql_drivers, M_TRUE);
	M_sql_drivers = NULL;

	M_thread_once_reset(&M_sql_init_once);
}


static void M_sql_driver_destroy(void *arg)
{
	M_sql_driver_t *driver = arg;

	if (arg == NULL)
		return;

	/* If there is a destroy callback, call it */
	if (driver->cb_destroy) {
		driver->cb_destroy();
	}

	M_module_unload(driver->handle);
}


static M_sql_error_t M_sql_driver_init(M_sql_driver_t **driver, M_sql_driver_t *(*get_driver)(void), const char *name, M_module_handle_t handle, char *error, size_t error_size)
{
	M_sql_error_t   err = M_SQL_ERROR_SUCCESS;

	*driver = get_driver();
	if (*driver == NULL) {
		M_snprintf(error, error_size, "Driver callback did not return driver structure");
		err = M_SQL_ERROR_CONN_DRIVERLOAD;
		goto done;
	}

	/* Validate the major version */
	if ((((*driver)->driver_sys_version >> 8) & 0xFF) != ((M_SQL_DRIVER_VERSION >> 8) & 0xFF)) {
		M_snprintf(error, error_size, "Incompatible driver major (driver %d.%02d vs system %d.%02d)",
		           (int)(((*driver)->driver_sys_version >> 8) & 0xFF), (int)((*driver)->driver_sys_version & 0xFF),
		           (int)((M_SQL_DRIVER_VERSION >> 8) & 0xFF), (int)(M_SQL_DRIVER_VERSION & 0xFF));
		err = M_SQL_ERROR_CONN_DRIVERVER;
		goto done;
	}
	/* Validate the minor version */
	if (((*driver)->driver_sys_version & 0xFF) > (M_SQL_DRIVER_VERSION & 0xFF)) {
		M_snprintf(error, error_size, "Incompatible driver minor (driver %d.%02d vs system %d.%02d)",
		           (int)(((*driver)->driver_sys_version >> 8) & 0xFF), (int)((*driver)->driver_sys_version & 0xFF),
		           (int)((M_SQL_DRIVER_VERSION >> 8) & 0xFF), (int)(M_SQL_DRIVER_VERSION & 0xFF));
		err = M_SQL_ERROR_CONN_DRIVERVER;
		goto done;
	}

	if (!(*driver)->cb_init            ||
	    !(*driver)->cb_destroy         ||
	    !(*driver)->cb_createpool      ||
	    !(*driver)->cb_destroypool     ||
	    !(*driver)->cb_connect         ||
	    !(*driver)->cb_serverversion   ||
	    !(*driver)->cb_disconnect      ||
	    !(*driver)->cb_queryformat     ||
	    !(*driver)->cb_prepare         ||
	    !(*driver)->cb_prepare_destroy ||
	    !(*driver)->cb_execute         ||
	    !(*driver)->cb_fetch           ||
	    !(*driver)->cb_begin           ||
	    !(*driver)->cb_rollback        ||
	    !(*driver)->cb_commit          ||
	    !(*driver)->cb_datatype        ||
	    !(*driver)->cb_append_bitop) {
		M_snprintf(error, error_size, "Malformed module, missing callback(s)");
		err = M_SQL_ERROR_CONN_DRIVERLOAD;
		goto done;
	}

	/* Run driver custom init routine */
	if ((*driver)->cb_init) {
		if (!(*driver)->cb_init(error, error_size)) {
			err = M_SQL_ERROR_CONN_DRIVERLOAD;
			goto done;
		}
	}

	/* Looks like driver is in a good state, register it! */
	(*driver)->handle = handle;
	M_hash_strvp_insert(M_sql_drivers, name, *driver);

done:
	return err;
}


/* Create prototypes for built-in modules */
#ifdef MSTDLIB_SQL_STATIC_SQLITE
M_sql_driver_t *M_sql_get_driver_sqlite(void);
#endif
#ifdef MSTDLIB_SQL_STATIC_MYSQL
M_sql_driver_t *M_sql_get_driver_mysql(void);
#endif
#ifdef MSTDLIB_SQL_STATIC_POSTGRESQL
M_sql_driver_t *M_sql_get_driver_postgresql(void);
#endif


static void M_sql_init_routine(M_uint64 flags)
{
	M_sql_driver_t *driver = NULL;;
	(void)driver;
	(void)flags;
	M_sql_lock    = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	M_sql_drivers = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_LOWER|M_HASH_STRVP_CASECMP, M_sql_driver_destroy);
	M_library_cleanup_register(M_sql_destroy, NULL);

	/* Load built-in modules */
#ifdef MSTDLIB_SQL_STATIC_SQLITE
	M_sql_driver_init(&driver, M_sql_get_driver_sqlite,     "sqlite",     M_MODULE_INVALID_HANDLE, NULL, 0);
#endif
#ifdef MSTDLIB_SQL_STATIC_MYSQL
	M_sql_driver_init(&driver, M_sql_get_driver_mysql,      "mysql",      M_MODULE_INVALID_HANDLE, NULL, 0);
#endif
#ifdef MSTDLIB_SQL_STATIC_POSTGRESQL
	M_sql_driver_init(&driver, M_sql_get_driver_postgresql, "postgresql", M_MODULE_INVALID_HANDLE, NULL, 0);
#endif
}


static void M_sql_init(void)
{
	M_thread_once(&M_sql_init_once, M_sql_init_routine, 0);
}



static M_sql_error_t M_sql_driver_load(M_sql_driver_t **driver, const char *name, char *error, size_t error_size)
{
	M_sql_error_t      err               = M_SQL_ERROR_SUCCESS;
	M_module_handle_t  handle            = M_MODULE_INVALID_HANDLE;
	char              *lower_name        = NULL;
	char               module_name[256];
	char               module_symbol[256];
	M_sql_driver_t  *(*get_driver)(void) = NULL;
	char               myerror[256];

	M_sql_init();

	if (driver != NULL)
		*driver = NULL;

	if (M_str_isempty(name) || driver == NULL) {
		M_snprintf(error, error_size, "Must specify a driver");
		return M_SQL_ERROR_INVALID_USE;
	}

	M_thread_mutex_lock(M_sql_lock);

	/* See if driver is cached */
	*driver = M_hash_strvp_get_direct(M_sql_drivers, name);
	if (*driver != NULL)
		goto done;

	/* Attempt to load the module */
	lower_name = M_strdup_lower(name);
	M_snprintf(module_name, sizeof(module_name), "mstdlib_sql_%s", lower_name);
	handle     = M_module_load(module_name, error, error_size);
	if (handle == M_MODULE_INVALID_HANDLE) {
		err = M_SQL_ERROR_CONN_NODRIVER;
		goto done;
	}

	/* Find the M_sql_get_driver_%s function */
	M_snprintf(module_symbol, sizeof(module_symbol), "M_sql_get_driver_%s", lower_name);
	get_driver = M_module_symbol(handle, module_symbol);
	if (get_driver == NULL) {
		M_snprintf(error, error_size, "%s() symbol not found in module %s", module_symbol, module_name);
		err = M_SQL_ERROR_CONN_DRIVERLOAD;
		goto done;
	}

	err = M_sql_driver_init(driver, get_driver, lower_name, handle, myerror, sizeof(myerror));
	if (M_sql_error_is_error(err)) {
		M_snprintf(error, error_size, "module %s: %s", module_name, myerror);
		goto done;
	}

done:
	M_free(lower_name);
	if (err != M_SQL_ERROR_SUCCESS) {
		if (handle != M_MODULE_INVALID_HANDLE)
			M_module_unload(handle);
		*driver = NULL;
	}
	M_thread_mutex_unlock(M_sql_lock);
	return err;
}


static M_sql_connpool_t *M_sql_connpool_init(M_sql_driver_t *driver, const char *username, const char *password, M_sql_connpool_flags_t flags)
{
	M_sql_connpool_t *pool = M_malloc_zero(sizeof(*pool));

	pool->driver                  = driver;
	pool->lock                    = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	pool->username                = M_strdup(username);
	pool->password                = M_strdup(password);
	pool->flags                   = flags;
	pool->rand                    = M_rand_create(0);
	pool->group_insert            = M_hash_strvp_create(16, 75, M_HASH_STRVP_NONE, NULL);

	return pool;
}


static M_bool M_sql_connpool_add_subpool(M_sql_connpool_t *pool, M_bool is_readonly, const char *conn_str, size_t max_conns, char *error, size_t error_size)
{
	M_sql_connpool_data_t *data = is_readonly?&pool->pool_readonly:&pool->pool_primary;
	M_bool                 rv;
	M_hash_dict_t         *conndict;

	if (data->conns != NULL) {
		M_snprintf(error, error_size, "Pool has already been configured");
		return M_FALSE;
	}

	conndict = M_hash_dict_deserialize(conn_str, ';', '=', '\'', '\'', M_HASH_DICT_DESER_FLAG_CASECMP);
	if (conndict == NULL) {
		M_snprintf(error, error_size, "Failed to parse connection string");
		return M_FALSE;
	}

	rv                   = pool->driver->cb_createpool(&pool->dpool, pool, is_readonly, conndict, &data->num_hosts, error, error_size);
	M_hash_dict_destroy(conndict);
	if (!rv) {
		return M_FALSE;
	}

	data->cond           = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	data->conns          = M_llist_create(NULL, M_LLIST_NONE);
	data->info           = M_malloc_zero(sizeof(*data->info) * max_conns);
	data->max_conns      = max_conns;
	data->host_offline_t = M_malloc_zero(sizeof(*data->host_offline_t) * data->num_hosts);
	return M_TRUE;
}


static void M_sql_conn_destroy(M_sql_conn_t *conn, M_bool graceful)
{
	if (conn == NULL)
		return;

	if (graceful)
		M_sql_trace_message_conn(M_SQL_TRACE_DISCONNECTING, conn, M_SQL_ERROR_SUCCESS, NULL);

	/* Update last used time to track how long a destroy takes */
	M_time_elapsed_start(&conn->last_used_tv);

	/* Clear cached statement handles */
	M_cache_strvp_destroy(conn->stmt_cache);

	if (conn->conn)
		conn->pool->driver->cb_disconnect(conn->conn);

	if (graceful)
		M_sql_trace_message_conn(M_SQL_TRACE_DISCONNECTED, conn, M_SQL_ERROR_SUCCESS, NULL);

	M_free(conn);
}


static void M_sql_stmt_cache_remove(void *arg)
{
	M_sql_stmt_cache_t *cache = arg;
	if (arg == NULL)
		return;
	cache->conn->pool->driver->cb_prepare_destroy(cache->stmt);
	M_free(cache);
}


/*! Requires pool to already be locked */
static size_t M_sql_connpool_get_host_idx(M_sql_connpool_t *pool, M_bool readonly)
{
	M_sql_connpool_data_t *pool_data;
	size_t                 curr_idx;
	size_t                 i;

	if (readonly && pool->pool_readonly.max_conns > 0) {
		pool_data = &pool->pool_readonly;
	} else {
		pool_data = &pool->pool_primary;
	}

	curr_idx = pool_data->host_idx;

	/* If load balancing, increment the host index, and return the pre-incremented value */
	if (pool->flags & M_SQL_CONNPOOL_FLAG_LOAD_BALANCE) {
		pool_data->host_idx = (pool_data->host_idx + 1) % pool_data->num_hosts;
		return curr_idx;
	}

	/* If the current index indicates it has failed over, and we have a fallback
	 * time configured, we need to scan to see if one of the higher-priority
	 * connections is eligible to be attempted again */
	if (curr_idx != 0 && pool->fallback_s > 0) {
		for (i=0; i<curr_idx; i++) {
			if (pool_data->host_offline_t[i] + pool->fallback_s <= M_time()) {
				curr_idx                     = i;
				pool_data->host_offline_t[i] = 0;
				break;
			}
		}
	}

	return curr_idx;
}


/*! Requires pool to already be locked */
static void M_sql_connpool_mark_host_idx_failed(M_sql_connpool_t *pool, size_t host_idx, M_bool readonly)
{
	M_sql_connpool_data_t *pool_data;

	if (readonly && pool->pool_readonly.max_conns > 0) {
		pool_data = &pool->pool_readonly;
	} else {
		pool_data = &pool->pool_primary;
	}

	pool_data->host_offline_t[host_idx] = M_time();

	/* With loadbalancing, we don't touch the index */
	if (pool->flags & M_SQL_CONNPOOL_FLAG_LOAD_BALANCE)
		return;

	/* If the current index matches the tested index, and in failover mode, increment */
	if (host_idx == pool_data->host_idx) {
		pool_data->host_idx = (pool_data->host_idx + 1) % pool_data->num_hosts;
	}
}


static M_sql_error_t M_sql_conn_create(M_sql_conn_t **conn, M_sql_connpool_t *pool, size_t id, M_bool is_readonly, char *error, size_t error_size)
{
	M_sql_error_t          err;
	M_sql_connpool_data_t *pool_data = is_readonly?&pool->pool_readonly:&pool->pool_primary;
	char                   myerror[256];

	/* For tracing, we need a real buffer */
	if (error == NULL || error_size == 0) {
		error      = myerror;
		error_size = sizeof(myerror);
	}

	*conn                    = M_malloc_zero(sizeof(**conn));

	(*conn)->id              = id;
	(*conn)->pool            = pool;
	(*conn)->pool_data       = pool_data;
	(*conn)->stmt_cache      = M_cache_strvp_create(32, M_CACHE_STRVP_NONE /* Not CASECMP */, M_sql_stmt_cache_remove);
	(*conn)->state           = M_SQL_CONN_STATE_OK;

	M_thread_mutex_lock(pool->lock);
	(*conn)->host_idx        = M_sql_connpool_get_host_idx(pool, is_readonly);
	M_thread_mutex_unlock(pool->lock);

	M_time_elapsed_start(&(*conn)->start_tv);

	M_sql_trace_message_conn(M_SQL_TRACE_CONNECTING, *conn, M_SQL_ERROR_SUCCESS, NULL);

	err                      = pool->driver->cb_connect(&(*conn)->conn, pool, is_readonly, (*conn)->host_idx, error, error_size);
	(*conn)->connect_time_ms = M_time_elapsed(&(*conn)->start_tv);

	if (err != M_SQL_ERROR_SUCCESS)
		goto fail;

	if (pool->driver->cb_connect_runonce) {
		err = pool->driver->cb_connect_runonce(*conn, pool->dpool, ((*conn)->id == 0 && !pool_data->is_initialized)?M_TRUE:M_FALSE, is_readonly, error, error_size);
		if (err != M_SQL_ERROR_SUCCESS)
			goto fail;
	}

	M_time_elapsed_start(&(*conn)->last_used_tv);
	M_sql_trace_message_conn(M_SQL_TRACE_CONNECTED, *conn, M_SQL_ERROR_SUCCESS, NULL);

	if ((*conn)->id == 0 && !pool_data->is_initialized)
		pool_data->is_initialized = M_TRUE;

fail:
	if (err != M_SQL_ERROR_SUCCESS) {
		M_sql_trace_message_conn(M_SQL_TRACE_CONNECT_FAILED, *conn, err, error);

		/* Update tracking for failed hosts */
		M_thread_mutex_lock(pool->lock);
		M_sql_connpool_mark_host_idx_failed(pool, (*conn)->host_idx, is_readonly);
		M_thread_mutex_unlock(pool->lock);

		M_sql_conn_destroy(*conn, M_FALSE);
		*conn = NULL;
	}
	return err;
}


static M_sql_error_t M_sql_connpool_spawn(M_sql_connpool_t *pool, M_sql_connpool_data_t *pool_data, char *error, size_t error_size)
{
	size_t        start_conns = (pool->flags & M_SQL_CONNPOOL_FLAG_PRESPAWN_ALL)?pool_data->max_conns:1;
	size_t        i;
	M_bool        is_readonly = (&pool->pool_readonly == pool_data);
	M_sql_error_t err         = M_SQL_ERROR_SUCCESS;

	/* Most likely this is the readonly pool since there are no connections/data, return success */
	if (pool_data->max_conns == 0)
		return M_SQL_ERROR_SUCCESS;

	for (i=0; i<start_conns && err == M_SQL_ERROR_SUCCESS; i++) {
		M_sql_conn_t *conn = NULL;
		char          temp[256];
		size_t        j;

		pool_data->info[i] = M_SQL_CONN_INFO_UP;

		/* Try to connect up to num_hosts times, or until successful */
		for (j=0; j<pool_data->num_hosts; j++) {
			err = M_sql_conn_create(&conn, pool, i, is_readonly, temp, sizeof(temp));
			if (err == M_SQL_ERROR_SUCCESS || conn == NULL)
				break;
		}

		if (err != M_SQL_ERROR_SUCCESS) {
			pool_data->info[i] = M_SQL_CONN_INFO_FAILED;
			M_snprintf(error, error_size, "(%s) #%zu of %zu: %s", is_readonly?"RO":"RW", i+1, start_conns, temp);
		} else {
			if (i == 0 && !is_readonly) {
				if (pool->sql_serverversion)
					M_free(pool->sql_serverversion);
				pool->sql_serverversion = M_strdup(pool->driver->cb_serverversion(conn->conn));
			}
			/* Add connection to pool as idle */
			M_llist_insert(pool_data->conns, conn);
		}
	}

	return err;
}


M_sql_error_t M_sql_connpool_create(M_sql_connpool_t **pool, const char *driver_str, const char *conn_str, const char *username, const char *password, size_t max_conns, M_uint32 flags, char *error, size_t error_size)
{
	M_sql_error_t   err         = M_SQL_ERROR_SUCCESS;
	M_sql_driver_t *driver      = NULL;

	if (pool == NULL)
		return M_SQL_ERROR_INVALID_USE;

	if (M_str_isempty(driver_str)) {
		M_snprintf(error, error_size, "must specify a driver");
		return M_SQL_ERROR_INVALID_USE;
	}

	if (M_str_isempty(conn_str)) {
		M_snprintf(error, error_size, "must specify a valid connection string");
		return M_SQL_ERROR_INVALID_USE;
	}

	if (max_conns == 0) {
		M_snprintf(error, error_size, "must specify maximum number of connections greater than 0");
		return M_SQL_ERROR_INVALID_USE;
	}

	*pool = NULL;

	err = M_sql_driver_load(&driver, driver_str, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS)
		return err;

	/* Should never fail */
	*pool = M_sql_connpool_init(driver, username, password, flags);

	if (!M_sql_connpool_add_subpool(*pool, M_FALSE, conn_str, max_conns, error, error_size)) {
		err = M_SQL_ERROR_INVALID_USE;
		goto done;
	}

done:
	if (err != M_SQL_ERROR_SUCCESS) {
		M_sql_connpool_destroy(*pool);
		*pool = NULL;
	}

	return err;
}


M_sql_error_t M_sql_connpool_add_readonly_pool(M_sql_connpool_t *pool, const char *conn_str, size_t max_conns, char *error, size_t error_size)
{
	M_sql_error_t err = M_SQL_ERROR_SUCCESS;

	if (pool == NULL) {
		return M_SQL_ERROR_INVALID_USE;
	}

	if (M_str_isempty(conn_str)) {
		M_snprintf(error, error_size, "must specify a valid connection string");
		return M_SQL_ERROR_INVALID_USE;
	}

	if (max_conns == 0) {
		M_snprintf(error, error_size, "must specify maximum number of connections greater than 0");
		return M_SQL_ERROR_INVALID_USE;
	}

	M_thread_mutex_lock(pool->lock);
	if (pool->started) {
		err = M_SQL_ERROR_INVALID_USE;
		M_snprintf(error, error_size, "%s", "Pool is already started, cannot add readonly pool");
	} else {
		if (!M_sql_connpool_add_subpool(pool, M_TRUE, conn_str, max_conns, error, error_size))
			err = M_SQL_ERROR_INVALID_USE;
	}
	M_thread_mutex_unlock(pool->lock);

	return err;
}


M_bool M_sql_connpool_add_trace(M_sql_connpool_t *pool, M_sql_trace_cb_t cb, void *cb_arg)
{
	if (pool == NULL || cb == NULL)
		return M_FALSE;

	M_thread_mutex_lock(pool->lock);
	if (pool->started) {
		M_thread_mutex_unlock(pool->lock);
		return M_FALSE;
	}

	pool->trace_cb     = cb;
	pool->trace_cb_arg = cb_arg;
	M_thread_mutex_unlock(pool->lock);
	return M_TRUE;
}


static M_sql_error_t M_sql_connpool_stop(M_sql_connpool_t *pool)
{
	M_sql_conn_t *conn;

	M_thread_mutex_lock(pool->lock);

	/* If in active use, fail to destroy */
	if (pool->pool_primary.used_conns || pool->pool_primary.num_waiters ||
	    pool->pool_readonly.used_conns || pool->pool_readonly.num_waiters) {
		M_thread_mutex_unlock(pool->lock);
		return M_SQL_ERROR_INUSE;
	}

	/* Disconnect all connections */
	while ((conn = M_llist_take_node(M_llist_first(pool->pool_primary.conns))) != NULL) {
		M_sql_conn_destroy(conn, M_TRUE);
	}
	while ((conn = M_llist_take_node(M_llist_first(pool->pool_readonly.conns))) != NULL) {
		M_sql_conn_destroy(conn, M_TRUE);
	}

	pool->started = M_FALSE;
	M_thread_mutex_unlock(pool->lock);

	return M_SQL_ERROR_SUCCESS;
}


M_sql_error_t M_sql_connpool_start(M_sql_connpool_t *pool, char *error, size_t error_size)
{
	M_sql_error_t err = M_SQL_ERROR_SUCCESS;

	M_thread_mutex_lock(pool->lock);
	if (pool->started) {
		err = M_SQL_ERROR_INVALID_USE;
		M_snprintf(error, error_size, "%s", "pool already started");
	} else {
		pool->started = M_TRUE;
	}
	M_thread_mutex_unlock(pool->lock);

	if (err != M_SQL_ERROR_SUCCESS)
		return err;

	err   = M_sql_connpool_spawn(pool, &pool->pool_primary, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS) {
		goto done;
	}

	err   = M_sql_connpool_spawn(pool, &pool->pool_readonly, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS) {
		goto done;
	}

done:
	if (err != M_SQL_ERROR_SUCCESS) {
		M_sql_connpool_stop(pool);
	}
	return err;
}


const char *M_sql_connpool_server_version(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return NULL;
	return pool->sql_serverversion;
}


const char *M_sql_connpool_driver_display_name(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return NULL;
	return pool->driver->display_name;
}


const char *M_sql_connpool_driver_name(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return NULL;
	return pool->driver->name;
}


const char *M_sql_connpool_driver_version(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return NULL;
	return pool->driver->version;
}


M_sql_connpool_flags_t M_sql_connpool_flags(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return M_SQL_CONNPOOL_FLAG_NONE;
	return pool->flags;
}


void M_sql_connpool_set_timeouts(M_sql_connpool_t *pool, M_time_t reconnect_time_s, M_time_t max_idle_time_s, M_time_t fallback_s)
{
	if (pool == NULL)
		return;

	M_thread_mutex_lock(pool->lock);

	if (reconnect_time_s >= 0)
		pool->reconnect_time_s = reconnect_time_s;

	if (max_idle_time_s >= 0)
		pool->max_idle_time_s = max_idle_time_s;

	if (fallback_s >= 0)
		pool->fallback_s = fallback_s;

	M_thread_mutex_unlock(pool->lock);
}


M_sql_error_t M_sql_connpool_destroy(M_sql_connpool_t *pool)
{
	M_sql_error_t err;

	if (pool == NULL)
		return M_SQL_ERROR_SUCCESS;

	err = M_sql_connpool_stop(pool);
	if (err != M_SQL_ERROR_SUCCESS)
		return err;

	/* Clean up */
	M_llist_destroy(pool->pool_primary.conns, M_TRUE);
	M_llist_destroy(pool->pool_readonly.conns, M_TRUE);
	M_free(pool->pool_primary.info);
	M_free(pool->pool_readonly.info);
	M_thread_cond_destroy(pool->pool_primary.cond);
	M_thread_cond_destroy(pool->pool_readonly.cond);
	M_free(pool->pool_primary.host_offline_t);
	M_free(pool->pool_readonly.host_offline_t);

	M_free(pool->username);
	M_free(pool->password);
	M_free(pool->sql_serverversion);
	pool->driver->cb_destroypool(pool->dpool);
	M_rand_destroy(pool->rand);
	M_hash_strvp_destroy(pool->group_insert, M_TRUE);
	M_thread_mutex_destroy(pool->lock);
	M_free(pool);
	return M_SQL_ERROR_SUCCESS;
}


size_t M_sql_connpool_active_conns(M_sql_connpool_t *pool, M_bool readonly)
{
	size_t                 cnt;
	M_sql_connpool_data_t *pool_data;
	if (pool == NULL)
		return 0;

	M_thread_mutex_lock(pool->lock);

	if (readonly) {
		pool_data = &pool->pool_readonly;
	} else {
		pool_data = &pool->pool_primary;
	}
	cnt = M_llist_len(pool_data->conns) + pool_data->used_conns;

	M_thread_mutex_unlock(pool->lock);

	return cnt;
}


static size_t M_sql_connpool_get_unused_id(M_sql_connpool_data_t *pool_data)
{
	size_t i;

	for (i=0; i<pool_data->max_conns; i++) {
		if (pool_data->info[i] != M_SQL_CONN_INFO_UP)
			return i;
	}

	return 0;
}


M_sql_conn_t *M_sql_connpool_acquire_conn(M_sql_connpool_t *pool, M_bool readonly, M_bool for_trans)
{
	M_bool                 just_woken     = M_FALSE;
	M_bool                 newconn_failed = M_FALSE;
	M_sql_conn_t          *conn           = NULL;
	M_sql_connpool_data_t *pool_data      = NULL;
	size_t                 id             = 0;

	if (pool == NULL)
		return NULL;

	do {
		M_thread_mutex_lock(pool->lock);

		if (!pool->started) {
			M_thread_mutex_unlock(pool->lock);
			return NULL;
		}

		/* Select appropriate pool */
		if (readonly && pool->pool_readonly.max_conns > 0) {
			pool_data = &pool->pool_readonly;
		} else {
			pool_data = &pool->pool_primary;
			readonly  = M_FALSE; /* Override */
		}

		/* Ugh, we just tried to reconnect, and it failed. We need to decrement
		 * the used_conns counter and de-reserve the id that failed and try the
		 * whole shebang over again */
		if (newconn_failed) {
			pool_data->used_conns--;
			pool_data->info[id] = M_SQL_CONN_INFO_FAILED;
			newconn_failed      = M_FALSE;
		}

		/* Loop until there's an available connection, and we don't want to accidentally
		 * not wait our turn, so if there's waiters, wait our turn.  If connections are
		 * growable, then we might need to spawn a new one. */
		while ((pool_data->num_waiters && !just_woken) ||
		       (M_llist_len(pool_data->conns) == 0 && pool_data->used_conns == pool_data->max_conns)) {
			pool_data->num_waiters++;
			M_thread_cond_wait(pool_data->cond, pool->lock);
			pool_data->num_waiters--;
			just_woken = M_TRUE;
		}

		conn = M_llist_take_node(M_llist_first(pool_data->conns));

		/* Check conn for max_idle_time_s */
		if (conn && pool->max_idle_time_s > 0 && (M_time_elapsed(&conn->last_used_tv) / 1000) > (M_uint64)pool->max_idle_time_s) {
			/* We hit a connection sitting idle too long, we actually need to destroy it and see if there's
			 * a less stale one (if not, it should end up auto-creating a new one on the next loop) */
			pool_data->info[conn->id] = M_SQL_CONN_INFO_NEW;
			M_sql_conn_destroy(conn, M_TRUE);
			M_thread_mutex_unlock(pool->lock);
			conn = NULL;
			continue;
		}

		pool_data->used_conns++;

		/* We're going to establish a new connection, we need to reserve an
		 * id while we hold the pool lock */
		if (conn == NULL) {
			id                  = M_sql_connpool_get_unused_id(pool_data);
			pool_data->info[id] = M_SQL_CONN_INFO_UP;
		}

		M_thread_mutex_unlock(pool->lock);

		/* If conn is NULL here, that means we've been told to spawn a new connection,
		 * used_conns has already be incremented while the pool lock was held so we
		 * know too many won't be spawned. */
		/* Successfully retrieved a connection */
		if (conn == NULL) {
			if (M_sql_error_is_error(M_sql_conn_create(&conn, pool, id, readonly, NULL, 0))) {
				newconn_failed = M_TRUE;
				M_thread_sleep(100000);
			}
		}

	} while(newconn_failed || conn == NULL);

	conn->in_trans = for_trans;

	return conn;
}


void M_sql_connpool_release_conn(M_sql_conn_t *conn)
{
	M_sql_connpool_t      *pool;
	M_sql_connpool_data_t *pool_data;
	M_bool                 is_readonly;

	if (conn == NULL)
		return;

	pool        = conn->pool;
	pool_data   = conn->pool_data;

	M_thread_mutex_lock(pool->lock);

	is_readonly = (&pool->pool_readonly == pool_data); 

	/* Connection is no longer used/reserved ... this is mostly for dynamic
	 * reconnect purposes */
	pool_data->used_conns--;

	/* If connection is failed, destroy it */
	if (M_sql_conn_get_state(conn) == M_SQL_CONN_STATE_FAILED) {
		pool_data->info[conn->id] = M_SQL_CONN_INFO_FAILED;
		M_sql_connpool_mark_host_idx_failed(pool, conn->host_idx, is_readonly);
		M_sql_conn_destroy(conn, M_FALSE);
	} else if (pool->reconnect_time_s > 0 && (M_time_elapsed(&conn->start_tv) / 1000) > (M_uint64)pool->reconnect_time_s) {
		/* Force reconnect due to maximum uptime.  Used for rebalancing. */
		pool_data->info[conn->id] = M_SQL_CONN_INFO_NEW;
		M_sql_conn_destroy(conn, M_TRUE);
	} else {
		/* Might be set to rollback, clear condition as we're guaranteed it was
		 * rolled back if we're here. */
		M_sql_conn_set_state(conn, M_SQL_CONN_STATE_OK);

		/* Keep last used time to track max_idle_time_s */
		M_time_elapsed_start(&conn->last_used_tv);

		/* Make sure we unset the in_trans */
		conn->in_trans = M_FALSE;

		/* Return to pool */
		M_llist_insert(pool_data->conns, conn);
	}

	M_thread_cond_signal(pool_data->cond);

	M_thread_mutex_unlock(pool->lock);
}


const M_sql_driver_t *M_sql_connpool_get_driver(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return NULL;

	return pool->driver;
}


const M_sql_driver_t *M_sql_conn_get_driver(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return NULL;

	return conn->pool->driver;
}


M_sql_trace_cb_t M_sql_connpool_get_cb(M_sql_connpool_t *pool, void **cb_arg)
{
	if (pool == NULL)
		return NULL;

	*cb_arg = pool->trace_cb_arg;
	return pool->trace_cb;
}


M_sql_conn_state_t M_sql_conn_get_state(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return M_SQL_CONN_STATE_FAILED;
	return conn->state;
}


M_bool M_sql_driver_conn_is_readonly(M_sql_conn_t *conn)
{
	if (&conn->pool->pool_readonly == conn->pool_data)
		return M_TRUE;
	return M_FALSE;
}


size_t M_sql_driver_conn_get_id(M_sql_conn_t *conn)
{
	return conn->id;
}


void M_sql_conn_set_state(M_sql_conn_t *conn, M_sql_conn_state_t state)
{
	if (conn == NULL)
		return;
	conn->state = state;
}


void M_sql_conn_set_state_from_error(M_sql_conn_t *conn, M_sql_error_t err)
{
	if (conn == NULL)
		return;

	if (M_sql_error_is_disconnect(err))
		conn->state = M_SQL_CONN_STATE_FAILED;
	if (M_sql_error_is_rollback(err))
		conn->state = M_SQL_CONN_STATE_ROLLBACK;

	/* Nothing else should change the state */
}


M_sql_driver_conn_t *M_sql_driver_conn_get_conn(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return NULL;
	return conn->conn;
}


M_sql_connpool_t *M_sql_driver_conn_get_pool(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return NULL;
	return conn->pool;
}


M_sql_driver_connpool_t *M_sql_driver_pool_get_dpool(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return NULL;
	return pool->dpool;
}


M_sql_driver_connpool_t *M_sql_driver_conn_get_dpool(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return NULL;
	return M_sql_driver_pool_get_dpool(conn->pool);
}


const char *M_sql_driver_pool_get_username(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return NULL;
	return pool->username;
}


const char *M_sql_driver_pool_get_password(M_sql_connpool_t *pool)
{
	if (pool == NULL)
		return NULL;
	return pool->password;
}


const char *M_sql_driver_conn_get_username(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return NULL;
	return M_sql_driver_pool_get_username(conn->pool);
}


const char *M_sql_driver_conn_get_password(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return NULL;
	return M_sql_driver_pool_get_username(conn->pool);
}


M_uint64 M_sql_conn_duration_start_ms(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return 0;

	return M_time_elapsed(&conn->start_tv);
}


M_uint64 M_sql_conn_duration_last_ms(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return 0;

	return M_time_elapsed(&conn->last_used_tv);
}


M_bool M_sql_driver_conn_in_trans(M_sql_conn_t *conn)
{
	if (conn == NULL)
		return M_FALSE;
	return conn->in_trans;
}


M_sql_driver_stmt_t *M_sql_conn_get_stmt_cache(M_sql_conn_t *conn, const char *query)
{
	M_sql_stmt_cache_t *cache;

	if (conn == NULL || M_str_isempty(query))
		return NULL;

	cache = M_cache_strvp_get_direct(conn->stmt_cache, query);

	if (cache == NULL)
		return NULL;

	return cache->stmt;
}


void M_sql_conn_set_stmt_cache(M_sql_conn_t *conn, const char *query, M_sql_driver_stmt_t *stmt)
{
	M_sql_stmt_cache_t *cache;

	if (conn == NULL || M_str_isempty(query))
		return;

	cache = M_cache_strvp_get_direct(conn->stmt_cache, query);

	if (cache != NULL) {
		/* If statement handles match, no-op */
		if (stmt == cache->stmt) {
			return;
		}

		/* If we have a cached value, and the input statement handle doesn't match, clear existing */
		/* XXX: if the connection is locked by a transaction, we really need to delay this as
		 *      some servers may execute commands within a txn */
		if (stmt != cache->stmt) {
			M_cache_strvp_remove(conn->stmt_cache, query);
		}
	}

	/* Don't cache a NULL handle */
	if (stmt == NULL)
		return;

	cache = M_malloc_zero(sizeof(*cache));
	cache->conn = conn;
	cache->stmt = stmt;
	M_cache_strvp_insert(conn->stmt_cache, query, cache);

	return;
}


M_int64 M_sql_gen_timerand_id(M_sql_connpool_t *pool, size_t max_len)
{
	M_int64       val = 0;
	M_time_gmtm_t gmt;

	if (pool == NULL || max_len == 0)
		return 0;

	if (max_len > 18)
		max_len = 18;

	M_time_togm(M_time(), &gmt);

	/* Formats:
	 * - 17-18+ digits : YYYJJJSSSSS{6-7R}
	 * - 16 digits     : YYJJJSSSSS{6R}
	 * - 14-15 digits  : YJJJSSSSS{5-6R}
	 * - 13 digits     : YJJJSSSS{6R}
	 * - 11-12 digits  : YJJJSS{6-7R}
	 * - 9-10 digits   : YJJJ{5-6R}
	 * - 1-8 digits    : {1-8R}
	 */

	/* Output Year */
	switch (max_len) {
		/* YYY */
		case 18:
		case 17:
			val += gmt.year % 1000;
			break;

		/* YY */
		case 16:
			val += gmt.year % 100;
			break;

		/* Y */
		default:
			val += gmt.year % 10;
			break;
	}

	switch (max_len) {
		case 18:
		case 17:
		case 16:
		case 15:
		case 14:
		case 13:
		case 12:
		case 11:
		case 10:
		case 9:
			/* Output Julian Day  */
			val *= 1000;
			val += gmt.yday;
			break;
		default:
			break;
	}

	/* Output Seconds */
	switch (max_len) {
		/* SSSSS */
		case 18:
		case 17:
		case 16:
		case 15:
		case 14:
			val *= 100000;
			val += (gmt.hour * 3600) + (gmt.min * 60) + gmt.sec;
			break;

		/* SSSS */
		case 13:
			val *= 10000;
			val += ((gmt.hour * 3600) + (gmt.min * 60) + gmt.sec) / 10;
			break;

		/* SS */
		case 12:
		case 11:
			val *= 100;
			val += ((gmt.hour * 3600) + (gmt.min * 60) + gmt.sec) / 1000;
			break;

		/* NONE */
		default:
			break;
	}

	M_thread_mutex_lock(pool->lock);

	/* Output Random */
	switch (max_len) {
		/* 7R */
		case 18:
		case 12:
			val *= 10000000;
			val += (M_int64)M_rand_max(pool->rand, 9999999+1); /* Max of M_rand_max() is really max-1, so +1 here */
			break;

		/* 5R */
		case 14:
		case 9:
			val *= 100000;
			val += (M_int64)M_rand_max(pool->rand, 99999+1); /* Max of M_rand_max() is really max-1, so +1 here */
			break;

		/* 6R */
		case 17:
		case 16:
		case 15:
		case 13:
		case 11:
		case 10:
			val *= 1000000;
			val += (M_int64)M_rand_max(pool->rand, 999999+1); /* Max of M_rand_max() is really max-1, so +1 here */
			break;

		/* Short value is all random */
		default:
			val = (M_int64)M_rand_max(pool->rand, M_uint64_exp(10, (int)max_len));
			break;
	}

	M_thread_mutex_unlock(pool->lock);

	return val;
}


M_uint64 M_sql_rollback_delay_ms(M_sql_connpool_t *pool)
{
	M_uint64 val;

	if (pool == NULL)
		return 0;

	M_thread_mutex_lock(pool->lock);
	val = M_rand_range(pool->rand, 15, 100);
	M_thread_mutex_unlock(pool->lock);

	return val;
}


M_sql_stmt_t *M_sql_connpool_get_groupinsert(M_sql_connpool_t *pool, const char *query)
{
	M_sql_stmt_t *stmt;

	if (pool == NULL || M_str_isempty(query))
		return NULL;

	M_thread_mutex_lock(pool->lock);
	stmt = M_hash_strvp_get_direct(pool->group_insert, query);
	if (stmt == NULL) {
		/* Hold lock on pool and return */
		return NULL;
	}

	/* Get statement handle lock before releasing pool lock */
	M_thread_mutex_lock(stmt->group_lock);
	M_thread_mutex_unlock(pool->lock);
	return stmt;
}


void M_sql_connpool_set_groupinsert(M_sql_connpool_t *pool, const char *query, M_sql_stmt_t *stmt)
{
	if (pool == NULL || M_str_isempty(query) || stmt == NULL)
		return;

	/* Pool is locked on entry, no need to relock */
	M_hash_strvp_insert(pool->group_insert, query, stmt);

	/* Unlock pool handle as that is what the docs say to do for this function */
	M_thread_mutex_unlock(pool->lock);
}



void M_sql_connpool_remove_groupinsert(M_sql_connpool_t *pool, const char *query, M_sql_stmt_t *stmt)
{
	if (pool == NULL || M_str_isempty(query) || stmt == NULL)
		return;

	/* Lock order is pool->stmt, so we must unlock the statement to lock the pool before
	 * we can remove the entry */
	M_thread_mutex_unlock(stmt->group_lock);

	M_thread_mutex_lock(pool->lock);
	M_hash_strvp_remove(pool->group_insert, query, M_TRUE);

	/* Re-lock the statement handle so we can execute */
	M_thread_mutex_lock(stmt->group_lock);

	/* Release the pool lock since we always wanted the lock order to be pool->stmt */
	M_thread_mutex_unlock(pool->lock);
}
