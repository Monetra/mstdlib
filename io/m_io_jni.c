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
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"
#include <mstdlib/io/m_io_jni.h>

#ifdef ANDROID
#  include <android/log.h> 
#endif

JavaVM  *M_io_jni_jvm = NULL;


#define M_IO_JNI_TYPE_ARRAY_VAL_OFFSET ('A' - 'a')
enum M_IO_JNI_TYPE {
	M_IO_JNI_UNKNOWN = 0,
	M_IO_JNI_VOID    = 'V',
	M_IO_JNI_OBJECT  = 'L',
	M_IO_JNI_BOOL    = 'Z',
	M_IO_JNI_BYTE    = 'B',
	M_IO_JNI_CHAR    = 'C',
	M_IO_JNI_SHORT   = 'S',
	M_IO_JNI_INT     = 'I',
	M_IO_JNI_LONG    = 'J',
	M_IO_JNI_FLOAT   = 'F',
	M_IO_JNI_DOUBLE  = 'D',
	M_IO_JNI_ARRAY   = '[',

	M_IO_JNI_ARRAY_OBJECT = M_IO_JNI_OBJECT + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET,
	M_IO_JNI_ARRAY_BOOL   = M_IO_JNI_BOOL   + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET,
	M_IO_JNI_ARRAY_BYTE   = M_IO_JNI_BYTE   + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET,
	M_IO_JNI_ARRAY_CHAR   = M_IO_JNI_CHAR   + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET,
	M_IO_JNI_ARRAY_SHORT  = M_IO_JNI_SHORT  + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET,
	M_IO_JNI_ARRAY_INT    = M_IO_JNI_INT    + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET,
	M_IO_JNI_ARRAY_LONG   = M_IO_JNI_LONG   + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET,
	M_IO_JNI_ARRAY_FLOAT  = M_IO_JNI_FLOAT  + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET,
	M_IO_JNI_ARRAY_DOUBLE = M_IO_JNI_DOUBLE + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET
};


typedef struct {
	jmethodID          methodid;
	jclass             cls;
	enum M_IO_JNI_TYPE return_type;
	M_bool             is_static;
	size_t             argc;
} M_io_jni_method_t;


typedef struct {
	M_hash_strvp_t *classes;
	M_hash_strvp_t *methods;
} M_io_jni_references_t;

static M_io_jni_references_t M_io_jni_ref     = { NULL, NULL };
static M_hash_u64str_t      *M_io_jni_threads = NULL;
static M_thread_mutex_t     *M_io_jni_lock    = NULL;

static const struct {
	const char *class;
	const char *method;
	const char *alias;
	const char *signature;
	M_bool      is_static;
} M_io_jni_lookups[] = {
	{ "java/io/InputStream",                     "read",                               NULL,              "([BII)I",                                                                       M_FALSE },
	{ "java/io/InputStream",                     "available",                          NULL,              "()I",                                                                           M_FALSE },
	{ "java/io/OutputStream",                    "write",                              NULL,              "([BII)V",                                                                       M_FALSE },
	{ "java/io/OutputStream",                    "flush",                              NULL,              "()V",                                                                           M_FALSE },
	{ "java/util/HashMap",                       "get",                                NULL,              "(Ljava/lang/Object;)Ljava/lang/Object;",                                        M_FALSE },
	{ "java/util/HashMap",                       "keySet",                             NULL,              "()Ljava/util/Set;",                                                             M_FALSE },
	{ "java/util/HashMap",                       "<init>",                             NULL,              "()V",                                                                           M_FALSE },
	{ "java/util/HashMap",                       "put",                                NULL,              "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;",                      M_FALSE },
	{ "java/util/Set",                           "toArray",                            NULL,              "()[Ljava/lang/Object;",                                                         M_FALSE },
	{ "java/util/UUID",                          "fromString",                         NULL,              "(Ljava/lang/String;)Ljava/util/UUID;",                                          M_TRUE  },
#ifdef ANDROID
	{ "android/os/ParcelUuid",                   "toString",                           NULL,              "()Ljava/lang/String;",                                                          M_FALSE },
	{ "android/os/ParcelUuid",                   "getUuid",                            NULL,              "()Ljava/util/UUID;",                                                            M_FALSE },
	{ "android/bluetooth/BluetoothAdapter",      "getDefaultAdapter",                  NULL,              "()Landroid/bluetooth/BluetoothAdapter;",                                        M_TRUE  },
	{ "android/bluetooth/BluetoothAdapter",      "isEnabled",                          NULL,              "()Z",                                                                           M_FALSE },
	{ "android/bluetooth/BluetoothAdapter",      "getBondedDevices",                   NULL,              "()Ljava/util/Set;",                                                             M_FALSE },
	{ "android/bluetooth/BluetoothAdapter",      "checkBluetoothAddress",              NULL,              "(Ljava/lang/String;)Z",                                                         M_TRUE  },
	{ "android/bluetooth/BluetoothAdapter",      "cancelDiscovery",                    NULL,              "()Z",                                                                           M_FALSE },
	{ "android/bluetooth/BluetoothAdapter",      "getRemoteDevice",                    NULL,              "(Ljava/lang/String;)Landroid/bluetooth/BluetoothDevice;",                       M_FALSE },
	{ "android/bluetooth/BluetoothAdapter",      "listenUsingRfcommWithServiceRecord", NULL,              "(Ljava/lang/String;Ljava/util/UUID;)Landroid/bluetooth/BluetoothServerSocket;", M_FALSE },
	{ "android/bluetooth/BluetoothDevice",       "getName",                            NULL,              "()Ljava/lang/String;",                                                          M_FALSE },
	{ "android/bluetooth/BluetoothDevice",       "getAddress",                         NULL,              "()Ljava/lang/String;",                                                          M_FALSE },
	{ "android/bluetooth/BluetoothDevice",       "getUuids",                           NULL,              "()[Landroid/os/ParcelUuid;",                                                    M_FALSE },
	{ "android/bluetooth/BluetoothDevice",       "createRfcommSocketToServiceRecord",  NULL,              "(Ljava/util/UUID;)Landroid/bluetooth/BluetoothSocket;",                         M_FALSE },
	{ "android/bluetooth/BluetoothServerSocket", "accept",                             NULL,              "()Landroid/bluetooth/BluetoothSocket;",                                         M_FALSE },
	{ "android/bluetooth/BluetoothServerSocket", "accept",                             "accept(timeout)", "(I)Landroid/bluetooth/BluetoothSocket;",                                        M_FALSE },
	{ "android/bluetooth/BluetoothSocket",       "connect",                            NULL,              "()V",                                                                           M_FALSE },
	{ "android/bluetooth/BluetoothSocket",       "close",                              NULL,              "()V",                                                                           M_FALSE },
	{ "android/bluetooth/BluetoothSocket",       "getInputStream",                     NULL,              "()Ljava/io/InputStream;",                                                       M_FALSE },
	{ "android/bluetooth/BluetoothSocket",       "getOutputStream",                    NULL,              "()Ljava/io/OutputStream;",                                                      M_FALSE },
#endif
	{ NULL,                                      NULL,                                 NULL,              NULL,                                                                            M_FALSE }
};


void M_io_jni_debug(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

#ifdef DEBUG
#  ifdef ANDROID
	__android_log_vprint(ANDROID_LOG_DEBUG, "mstdlib_jni", fmt, ap); 
#  else
	M_vfprintf(stderr, fmt, ap);
	M_fprintf(stderr, "\n");
#  endif
#endif

	va_end(ap);
}


static enum M_IO_JNI_TYPE M_io_jni_sig_return_type(const char *signature)
{
	const char        *ptr;
	enum M_IO_JNI_TYPE type;

	ptr = strchr(signature, ')');
	if (ptr == NULL)
		return M_IO_JNI_UNKNOWN;

	ptr++;
	switch (*ptr) {
		case M_IO_JNI_VOID:
		case M_IO_JNI_OBJECT:
		case M_IO_JNI_BOOL:
		case M_IO_JNI_BYTE:
		case M_IO_JNI_CHAR:
		case M_IO_JNI_SHORT:
		case M_IO_JNI_INT:
		case M_IO_JNI_LONG:
		case M_IO_JNI_FLOAT:
		case M_IO_JNI_DOUBLE:
		case M_IO_JNI_ARRAY:
			type = *ptr;
			break;
		default:
			return M_IO_JNI_UNKNOWN;
	}

	if (type == M_IO_JNI_ARRAY) {
		ptr++;
		switch (*ptr) {
			case M_IO_JNI_OBJECT:
			case M_IO_JNI_BOOL:
			case M_IO_JNI_BYTE:
			case M_IO_JNI_CHAR:
			case M_IO_JNI_SHORT:
			case M_IO_JNI_INT:
			case M_IO_JNI_LONG:
			case M_IO_JNI_FLOAT:
			case M_IO_JNI_DOUBLE:
				type = *ptr + M_IO_JNI_TYPE_ARRAY_VAL_OFFSET;
				break;
			default:
				return M_IO_JNI_UNKNOWN;
		}
	}
	return type;
}


static M_bool M_io_jni_sig_arg_count(const char *signature, size_t *cnt)
{
	if (cnt == NULL || M_str_isempty(signature) || *signature != '(')
		return M_FALSE;

	/* Skip past opening '(' */
	signature++;

	(*cnt) = 0;

	for ( ; signature != NULL && *signature != '\0'; signature++) {
		switch (*signature) {
			case 'L':
				signature = strchr(signature, ';');
				if (!signature)
					return M_FALSE; /* ERROR */
				/* Fall-Thru */
			case 'V':
			case 'Z':
			case 'B':
			case 'C':
			case 'S':
			case 'I':
			case 'J':
			case 'F':
			case 'D':
				(*cnt)++;
				break;
			case '[':
				/* Ignore array start, next character is what matters */
				break;
			case ')':
				return M_TRUE; /* End of arguments */
			default:
				/* Unrecognized, error */
				return M_FALSE;
		}
	}

	return M_TRUE;
}


JNIEnv *M_io_jni_getenv(void)
{
	JNIEnv *env        = NULL;
	int     getEnvStat;

	if (M_io_jni_jvm == NULL) {
		M_io_jni_debug("JVM not initialized");
		return NULL;
	}

	getEnvStat = (*M_io_jni_jvm)->GetEnv(M_io_jni_jvm, (void **)&env, JNI_VERSION_1_6);
	if (getEnvStat == JNI_EDETACHED) {
		if ((*M_io_jni_jvm)->AttachCurrentThread(M_io_jni_jvm, &env, NULL) != 0) {
			M_io_jni_debug("Failed to attach current thread to JVM");
			return NULL;
		}
		/* Save the fact that *we* did the initialization in case some other JNI
		 * app did it, we don't want to clear it out from under them. */
		M_thread_mutex_lock(M_io_jni_lock);
		M_hash_u64str_insert(M_io_jni_threads, (M_uint64)((M_uintptr)M_thread_self()), "1");
		M_thread_mutex_unlock(M_io_jni_lock);
	} else if (getEnvStat == JNI_EVERSION) {
		M_io_jni_debug("Invalid JNI version");
		return NULL;
	}
	/* JNI_OK */
	return env;
}


static M_bool M_io_jni_handle_exception(JNIEnv *env, char *error, size_t error_len)
{
	jthrowable e          = (*env)->ExceptionOccurred(env);
	jclass     eclass     = NULL;
	jmethodID  getMessage = NULL;
	jstring    message    = NULL;
	char      *temp       = NULL;

	if (!e) {
		return M_FALSE;
	}

	/* Clear exception since we can't make other calls until we do */
	(*env)->ExceptionClear(env);

	/* If no error string, we're done */
	if (error == NULL || error_len == 0)
		goto done;

	/* Grab class for exception */
	eclass = (*env)->GetObjectClass(env, e);
	if (eclass == NULL)
		goto done;

	/* Grab method id for getMessage */
	getMessage = (*env)->GetMethodID(env, eclass, "getMessage", "()Ljava/lang/String;");
	if (getMessage == NULL)
		goto done;

	/* Call the getMessage function */
	message = (jstring)(*env)->CallObjectMethod(env, e, getMessage);
	if ((*env)->ExceptionOccurred(env)) {
		(*env)->ExceptionClear(env);
		goto done;
	}
	if (message == NULL)
		goto done;

	/* Convert into C String */
	temp = M_io_jni_jstring_to_pchar(env, message);
	if (temp == NULL)
		goto done;

	M_str_cpy(error, error_len, temp);

done:
	M_io_jni_deletelocalref(env, &e);
	M_io_jni_deletelocalref(env, &eclass);
	M_io_jni_deletelocalref(env, &message);
	M_free(temp);
	return M_TRUE;
}


static void M_io_jni_free_class(void *arg)
{
	jclass   cls = arg;
	JNIEnv  *env    = NULL;

	if (cls == NULL)
		return;

	env = M_io_jni_getenv();
	if (env == NULL)
		return;

	(*env)->DeleteGlobalRef(env, cls);
}


jclass M_io_jni_find_class(JNIEnv *env, const char *path)
{
	char error[256];
	jclass cls;

	if (M_str_isempty(path))
		return NULL;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}

	cls = M_hash_strvp_get_direct(M_io_jni_ref.classes, path);
	if (cls != NULL)
		return cls;


	cls = (*env)->FindClass(env, path);
	if (M_io_jni_handle_exception(env, error, sizeof(error))) {
		M_io_jni_debug("FindClass for %s threw exception: %s", path, error);
		return NULL;
	}


	/* Convert into a global reference */
	cls = (*env)->NewGlobalRef(env, cls);
	if (cls == NULL) {
		M_io_jni_debug("Failed to convert class %s into a global reference", path);
		return NULL;
	}

	M_hash_strvp_insert(M_io_jni_ref.classes, path, cls);
	return cls;
}


static M_bool M_io_jni_register_funcs(void)
{
	size_t  i;
	JNIEnv *env       = NULL;
	M_bool  is_error  = M_FALSE;

	env = M_io_jni_getenv();
	if (env == NULL) {
		M_io_jni_debug("Failed to obtain java environment");
		return M_FALSE;
	}

	M_io_jni_ref.classes = M_hash_strvp_create(16, 75, M_HASH_STRVP_CASECMP, M_io_jni_free_class);
	M_io_jni_ref.methods = M_hash_strvp_create(16, 75, M_HASH_STRVP_CASECMP, M_free);

	for (i=0; M_io_jni_lookups[i].class != NULL; i++) {
		jclass             cls;
		jmethodID          mid;
		enum M_IO_JNI_TYPE return_type;
		size_t             argc;
		M_io_jni_method_t *method;
		char               temp[256];
		char               error[256];

		/* Locate class */
		cls = M_io_jni_find_class(env, M_io_jni_lookups[i].class);
		if (cls == NULL) {
			M_io_jni_debug("Failed to find class %s", M_io_jni_lookups[i].class);
			is_error = M_TRUE;
			continue;
		}

		/* Locate Method */
		return_type = M_io_jni_sig_return_type(M_io_jni_lookups[i].signature);
		if (return_type == M_IO_JNI_UNKNOWN) {
			M_io_jni_debug("Failed to parse return type signature for method %s::%s with signature %s", M_io_jni_lookups[i].class, M_io_jni_lookups[i].method, M_io_jni_lookups[i].signature);
			is_error = M_TRUE;
			continue;
		}

		if (!M_io_jni_sig_arg_count(M_io_jni_lookups[i].signature, &argc)) {
			M_io_jni_debug("Failed to parse argc signature for method %s::%s with signature %s", M_io_jni_lookups[i].class, M_io_jni_lookups[i].method, M_io_jni_lookups[i].signature);
			is_error = M_TRUE;
			continue;
		}

		if (M_io_jni_lookups[i].is_static) {
			mid = (*env)->GetStaticMethodID(env, cls, M_io_jni_lookups[i].method, M_io_jni_lookups[i].signature);
		} else {
			mid = (*env)->GetMethodID(env, cls, M_io_jni_lookups[i].method, M_io_jni_lookups[i].signature);
		}
		if (M_io_jni_handle_exception(env, error, sizeof(error))) {
			M_io_jni_debug("GetMethodID for %s::%s with signature %s threw exception: %s", M_io_jni_lookups[i].class, M_io_jni_lookups[i].method, M_io_jni_lookups[i].signature, error);
			mid = NULL;
		}

		if (mid == NULL) {
			M_io_jni_debug("Failed to find %smethod %s::%s with signature %s", M_io_jni_lookups[i].is_static?"static ":"", M_io_jni_lookups[i].class, M_io_jni_lookups[i].method, M_io_jni_lookups[i].signature);
			is_error = M_TRUE;
			continue;
		}

		method              = M_malloc_zero(sizeof(*method));
		method->methodid    = mid;
		method->cls         = cls;
		method->is_static   = M_io_jni_lookups[i].is_static;
		method->return_type = return_type;
		method->argc        = argc;
		M_snprintf(temp, sizeof(temp), "%s.%s", M_io_jni_lookups[i].class, M_io_jni_lookups[i].method);
		M_hash_strvp_insert(M_io_jni_ref.methods, temp, method);
	}

	/* We delay returning the error so we can output all possible errors in function lookups as the list
	 * could be large and there could be a lot of typos */
	if (is_error)
		return M_FALSE;

	return M_TRUE;
}


static void M_io_jni_detach(void)
{
	JNIEnv *env        = NULL;
	M_bool  has_thread = M_FALSE;

	/* Verify we are the ones that Attached the thread, if not, we should exit */
	M_thread_mutex_lock(M_io_jni_lock);
	has_thread = M_hash_u64str_get(M_io_jni_threads, (M_uint64)((M_uintptr)M_thread_self()), NULL);
	if (has_thread)
		M_hash_u64str_remove(M_io_jni_threads, (M_uint64)((M_uintptr)M_thread_self()));
	M_thread_mutex_unlock(M_io_jni_lock);

	if (!has_thread)
		return;

	/* Make sure someone else didn't detach our thread even though we initialized it */
	(*M_io_jni_jvm)->GetEnv(M_io_jni_jvm, (void **)&env, JNI_VERSION_1_6);
	if (env != NULL)
		(*M_io_jni_jvm)->DetachCurrentThread(M_io_jni_jvm);
}


static void M_io_jni_deinit(void *arg)
{
	(void)arg;
	M_hash_strvp_destroy(M_io_jni_ref.classes, M_TRUE);
	M_hash_strvp_destroy(M_io_jni_ref.methods, M_TRUE);
	M_hash_u64str_destroy(M_io_jni_threads);
	M_io_jni_threads = NULL;
	M_thread_mutex_destroy(M_io_jni_lock);
	M_io_jni_lock    = NULL;
	M_mem_set(&M_io_jni_ref, 0, sizeof(M_io_jni_ref));
	M_io_jni_jvm     = NULL;
}


M_bool M_io_jni_init(JavaVM *jvm)
{
	if (M_io_jni_jvm != NULL)
		return M_FALSE;

	if (jvm == NULL)
		return M_FALSE;

	M_io_jni_jvm = jvm;

	M_io_jni_lock    = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	M_io_jni_threads = M_hash_u64str_create(16, 75, M_HASH_U64STR_NONE);

	M_thread_destructor_insert(M_io_jni_detach);
	if (!M_io_jni_register_funcs()) {
		M_io_jni_deinit(NULL);
		return M_FALSE;
	}
	M_library_cleanup_register(M_io_jni_deinit, NULL);

	return M_TRUE;
}


static M_bool M_io_jni_call(void *rv, enum M_IO_JNI_TYPE rv_type, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, va_list ap)
{
	M_io_jni_method_t *m;
	char               temp[256];
	char               fakeerror[256];

	/* We use this for debug messages, so we need it to have a real buffer */
	if (error == NULL || error_len == 0) {
		error     = fakeerror;
		error_len = sizeof(fakeerror);
	}

	if (rv == NULL && rv_type != M_IO_JNI_VOID) {
		M_snprintf(error, error_len, "Invalid use, wrong rv/type");
		return M_FALSE;
	}

	if (M_str_isempty(method)) {
		M_snprintf(error, error_len, "Invalid use, empty method");
		return M_FALSE;
	}

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL) {
			M_snprintf(error, error_len, "Failed to grab JNIEnv handle");
			return M_FALSE;
		}
	}

	m = M_hash_strvp_get_direct(M_io_jni_ref.methods, method);
	if (m == NULL) {
		M_snprintf(error, error_len, "Method not found");
		M_io_jni_debug("%s: %s", error, method);
		return M_FALSE;
	}

	if (!m->is_static && classobj == NULL) {
		M_snprintf(error, error_len, "Empty class specified for non-static method");
		M_io_jni_debug("%s: %s", error, method);
		return M_FALSE;
	}

	if (rv_type != m->return_type) {
		M_snprintf(error, error_len, "Invalid use, method return type mismatch");
		M_io_jni_debug("%s: %s", error, method);
		return M_FALSE;
	}

	if (argc != m->argc) {
		M_snprintf(error, error_len, "Method argument count mismatch, expected %zu, got %zu", m->argc, argc);
		M_io_jni_debug("%s: %s", error, method);
		return M_FALSE;
	}

	if (m->is_static) {
		switch (m->return_type) {
			case M_IO_JNI_VOID:
				(*env)->CallStaticVoidMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_BOOL:
				*((jboolean *)rv) = (*env)->CallStaticBooleanMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_BYTE:
				*((jbyte *)rv) = (*env)->CallStaticByteMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_CHAR:
				*((jchar *)rv) = (*env)->CallStaticCharMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_SHORT:
				*((jshort *)rv) = (*env)->CallStaticShortMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_INT:
				*((jint *)rv) = (*env)->CallStaticIntMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_LONG:
				*((jlong *)rv) = (*env)->CallStaticLongMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_FLOAT:
				*((jfloat *)rv) = (*env)->CallStaticFloatMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_DOUBLE:
				*((jdouble *)rv) = (*env)->CallStaticDoubleMethodV(env, m->cls, m->methodid, ap);
				break;
			case M_IO_JNI_OBJECT:
			default:
				*((jobject *)rv) = (*env)->CallStaticObjectMethodV(env, m->cls, m->methodid, ap);
				break;
		}
	} else {
		switch (m->return_type) {
			case M_IO_JNI_VOID:
				(*env)->CallVoidMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_BOOL:
				*((jboolean *)rv) = (*env)->CallBooleanMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_BYTE:
				*((jbyte *)rv) = (*env)->CallByteMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_CHAR:
				*((jchar *)rv) = (*env)->CallCharMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_SHORT:
				*((jshort *)rv) = (*env)->CallShortMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_INT:
				*((jint *)rv) = (*env)->CallIntMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_LONG:
				*((jlong *)rv) = (*env)->CallLongMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_FLOAT:
				*((jfloat *)rv) = (*env)->CallFloatMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_DOUBLE:
				*((jdouble *)rv) = (*env)->CallDoubleMethodV(env, classobj, m->methodid, ap);
				break;
			case M_IO_JNI_OBJECT:
			default:
				*((jobject *)rv) = (*env)->CallObjectMethodV(env, classobj, m->methodid, ap);
				break;
		}
	}

	if (M_io_jni_handle_exception(env, temp, sizeof(temp))) {
		M_snprintf(error, error_len, "Exception caught for %s: %s", method, temp);
		M_io_jni_debug(error);
		return M_FALSE;
	}

	return M_TRUE;
}


M_bool M_io_jni_call_jvoid(char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...)
{
	M_bool   ret;
	va_list  ap;

	va_start(ap, argc);
	ret = M_io_jni_call(NULL, M_IO_JNI_VOID, error, error_len, env, classobj, method, argc, ap);
	va_end(ap);

	return ret;
}


#define M_IO_JNI_CALL_TYPE_FUNC(type, id) \
	M_bool M_io_jni_call_##type(type *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...) \
	{ \
		M_bool             ret; \
		va_list            ap; \
		\
		if (rv == NULL) \
			return M_FALSE; \
		\
		*rv = (type)0; \
		\
		va_start(ap, argc); \
		ret = M_io_jni_call((void *)rv, id, error, error_len, env, classobj, method, argc, ap); \
		va_end(ap); \
		\
		return ret; \
	}

M_IO_JNI_CALL_TYPE_FUNC(jobject, M_IO_JNI_OBJECT)
M_IO_JNI_CALL_TYPE_FUNC(jbyte, M_IO_JNI_BYTE)
M_IO_JNI_CALL_TYPE_FUNC(jboolean, M_IO_JNI_BOOL)
M_IO_JNI_CALL_TYPE_FUNC(jchar, M_IO_JNI_CHAR)
M_IO_JNI_CALL_TYPE_FUNC(jint, M_IO_JNI_INT)
M_IO_JNI_CALL_TYPE_FUNC(jlong, M_IO_JNI_LONG)
M_IO_JNI_CALL_TYPE_FUNC(jfloat, M_IO_JNI_FLOAT)
M_IO_JNI_CALL_TYPE_FUNC(jdouble, M_IO_JNI_DOUBLE)
M_IO_JNI_CALL_TYPE_FUNC(jobjectArray, M_IO_JNI_ARRAY_OBJECT)
M_IO_JNI_CALL_TYPE_FUNC(jbyteArray, M_IO_JNI_ARRAY_BYTE)
M_IO_JNI_CALL_TYPE_FUNC(jbooleanArray, M_IO_JNI_ARRAY_BOOL)
M_IO_JNI_CALL_TYPE_FUNC(jcharArray, M_IO_JNI_ARRAY_CHAR)
M_IO_JNI_CALL_TYPE_FUNC(jintArray, M_IO_JNI_ARRAY_INT)
M_IO_JNI_CALL_TYPE_FUNC(jlongArray, M_IO_JNI_ARRAY_LONG)
M_IO_JNI_CALL_TYPE_FUNC(jfloatArray, M_IO_JNI_ARRAY_FLOAT)
M_IO_JNI_CALL_TYPE_FUNC(jdoubleArray, M_IO_JNI_ARRAY_DOUBLE)


M_bool M_io_jni_new_object(jobject *rv, char *error, size_t error_len, JNIEnv *env, const char *method, size_t argc, ...)
{
	M_io_jni_method_t *m;
	va_list            ap;
	char               temp[256];
	char               fakeerror[256];

	/* We use this for debug messages, so we need it to have a real buffer */
	if (error == NULL || error_len == 0) {
		error     = fakeerror;
		error_len = sizeof(fakeerror);
	}

	if (rv == NULL) {
		M_snprintf(error, error_len, "Invalid use, no rv");
		return M_FALSE;
	}

	if (M_str_isempty(method)) {
		M_snprintf(error, error_len, "Invalid use, no method specified");
		return M_FALSE;
	}

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL) {
			M_snprintf(error, error_len, "Failed to grab JNIEnv handle");
			return M_FALSE;
		}
	}

	m = M_hash_strvp_get_direct(M_io_jni_ref.methods, method);
	if (m == NULL) {
		M_snprintf(error, error_len, "Method not found");
		M_io_jni_debug("%s: %s", error, method);
		return M_FALSE;
	}

	if (argc != m->argc) {
		M_snprintf(error, error_len, "Method argument count mismatch, expected %zu, got %zu", m->argc, argc);
		M_io_jni_debug("%s: %s", error, method);
		return M_FALSE;
	}

	va_start(ap, argc);

	*rv = (*env)->NewObjectV(env, m->cls, m->methodid, ap);

	va_end(ap);

	if (M_io_jni_handle_exception(env, temp, sizeof(temp))) {
		M_snprintf(error, error_len, "Exception caught for %s: %s", method, temp);
		M_io_jni_debug(error);
		return M_FALSE;
	}

	return M_TRUE;
}


void M_io_jni_deletelocalref(JNIEnv *env, jobject *ref)
{
	if (ref == NULL || *ref == NULL)
		return;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return;
	}

	(*env)->DeleteLocalRef(env, *ref);
	*ref = NULL;
}


jobject M_io_jni_create_globalref(JNIEnv *env, jobject ref)
{
	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}
	return (*env)->NewGlobalRef(env, ref);
}

void M_io_jni_delete_globalref(JNIEnv *env, jobject *ref)
{
	if (ref == NULL || *ref == NULL)
		return;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return;
	}

	(*env)->DeleteGlobalRef(env, *ref);
	*ref = NULL;
}


size_t M_io_jni_array_length(JNIEnv *env, jobject arr)
{
	jsize size;
	if (arr == NULL)
		return 0;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return 0;
	}

	size = (*env)->GetArrayLength(env, arr);
	if (M_io_jni_handle_exception(env, NULL, 0)) {
		return 0;
	}

	return (size_t)size;
}


jobject M_io_jni_array_element(JNIEnv *env, jobject arr, size_t idx)
{
	jobject ret;

	if (arr == NULL)
		return NULL;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}

	ret = (jstring)(*env)->GetObjectArrayElement(env, arr, (jsize)idx);
	if (M_io_jni_handle_exception(env, NULL, 0)) {
		return NULL;
	}

	return ret;
}


char *M_io_jni_jstring_to_pchar(JNIEnv *env, jstring str)
{
	const char *temp;
	char       *ret;

	if (str == NULL)
		return NULL;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}

	temp = (*env)->GetStringUTFChars(env, str, 0);
	if (M_io_jni_handle_exception(env, NULL, 0)) {
		return NULL;
	}

	if (temp == NULL)
		return NULL;

	ret = M_strdup(temp);

	(*env)->ReleaseStringUTFChars(env, str, temp);

	return ret;
}


jstring M_io_jni_pchar_to_jstring(JNIEnv *env, const char *str)
{
	jstring ret;

	if (str == NULL)
		return NULL;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}

	ret = (*env)->NewStringUTF(env, str);
	if (M_io_jni_handle_exception(env, NULL, 0)) {
		return NULL;
	}

	return ret;
}


unsigned char *M_io_jni_jbyteArray_to_puchar(JNIEnv *env, jbyteArray in, size_t *size_out)
{
	size_t         len;
	unsigned char *ret;

	if (in == NULL || size_out == NULL)
		return NULL;

	*size_out = 0;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}

	len = M_io_jni_array_length(env, in);
	if (len == 0)
		return NULL;

	ret = M_malloc(len);
	(*env)->GetByteArrayRegion(env, in, 0, (jsize)len, (jbyte *)ret);
	if (M_io_jni_handle_exception(env, NULL, 0)) {
		M_free(ret);
		return NULL;
	}

	*size_out = len;
	return ret;
}



jbyteArray M_io_jni_puchar_to_jbyteArray(JNIEnv *env, const unsigned char *data, size_t data_size)
{
	jbyteArray out;

	if (data == NULL || data_size == 0)
		return NULL;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}

	out = (*env)->NewByteArray(env, (jsize)data_size);
	if (out == NULL || M_io_jni_handle_exception(env, NULL, 0)) {
		return NULL;
	}

	(*env)->SetByteArrayRegion(env, out, 0, (jsize)data_size, (const jbyte *)data);

	return out;
}


M_hash_dict_t *M_io_jni_jhashmap_to_mhashdict(JNIEnv *env, jobject map)
{
	M_hash_dict_t    *dict = NULL;
	jobject           set_o       = NULL;
	jobjectArray      str_array_o = NULL;
	size_t            size;
	size_t            i;

	if (map == NULL)
		return NULL;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}

	/* Translate map to dict */
	dict = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP); 

	/* Turn the keys into a set of keys. */
	if (!M_io_jni_call_jobject(&set_o, NULL, 0, env, map, "java/util/HashMap.keySet", 0))
		goto done;

	/* Turn the set of keys into an array of keys. */
	if (!M_io_jni_call_jobjectArray(&str_array_o, NULL, 0, env, set_o, "java/util/Set.toArray", 0))
		goto done;

	size = M_io_jni_array_length(env, str_array_o);
	if (size == 0)
		goto done;

	/* Iterate though the array of keys and put the value for each into the dict. */
	for (i=0; i<size; i++) {
		jstring  key_o = NULL;
		jstring  val_o = NULL;
		char *key      = NULL;
		char *val      = NULL;

		key_o = M_io_jni_array_element(env, str_array_o, i);
		if (key_o == NULL)
			goto loop_cleanup;

		if (!M_io_jni_call_jobject(&val_o, NULL, 0, env, map, "java/util/HashMap.get", 1, key_o))
			goto loop_cleanup;

		key = M_io_jni_jstring_to_pchar(env, key_o);
		val = M_io_jni_jstring_to_pchar(env, val_o);

		M_hash_dict_insert(dict, key, val);

loop_cleanup:
		M_io_jni_deletelocalref(env, &key_o);
		M_io_jni_deletelocalref(env, &val_o);
		M_free(key);
		M_free(val);
	}

done:
	M_io_jni_deletelocalref(env, &set_o);
	M_io_jni_deletelocalref(env, &str_array_o);

	return dict;
}


jobject M_io_jni_mhashdict_to_jhashmap(JNIEnv *env, M_hash_dict_t *dict)
{
	M_hash_dict_enum_t *henum   = NULL;
	const char         *key;
	const char         *val;
	jobject             out_map = NULL;
	M_bool              success = M_FALSE;

	if (dict == NULL)
		return NULL;

	if (env == NULL) {
		env = M_io_jni_getenv();
		if (env == NULL)
			return NULL;
	}

	/* Create and fill output map */
	if (!M_io_jni_new_object(&out_map, NULL, 0, env, "java/util/HashMap.<init>", 0) || out_map == NULL) {
		goto fail;
	}

	M_hash_dict_enumerate(dict, &henum);
	while (M_hash_dict_enumerate_next(dict, henum, &key, &val)) {
		jstring key_o = M_io_jni_pchar_to_jstring(env, key);
		jstring val_o = M_io_jni_pchar_to_jstring(env, val);
		jobject set_o = NULL;
		M_bool  rv    = M_io_jni_call_jobject(&set_o, NULL, 0, env, out_map, "java/util/HashMap.put", 2, key_o, val_o);

		M_io_jni_deletelocalref(env, &key_o);
		M_io_jni_deletelocalref(env, &val_o);
		M_io_jni_deletelocalref(env, &set_o);

		if (!rv)
			goto fail;
	}

	success = M_TRUE;

fail:
	M_hash_dict_enumerate_free(henum);

	if (!success) {
		M_io_jni_deletelocalref(env, &out_map);
	}

	return out_map;
}

