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

#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_bluetooth.h>
#include <mstdlib/io/m_io_jni.h>
#include "m_io_bluetooth_int.h"


struct M_io_handle {
	jobject          socket;
	jobject          instream;
	jobject          outstream;
	M_io_t          *io;
	M_buf_t         *readbuf;
	M_bool           is_timeout;
	M_event_timer_t *timer;
	M_threadid_t     thread;
	M_io_state_t     state;
	M_io_error_t     last_err;
	char             error[256];
};


M_io_bluetooth_enum_t *M_io_bluetooth_enum(void)
{
	JNIEnv                *env        = NULL;
	jobject                bt_adapter = NULL;
	jobject                device_set = NULL;
	jobjectArray           device_arr = NULL;
	size_t                 i;
	size_t                 count;
	jboolean               rv         = 0;
	M_io_bluetooth_enum_t *btenum     = NULL;

	env = M_io_jni_getenv();
	if (env == NULL)
		goto done;

	/* Get bluetooth adapter */
	if (!M_io_jni_call_jobject(&bt_adapter, NULL, 0, env, NULL, "android/bluetooth/BluetoothAdapter.getDefaultAdapter", 0) || bt_adapter == NULL)
		goto done;

	/* Make sure bluetooth adapter is enabled */
	if (!M_io_jni_call_jboolean(&rv, NULL, 0, env, bt_adapter, "android/bluetooth/BluetoothAdapter.isEnabled", 0) || !rv)
		goto done;

	/* Get list of devices as a set */
	if (!M_io_jni_call_jobject(&device_set, NULL, 0, env, bt_adapter, "android/bluetooth/BluetoothAdapter.getBondedDevices", 0) || device_set == NULL)
		goto done;

	/* Convert set of devices to an array */
	if (!M_io_jni_call_jobjectArray(&device_arr, NULL, 0, env, device_set, "java/util/Set.toArray", 0) || device_arr == NULL)
		goto done;

	/* Got this far, we probably have results, create the container */
	btenum = M_io_bluetooth_enum_init();

	/* Iterate across devices */
	count = M_io_jni_array_length(env, device_arr);
	for (i=0; i<count; i++) {
		jobjectArray  uuid_arr   = NULL;
		jobject       device     = NULL;
		size_t        uuid_count = 0;
		jobject       uuid       = NULL;
		jstring       uuid_str   = NULL;
		jstring       name_str   = NULL;
		jstring       mac_str    = NULL;
		char         *c_uuid     = NULL;
		char         *c_mac      = NULL;
		char         *c_name     = NULL;
		M_list_str_t *uuid_l     = NULL;
		size_t        len;
		size_t        j;

		uuid_l = M_list_str_create(M_LIST_STR_NONE);

		/* Grab device from array index */
		device = M_io_jni_array_element(env, device_arr, i);
		if (device == NULL)
			goto cleanup_loop;

		/* Get a list of UUIDs for the device */
		if (!M_io_jni_call_jobjectArray(&uuid_arr, NULL, 0, env, device, "android/bluetooth/BluetoothDevice.getUuids", 0) || uuid_arr == NULL)
			goto cleanup_loop;

		uuid_count = M_io_jni_array_length(env, uuid_arr);
		if (uuid_count == 0)
			goto cleanup_loop;

		for (j=0; j<uuid_count; j++) {
			uuid = M_io_jni_array_element(env, uuid_arr, j);
			if (uuid == NULL) {
				goto cleanup_loop;
			}

			/* Convert UUID to string */
			if (!M_io_jni_call_jobject(&uuid_str, NULL, 0, env, uuid, "android/os/ParcelUuid.toString", 0) || uuid_str == NULL) {
				goto cleanup_loop;
			}

			c_uuid = M_io_jni_jstring_to_pchar(env, uuid_str);
			M_list_str_insert(uuid_l, c_uuid);
			M_io_jni_deletelocalref(env, &uuid_str);
			M_free(c_uuid);
		}


		/* Get friendly name */
		if (!M_io_jni_call_jobject(&name_str, NULL, 0, env, device, "android/bluetooth/BluetoothDevice.getName", 0) || name_str == NULL)
			goto cleanup_loop;

		c_name = M_io_jni_jstring_to_pchar(env, name_str);


		/* Get Mac Address */
		if (!M_io_jni_call_jobject(&mac_str, NULL, 0, env, device, "android/bluetooth/BluetoothDevice.getAddress", 0) || mac_str == NULL)
			goto cleanup_loop;

		c_mac = M_io_jni_jstring_to_pchar(env, mac_str);

		/* Store the result.
 		 *
		 * We can't get the service name so that goes in as NULL.
		 * We can't get the connecnted status so we lie and say
		 * the device is connected.
		 */
		len = M_list_str_len(uuid_l);
		for (j=0; j<len; j++) {
			M_io_bluetooth_enum_add(btenum, c_name, c_mac, NULL, M_list_str_at(uuid_l, j), M_TRUE);
		}

cleanup_loop:
		M_io_jni_deletelocalref(env, &uuid_arr);
		M_io_jni_deletelocalref(env, &device);
		M_io_jni_deletelocalref(env, &uuid);
		M_io_jni_deletelocalref(env, &name_str);
		M_io_jni_deletelocalref(env, &mac_str);
		M_free(c_name);
		M_free(c_mac);
		M_list_str_destroy(uuid_l);
	}

done:
	M_io_jni_deletelocalref(env, &bt_adapter);
	M_io_jni_deletelocalref(env, &device_set);
	M_io_jni_deletelocalref(env, &device_arr);

	return btenum;
}


M_io_handle_t *M_io_bluetooth_open(const char *mac, const char *uuid, M_io_error_t *ioerr)
{
	JNIEnv                *env        = NULL;
	jobject                bt_adapter = NULL;
	jobject                device     = NULL;
	jboolean               rv         = 0;
	jstring                mac_str    = NULL;
	jobjectArray           uuid_arr   = NULL;
	jobject                puuid      = NULL;
	jstring                uuid_str   = NULL;
	jobject                uuid_obj   = NULL;
	jobject                socket     = NULL;
	M_io_handle_t         *handle     = NULL;

	*ioerr = M_IO_ERROR_SUCCESS;

	if (M_str_isempty(mac)) {
		*ioerr = M_IO_ERROR_INVALID;
		goto done;
	}

	env = M_io_jni_getenv();
	if (env == NULL) {
		*ioerr = M_IO_ERROR_NOSYSRESOURCES;
		goto done;
	}

	/* Get bluetooth adapter */
	if (!M_io_jni_call_jobject(&bt_adapter, NULL, 0, env, NULL, "android/bluetooth/BluetoothAdapter.getDefaultAdapter", 0) || bt_adapter == NULL) {
		*ioerr = M_IO_ERROR_PROTONOTSUPPORTED;
		goto done;
	}

	/* Make sure bluetooth adapter is enabled */
	if (!M_io_jni_call_jboolean(&rv, NULL, 0, env, bt_adapter, "android/bluetooth/BluetoothAdapter.isEnabled", 0) || !rv) {
		*ioerr = M_IO_ERROR_PROTONOTSUPPORTED;
		goto done;
	}

	/* Cancel discovery because it will make the entire connection process slowwwwww....  But ignore result in case discovery
	 * is already canceled */
	M_io_jni_call_jboolean(&rv, NULL, 0, env, bt_adapter, "android/bluetooth/BluetoothAdapter.cancelDiscovery", 0);

	/* Verify we have a real mac address. */
	mac_str = M_io_jni_pchar_to_jstring(env, mac);
	if (!M_io_jni_call_jboolean(&rv, NULL, 0, env, bt_adapter, "android/bluetooth/BluetoothAdapter.checkBluetoothAddress", 1, mac_str) || !rv) {
		*ioerr = M_IO_ERROR_INVALID;
		goto done;
	}

	/* Create a device from the adapter based on the specified mac address */
	if (!M_io_jni_call_jobject(&device, NULL, 0, env, bt_adapter, "android/bluetooth/BluetoothAdapter.getRemoteDevice", 1, mac_str) || device == NULL) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		goto done;
	}

	/* If the UUID is not specified, use the first one from the device. */
	if (M_str_isempty(uuid))
		uuid = M_IO_BLUETOOTH_RFCOMM_UUID;

	/* Convert C-String uuid into jstring */
	uuid_str = M_io_jni_pchar_to_jstring(env, uuid);
	if (uuid_str == NULL) {
		*ioerr = M_IO_ERROR_ERROR;
		goto done;
	}

	/* Convert string uuid into a UUID object */
	if (!M_io_jni_call_jobject(&uuid_obj, NULL, 0, env, NULL, "java/util/UUID.fromString", 1, uuid_str) || uuid_obj == NULL) {
		*ioerr = M_IO_ERROR_INVALID;
		goto done;
	}

	/* Get a socket from the adapter */
	if (!M_io_jni_call_jobject(&socket, NULL, 0, env, device, "android/bluetooth/BluetoothDevice.createRfcommSocketToServiceRecord", 1, uuid_obj) || socket == NULL) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		goto done;
	}


	/* All pre-validations are good here.  We're not going to start the actual connection yet as that
	 * is a blocking operation.  All of the above should have been non-blocking. */
	handle            = M_malloc_zero(sizeof(*handle));
	handle->socket    = M_io_jni_create_globalref(env, socket);
	handle->readbuf   = M_buf_create();


done:
	M_io_jni_deletelocalref(env, &bt_adapter);
	M_io_jni_deletelocalref(env, &device);
	M_io_jni_deletelocalref(env, &mac_str);
	M_io_jni_deletelocalref(env, &uuid_arr);
	M_io_jni_deletelocalref(env, &puuid);
	M_io_jni_deletelocalref(env, &uuid_str);
	M_io_jni_deletelocalref(env, &uuid_obj);
	/* We made these global references so we can remove the local refs */
	M_io_jni_deletelocalref(env, &socket);

	return handle;
}


M_bool M_io_bluetooth_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}


M_io_state_t M_io_bluetooth_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	return handle->state;
}


static void M_io_bluetooth_close(M_io_handle_t *handle, M_io_state_t state)
{
	JNIEnv *env;

	env = M_io_jni_getenv();
	if (env == NULL) {
		return;
	}

	/* Ignore any error */
	if (handle->socket && (handle->state == M_IO_STATE_CONNECTING || handle->state == M_IO_STATE_CONNECTED)) {
		handle->state = state;
		M_io_jni_call_jvoid(NULL, 0, env, handle->socket, "android/bluetooth/BluetoothSocket.close", 0);
	}
}


void M_io_bluetooth_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

	M_io_bluetooth_close(handle, M_IO_STATE_DISCONNECTED);

	/* Wait for thread to exit before we remove the global reference as there may be some
	 * delay, we don't want a crash */
	if (handle->thread) {
		void *thrv = NULL;
		M_thread_join(handle->thread, &thrv);
		handle->thread = 0;
	}

	M_buf_cancel(handle->readbuf);
	M_io_jni_delete_globalref(NULL, &handle->instream);
	M_io_jni_delete_globalref(NULL, &handle->outstream);
	M_io_jni_delete_globalref(NULL, &handle->socket);
	M_free(handle);
}


M_bool M_io_bluetooth_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	/* Do nothing, all events are generated as soft events and we don't have anything to process */
	return M_FALSE;
}


M_io_error_t M_io_bluetooth_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	JNIEnv        *env;
	jbyteArray     arr;
	M_bool         rv;

	(void)meta;

	if (layer == NULL)
		return M_IO_ERROR_INVALID;

	if (buf == NULL || *write_len == 0)
		return M_IO_ERROR_SUCCESS;

	if (handle->state != M_IO_STATE_CONNECTED || handle->socket == NULL)
		return M_IO_ERROR_INVALID;


	env = M_io_jni_getenv();
	if (env == NULL) {
		return M_IO_ERROR_NOSYSRESOURCES;
	}

	/* Copy data to write into a jbyteArray */
	arr = (*env)->NewByteArray(env, (jint)*write_len);
	(*env)->SetByteArrayRegion(env, arr, 0, (jint)*write_len, (const jbyte *)buf);

	/* Write data */
	rv = M_io_jni_call_jvoid(handle->error, sizeof(handle->error), env, handle->outstream, "java/io/OutputStream.write", 3, arr, 0, (jint)*write_len);

	/* Free jbyteArray */
	M_io_jni_deletelocalref(env, &arr);

	/* Handle error condition if any */
	if (!rv) {
		handle->state    = M_IO_STATE_ERROR;
		handle->last_err = M_IO_ERROR_ERROR;
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR);
		M_io_bluetooth_close(handle, M_IO_STATE_ERROR);
		return handle->last_err;
	}

	/* Flush output stream to ensure all bytes really got written */
	if (!M_io_jni_call_jvoid(handle->error, sizeof(handle->error), env, handle->outstream, "java/io/OutputStream.flush", 0)) {
		handle->state    = M_IO_STATE_ERROR;
		handle->last_err = M_IO_ERROR_ERROR;
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR);
		M_io_bluetooth_close(handle, M_IO_STATE_ERROR);
		return handle->last_err;
	}

	return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_bluetooth_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	(void)meta;

	if (buf == NULL || *read_len == 0)
		return M_IO_ERROR_INVALID;

	if (handle->state != M_IO_STATE_CONNECTED)
		return M_IO_ERROR_INVALID;

	if (M_buf_len(handle->readbuf) == 0)
		return M_IO_ERROR_WOULDBLOCK;

	if (*read_len > M_buf_len(handle->readbuf))
		*read_len = M_buf_len(handle->readbuf);

	M_mem_copy(buf, M_buf_peek(handle->readbuf), *read_len);
	M_buf_drop(handle->readbuf, *read_len);
	return M_IO_ERROR_SUCCESS;
}


M_bool M_io_bluetooth_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->state != M_IO_STATE_CONNECTED)
		return M_TRUE;

	M_io_bluetooth_close(handle, M_IO_STATE_DISCONNECTING);

	return M_FALSE;
}


void M_io_bluetooth_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* Only thing we can do is disable a timer if there was one */
	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

}


static void *M_io_bluetooth_thread(void *arg)
{
	M_io_handle_t *handle    = arg;
	M_io_layer_t  *layer     = NULL;
	jobject        instream  = NULL;
	jobject        outstream = NULL;
	jbyteArray     buf       = NULL;
	char           error[256];
	M_io_error_t   ioerr;
	JNIEnv        *env;

	M_mem_set(error, 0, sizeof(error));

	env = M_io_jni_getenv();
	if (env == NULL) {
		ioerr = M_IO_ERROR_NOSYSRESOURCES;
		M_snprintf(error, sizeof(error), "failed to retrieve JNIEnv");
		goto done;
	}

	/* Connect. This function will block which is why its run in its own thread.  If the caller wants to time
	 * out this process, the caller will call close() on the socket. */
	if (!M_io_jni_call_jvoid(error, sizeof(error), env, handle->socket, "android/bluetooth/BluetoothSocket.connect", 0))  {
		ioerr = M_IO_ERROR_CONNREFUSED;
		goto done;
	}

	/* Grab in and out streams */
	if (!M_io_jni_call_jobject(&instream, error, sizeof(error), env, handle->socket, "android/bluetooth/BluetoothSocket.getInputStream", 0) || instream == NULL) {
		ioerr = M_IO_ERROR_ERROR;
		goto done;
	}

	if (!M_io_jni_call_jobject(&outstream, error, sizeof(error), env, handle->socket, "android/bluetooth/BluetoothSocket.getOutputStream", 0) || outstream == NULL) {
		ioerr = M_IO_ERROR_ERROR;
		goto done;
	}

	/* Cache stream handles as global references so they can be used cross-thread and won't be garbage collected */
	handle->instream  = M_io_jni_create_globalref(env, instream);
	handle->outstream = M_io_jni_create_globalref(env, outstream);
	/* We made global reference so we can remove the localref */
	M_io_jni_deletelocalref(env, &instream);
	M_io_jni_deletelocalref(env, &outstream);


	/* Stop timer, signal connected */
	layer = M_io_layer_acquire(handle->io, 0, NULL);
	M_event_timer_remove(handle->timer);
	handle->timer = NULL;
	handle->state = M_IO_STATE_CONNECTED;
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);
	M_io_layer_release(layer);
	layer = NULL;


	/* Start read operation */
	buf = (*env)->NewByteArray(env, (jint)1024);
	while (1) {
		jint        read_len = 0;
		size_t      len;

		if (!M_io_jni_call_jint(&read_len, error, sizeof(error), env, handle->instream, "java/io/InputStream.read", 3, buf, 0, (jint)1024)) {
			ioerr = M_IO_ERROR_ERROR;
			goto done;
		}

		/* Lock layer, copy bytes read into readbuf, release */
		if (read_len > 0) {
			layer = M_io_layer_acquire(handle->io, 0, NULL);

			/* If no data was in readbuf, raise READ signal */
			if (M_buf_len(handle->readbuf) == 0) {
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
			}

			len = (size_t)read_len;
			(*env)->GetByteArrayRegion(env, buf, 0, read_len, (jbyte *)M_buf_direct_write_start(handle->readbuf, &len));
			M_buf_direct_write_end(handle->readbuf, (size_t)read_len);

			M_io_layer_release(layer);
			layer = NULL;
		}
	}

done:
	/* On error, these might not have been removed */
	M_io_jni_deletelocalref(env, &instream);
	M_io_jni_deletelocalref(env, &outstream);

	M_io_jni_deletelocalref(env, &buf);

	/* Don't attempt to lock the layer if it isn't in one of the listed states as we could deadlock on
	 * a destroy */
	if (handle->state == M_IO_STATE_DISCONNECTING || handle->state == M_IO_STATE_CONNECTED || handle->state == M_IO_STATE_CONNECTING) {
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		if (handle->state == M_IO_STATE_DISCONNECTING) {
			handle->state    = M_IO_STATE_DISCONNECTED;
			handle->last_err = M_IO_ERROR_DISCONNECT;
			M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_DISCONNECTED);
		} else if (handle->state == M_IO_STATE_CONNECTED || handle->state == M_IO_STATE_CONNECTING) {
			if (handle->state == M_IO_STATE_CONNECTING && handle->is_timeout) {
				handle->last_err = M_IO_ERROR_TIMEDOUT;
				M_snprintf(handle->error, sizeof(handle->error), "Timeout trying to connect");
			} else {
				handle->last_err = ioerr;
				M_snprintf(handle->error, sizeof(handle->error), "%s", error);
			}
			handle->state    = M_IO_STATE_ERROR;
			M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR);
		} else {
			/* Any other state we should not reset the error handles as we don't want to overwrite */
		}

		M_io_layer_release(layer);
	}

	return NULL;
}


static void M_io_bluetooth_timer_cb(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer;
	(void)dummy_io;
	(void)type;
	(void)event;

	/* Lock! */
	layer = M_io_layer_acquire(handle->io, 0, NULL);

	handle->timer = NULL;

	/* Sanity check ... don't think this could ever happen */
	if (handle->state != M_IO_STATE_CONNECTING)
		return;

	/* Record that this is a connection timeout condition */
	handle->is_timeout = M_TRUE;

	/* Tell the thread to shutdown by closing the socket on our end */
	M_io_bluetooth_close(handle, M_IO_STATE_ERROR);

	M_io_layer_release(layer);
}


M_bool M_io_bluetooth_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t   *handle = M_io_layer_get_handle(layer);
	M_io_t          *io     = M_io_layer_get_io(layer);
	M_event_t       *event  = M_io_get_event(io);
	M_thread_attr_t *attr;

	switch (handle->state) {
		case M_IO_STATE_INIT:
			handle->state = M_IO_STATE_CONNECTING;
			handle->io    = io;

			/* Spawn helper thread */
			attr = M_thread_attr_create();
			M_thread_attr_set_create_joinable(attr, M_TRUE);
			handle->thread = M_thread_create(attr, M_io_bluetooth_thread, handle);
			M_thread_attr_destroy(attr);

			/* Fall-thru */
		case M_IO_STATE_CONNECTING:
			/* start timer to time out operation */
			handle->timer = M_event_timer_oneshot(event, 10000, M_TRUE, M_io_bluetooth_timer_cb, handle);
			break;
		case M_IO_STATE_CONNECTED:
			/* Trigger connected soft event when registered with event handle */
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);

			/* If there is data in the read buffer, signal there is data to be read as well */
			if (M_buf_len(handle->readbuf)) {
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
			}
			break;
		default:
			/* Any other state is an error */
			return M_FALSE;
	}

	return M_TRUE;
}

