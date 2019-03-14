/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

#ifndef __M_IO_JNI_H__
#define __M_IO_JNI_H__

#include <jni.h>

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

__BEGIN_DECLS

/*! \addtogroup m_io_jni Java JNI helpers
 *  \ingroup m_eventio_semipublic
 *
 * Included using the semi-public header of <mstdlib/io/m_io_jni.h>
 * This is not included by default because it is considered a stable API.
 *
 * Java JNI Helper Functions. Primarily used for Android integration where
 * Bluetooth support requires it. However, nothing here is Android specific
 * and will never be. This is purely JNI and does not use anything outside
 * of what's provided by Java itself.
 *
 * @{
 */


/*! Initialization function to initialize the Java JNI environment. 
 *
 *  This routine must be called once at startup before any of the M_io_jni helper
 *  functions can be used.
 *
 *  This implementation only supports a single Java VM instance, globally.
 * 
 *  \param[in] Jvm  Initialized JavaVM to use.  Must be specified.
 *  \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_io_jni_init(JavaVM *Jvm);


/*! Initialization function to initialize the io system for use on Android.
 *
 * M_io_jni_init must be called before this function.
 * This should only be called when building for Android.
 *
 * This function must be called in order to use USB-HID devices.
 *
 * This function must be called before DNS resolution will work on Android 8
 * (Oreo) or newer when built targeting SDK 26. Also, the ACCESS_NETWORK_STATE
 * permission must be present in the Android application.
 *
 * \param[in] app_context Android application context. Can be accessed in Java from like so:
 *                                 getApplicationContext().
 *
 * \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_io_jni_android_init(jobject app_context);


/*! Retrieve JNI Environment Handle for current thread.
 * 
 *  If thread is not currently assigned a handle, a new one will be created,
 *  otherwise the existing handle will be returned.
 *
 *  \return JNI Environment Handle or NULL on error
 */
M_API JNIEnv *M_io_jni_getenv(void);


/*! Retrieve Android Applilcation Contect.
 *
 * M_io_jni_android_init must be called to set the context.
 *
 * \return Application Context
 */
M_API jobject M_io_jni_get_android_app_context(void);


/*! Output debug text relevant to JNI execution. 
 *
 * If not using a debug build, this is a no-op, and no information will be output.
 * This is mostly used internally by the implementation, but people wishing to
 * implement additional JNI methods might find this useful for debug purposes.
 *
 * On Android, this uses the android logging functions, on other systems this
 * simply outputs the message to stderr. 
 *
 * \param[in] fmt  Standard printf-style format string.  A new line will be
 *                 automatically added to the output.
 * \param[in] ...  Arguments as referenced in the format string
 */
M_API void M_io_jni_debug(const char *fmt, ...);


/*! Look up a class based on its path.
 *
 * \param[in]     env        Optional. Java JNI Environment. If not passed will request it from
 *                           the JVM.  Passing it is an optimization.
 * \param[in]     path       The path for the class, such as java/util/HashMap
 * \return global class reference or NULL on failure.
 */
M_API jclass M_io_jni_find_class(JNIEnv *env, const char *path);


/*! Convert a Java HashMap into an M_hash_dict_t *.
 *  \param[in]     env        Optional. Java JNI Environment. If not passed will request it from
 *                            the JVM.  Passing it is an optimization.
 *  \param[in]     map        Java Hash Map object to convert.
 *  \return Intialized M_hash_dict_t filled in with the map parameters or NULL on error.
 */
M_API M_hash_dict_t *M_io_jni_jhashmap_to_mhashdict(JNIEnv *env, jobject map);


/*! Convert an M_hash_dict_t * into a Java HashMap object.
 *  \param[in]     env        Optional. Java JNI Environment. If not passed will request it from
 *                            the JVM.  Passing it is an optimization.
 *  \param[in]     dict       M_hash_dict_t to convert.
 *  \return HashMap Object or NULL on error.  The returned object should be released with
 *          M_io_jni_deletelocalref() when no longer needed.
 */
M_API jobject M_io_jni_mhashdict_to_jhashmap(JNIEnv *env, M_hash_dict_t *dict);


/*! Delete reference to object so garbage collector can destroy it.
 *
 *  This isn't absolutely necessary to call, but is recommended for long-running routines,
 *  or if using many objects in a loop so you don't run out of descriptors.  When control
 *  returns from JNI back to Java, any used JNI objects not manually deleted will be released
 *  if they were not returned into java scope.  Once an object is deleted it can no longer
 *  be used, nor can it be returned to Java.
 *
 *  \param[in]     env  Optional. Java JNI Environment. If not passed will request it from
 *                      the JVM.  Passing it is an optimization.
 *  \param[in,out] ref  Object reference to be deleted, passed by reference.  Will be set
 *                      pointer will be set to NULL when dereferenced to ensure it won't
 *                      be used again. */
M_API void M_io_jni_deletelocalref(JNIEnv *env, jobject *ref);


/*! Create a global JNI reference to prevent garbage collection of a Java object that may
 *  need to persist past the point where execution is returned to Java.
 *
 *  If a Java object is held within a C object that needs to persist, and integrator
 *  must mark it as a global reference, then delete the global reference when no longer
 *  needed, otherwise the object will be cleaned up by Java.
 *
 *  \param[in]     env  Optional. Java JNI Environment. If not passed will request it from
 *                      the JVM.  Passing it is an optimization.
 *  \param[in]     ref  Java object to create a global reference to.
 *  \return Java object with global reference.  Must be cleaned up with M_io_jni_create_globalref()
 *          or will cause a resource leak.
 */
M_API jobject M_io_jni_create_globalref(JNIEnv *env, jobject ref);


/*! Delete the global JNI reference created with M_io_jni_create_globalref()
 *
 *  \param[in]     env  Optional. Java JNI Environment. If not passed will request it from
 *                      the JVM.  Passing it is an optimization.
 *  \param[in,out] ref  Object reference to be deleted, passed by reference.  Will be set
 *                      pointer will be set to NULL when dereferenced to ensure it won't
 *                      be used again.
 */
M_API void M_io_jni_delete_globalref(JNIEnv *env, jobject *ref);


/*! Retrieve length of Array.
 *  \param[in]      env  Optional. Java JNI Environment. If not passed will request it from
 *                       the JVM.  Passing it is an optimization.
 *  \param[in]      arr  Array object to get count.
 *  \return length of array
 */
M_API size_t M_io_jni_array_length(JNIEnv *env, jobject arr);

/*! Retrieve an element from an Array.
 *  \param[in]      env  Optional. Java JNI Environment. If not passed will request it from
 *                       the JVM.  Passing it is an optimization.
 *  \param[in]      arr  Array to retrieve element from.
 *  \param[in]      idx  Array index.
 *  \return Object retrieved from array or NULL on error.  The returned object should be
 *          released using M_io_jni_deletelocalref() when no longer needed.
 */
M_API jobject M_io_jni_array_element(JNIEnv *env, jobject arr, size_t idx);


/*! Convert jstring into C String (allocated, null terminated).
 *  \param[in]      env  Optional. Java JNI Environment. If not passed will request it from
 *                       the JVM.  Passing it is an optimization.
 *  \param[in]      str  jstring to convert into C String
 *  \return Allocated C String, must be freed with M_free().  NULL on error.
 */
M_API char *M_io_jni_jstring_to_pchar(JNIEnv *env, jstring str);


/*! Convert a C String into a jstring
 *  \param[in]      env  Optional. Java JNI Environment. If not passed will request it from
 *                       the JVM.  Passing it is an optimization.
 *  \param[in]      str  C String to convert into a jstring.
 *  \return jstring object or NULL on error.  The returned object should be released using
 *          M_io_jni_deletelocalref() when no longer needed.
 */
M_API jstring M_io_jni_pchar_to_jstring(JNIEnv *env, const char *str);


/*! Convert a byte array into an unsigned character pointer
 *  \param[in]      env      Optional. Java JNI Environment. If not passed will request it from
 *                           the JVM.  Passing it is an optimization.
 *  \param[in]      in       Byte array to convert to unsigned character data.
 *  \param[out]     size_out Size of returned buffer.
 *  \return unsigned character buffer, must be freed with M_free(). NULL on error.
 */
M_API unsigned char *M_io_jni_jbyteArray_to_puchar(JNIEnv *env, jbyteArray in, size_t *size_out);

/*! Convert an unsigned character buffer into a jbyteArray
 *  \param[in]      env       Optional. Java JNI Environment. If not passed will request it from
 *                            the JVM.  Passing it is an optimization.
 *  \param[in]      data      Unsigned character data.
 *  \param[in]      data_size Size of character data buffer.
 *  \return jbyteArray object or NULL on error.  The returned object should be released using
 *          M_io_jni_deletelocalref() when no longer needed.
 */
M_API jbyteArray M_io_jni_puchar_to_jbyteArray(JNIEnv *env, const unsigned char *data, size_t data_size);

/*! Zeroize a Byte Array.  Typically used for memory security reasons.
 *  \param[in]      env       Optional. Java JNI Environment. If not passed will request it from
 *                            the JVM.  Passing it is an optimization.
 *  \param[in]      arr       Byte Array to zeroize
 */
M_API void M_io_jni_jbyteArray_zeroize(JNIEnv *env, jbyteArray arr);

/*! Create a new object using the specified method.
 *  \param[out]     rv         Returned object, passed by reference.  Returned object should be
 *                             released using M_io_jni_deletelocalref() when no longer needed.
 *  \param[out]     error      Optional. Buffer to hold error message.
 *  \param[in]      error_len  Error buffer size.
 *  \param[in]      env        Optional. Java JNI Environment. If not passed will request it from
 *                             the JVM.  Passing it is an optimization.
 *  \param[in]      method     The class initializer method.  The method specified should
 *                             be in the form of "path/to/class.<init>", and must have been one
 *                             of the classes in the global initialization.
 *  \param[in]      argc       Count of arguments to follow.
 *  \param[in]      ...        Variable argument list depending on method being called.
 *  \return M_TRUE if the method was called successfully, M_FALSE if there was a usage error
 *          or exception.  A value of M_TRUE doesn't mean the returned object was populated,
 *          the call may have resulted in an error that didn't raise an exception.
 */
M_API M_bool M_io_jni_new_object(jobject *rv, char *error, size_t error_len, JNIEnv *env, const char *method, size_t argc, ...);


/*! Call an object method that returns jvoid (no result).
 *  \param[out]     error      Optional. Buffer to hold error message.
 *  \param[in]      error_len  Error buffer size.
 *  \param[in]      env        Optional. Java JNI Environment. If not passed will request it from
 *                             the JVM.  Passing it is an optimization.
 *  \param[in]      classobj   Class object to call method on.  If the method being called is static,
 *                             this parameter will be ignored, so should be passed as NULL.
 *  \param[in]      method     The method to be called.  The method should be in the form of
 *                             "path/to/class.method", and must have been one of the methods in the 
 *                             global initialization.
 *  \param[in]      argc       Count of argument to follow.
 *  \param[in]      ...        Variable argument list depending on method being called.
 *  \return M_TRUE if the method was called successfully, M_FALSE if there was a usage error
 *          or exception. 
 */
M_API M_bool M_io_jni_call_jvoid(char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns jobject.
 *  \param[out]     rv         Returned object, passed by reference.  Returned object should be
 *                             released using M_io_jni_deletelocalref() when no longer needed.
 *  \param[out]     error      Optional. Buffer to hold error message.
 *  \param[in]      error_len  Error buffer size.
 *  \param[in]      env        Optional. Java JNI Environment. If not passed will request it from
 *                             the JVM.  Passing it is an optimization.
 *  \param[in]      classobj   Class object to call method on.  If the method being called is static,
 *                             this parameter will be ignored, so should be passed as NULL.
 *  \param[in]      method     The method to be called.  The method should be in the form of
 *                             "path/to/class.method", and must have been one of the methods in the 
 *                             global initialization.
 *  \param[in]      argc       Count of arguments to follow.
 *  \param[in]      ...        Variable argument list depending on method being called.
 *  \return M_TRUE if the method was called successfully, M_FALSE if there was a usage error
 *          or exception.  A value of M_TRUE doesn't mean the returned object was populated,
 *          the call may have resulted in an error that didn't raise an exception.
 */
M_API M_bool M_io_jni_call_jobject(jobject *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jbyte.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jbyte(jbyte *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jboolean.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jboolean(jboolean *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jchar.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jchar(jchar *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jint.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jint(jint *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jlong.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jlong(jlong *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jfloat.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jfloat(jfloat *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jdouble.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jdouble(jdouble *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jobjectArray.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jobjectArray(jobjectArray *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jbyteArray.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jbyteArray(jbyteArray *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jbooleanArray.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jbooleanArray(jbooleanArray *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jcharArray.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jcharArray(jcharArray *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jintArray.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jintArray(jintArray *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jlongArray.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jlongArray(jlongArray *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jfloatArray.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jfloatArray(jfloatArray *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object method that returns a jdoubleArray.
 *
 *  See M_io_jni_call_jobject() for usage information.
 */
M_API M_bool M_io_jni_call_jdoubleArray(jdoubleArray *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *method, size_t argc, ...);


/*! Call an object field that returns a jobject.
 *  \param[out]     rv         Returned value, passed by reference.  Returned object should be
 *                             released using M_io_jni_deletelocalref() when no longer needed.
 *  \param[out]     error      Optional. Buffer to hold error message.
 *  \param[in]      error_len  Error buffer size.
 *  \param[in]      env        Optional. Java JNI Environment. If not passed will request it from
 *                             the JVM.  Passing it is an optimization.
 *  \param[in]      classobj   Class object to call method on.  If the method being called is static,
 *                             this parameter will be ignored, so should be passed as NULL.
 *  \param[in]      field      The field to be called.  The field should be in the form of
 *                             "path/to/class.field", and must have been one of the fields in the 
 *                             global initialization.
 *  \return M_TRUE if the method was called successfully, M_FALSE if there was a usage error
 *          or exception.  A value of M_TRUE doesn't mean the returned object was populated,
 *          the call may have resulted in an error that didn't raise an exception.
 */
M_API M_bool M_io_jni_call_jobjectField(jobject *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *field);


/*! Call an object field that returns a jbyte.
 *
 *  See M_io_jni_call_jobjectField() for usage information.
 */
M_API M_bool M_io_jni_call_jbyteField(jbyte *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *field);


/*! Call an object field that returns a jboolean.
 *
 *  See M_io_jni_call_jobjectField() for usage information.
 */
M_API M_bool M_io_jni_call_jbooleanField(jboolean *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *field);


/*! Call an object field that returns a jchar.
 *
 *  See M_io_jni_call_jobjectField() for usage information.
 */
M_API M_bool M_io_jni_call_jcharField(jchar *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *field);


/*! Call an object field that returns a jint.
 *
 *  See M_io_jni_call_jobjectField() for usage information.
 */
M_API M_bool M_io_jni_call_jintField(jint *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *field);


/*! Call an object field that returns a jlong.
 *
 *  See M_io_jni_call_jobjectField() for usage information.
 */
M_API M_bool M_io_jni_call_jlongField(jlong *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *field);


/*! Call an object field that returns a jfloat.
 *
 *  See M_io_jni_call_jobjectField() for usage information.
 */
M_API M_bool M_io_jni_call_jfloatField(jfloat *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *field);


/*! Call an object field that returns a jdouble.
 *
 *  See M_io_jni_call_jobjectField() for usage information.
 */
M_API M_bool M_io_jni_call_jdoubleField(jdouble *rv, char *error, size_t error_len, JNIEnv *env, jobject classobj, const char *field);

/*! @} */

__END_DECLS

#endif
