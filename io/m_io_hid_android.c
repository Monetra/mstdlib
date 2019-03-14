/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_hid.h>
#include <mstdlib/io/m_io_jni.h>
#include "m_io_hid_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef enum {
	M_IO_HID_STATUS_SYSUP    = 1 << 0,  /*!< System is online        */
	M_IO_HID_STATUS_WRITERUP = 1 << 1,  /*!< Writer thread is online */
	M_IO_HID_STATUS_READERUP = 1 << 2   /*!< Reader thread is online */
} M_io_hid_status_t;


struct M_io_handle {
	jobject           connection;  /*!< UsbDeviceConnection */
	jobject           interface;   /*!< UsbInterface */
	jobject           ep_in;       /*!< UsbEndpoint In (read) */
	jobject           ep_out;      /*!< UsbEndpoint Out (write) */
	M_io_t           *io;
	M_buf_t          *readbuf;     /*!< Reads are transferred via a buffer. */
	M_buf_t          *writebuf;    /*!< Write are transferred via a buffer. */
	M_thread_mutex_t *read_lock;   /*!< Lock when manipulating read buffer. */
	M_thread_mutex_t *write_lock;  /*!< Lock when manipulating write buffer. */
	M_thread_cond_t  *write_cond;  /*!< Conditional to wake write thread when there is data to write. */

	M_io_hid_status_t status;      /*!< Bitmap of current status */
	M_bool            in_destroy;  /*!< Are we currently destroying the device. Prevents Disocnnected signal from being sent. */
	M_threadid_t      read_tid;    /*!< Thread id for the read thread. */
	M_threadid_t      write_tid;   /*!< Thread id for the write thread. */
	char              error[256];  /*!< Error buffer for description of last system error. */

	char             *path;
	char             *manufacturer;
	char             *product;
	char             *serial;
	M_uint16          productid;
	M_uint16          vendorid;

	M_bool            uses_reportid;
	size_t            max_input_report_size;
	size_t            max_output_report_size;

	M_event_timer_t  *disconnect_timer;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static jobject get_usb_manager(JNIEnv *env)
{
	jobject app_context = NULL;
	jstring sname       = NULL;
	jobject manager     = NULL;

	/* Get the application context. */
	app_context = M_io_jni_get_android_app_context();
	if (app_context == NULL)
		goto done;

	/* Get the Usb Service name. */
	if (!M_io_jni_call_jobjectField(&sname, NULL, 0, env, NULL, "android/content/Context.USB_SERVICE"))
		goto done;

	/* Get the UsbManager from the system services. */
	if (!M_io_jni_call_jobject(&manager, NULL, 0, env, app_context, "android/content/Context.getSystemService", 1, sname))
		goto done;

done:
	M_io_jni_deletelocalref(env, &sname);
	return manager;
}

static M_bool M_io_hid_dev_info(JNIEnv *env, jobject device,
		char **path, char **manuf, char **product, char **serial,
		M_uint16 *vendorid, M_uint16 *productid)
{
	jstring sval = NULL;
	jint    id;

	if (device == NULL)
		return M_FALSE;

	if (path != NULL)
		*path = NULL;
	if (manuf != NULL)
		*manuf = NULL;
	if (product != NULL)
		*product = NULL;
	if (serial != NULL)
		*serial = NULL;

	if (path != NULL) {
		/* Device name is really the path. The list of Usb Devices the manager
 		 * will give us is referenced by this value. */
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getDeviceName", 0)) {
			goto err;
		}
		*path = M_io_jni_jstring_to_pchar(env, sval);
	}
	M_io_jni_deletelocalref(env, &sval);

	if (manuf != NULL) {
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getManufacturerName", 0)) {
			goto err;
		}
		*manuf = M_io_jni_jstring_to_pchar(env, sval);
	}
	M_io_jni_deletelocalref(env, &sval);

	if (product != NULL) {
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getProductName", 0)) {
			goto err;
		}
		*product = M_io_jni_jstring_to_pchar(env, sval);
	}
	M_io_jni_deletelocalref(env, &sval);

	if (serial != NULL) {
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getSerialNumber", 0)) {
			goto err;
		}
		*serial = M_io_jni_jstring_to_pchar(env, sval);
	}
	M_io_jni_deletelocalref(env, &sval);

	if (vendorid != NULL) {
		if (!M_io_jni_call_jint(&id, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getVendorId", 0) || id == -1) {
			goto err;
		}
		*vendorid = (M_uint16)id;
	}

	if (productid != NULL) {
		if (!M_io_jni_call_jint(&id, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getProductId", 0) || id == -1) {
			goto err;
		}
		*productid = (M_uint16)id;
	}

	return M_TRUE;

err:
	M_io_jni_deletelocalref(env, &sval);

	if (path != NULL) {
		M_free(*path);
		*path = NULL;
	}
	if (manuf != NULL) {
		M_free(*manuf);
		*manuf = NULL;
	}
	if (product != NULL) {
		M_free(*product);
		*product = NULL;
	}
	if (serial != NULL) {
		M_free(*serial);
		*serial = NULL;
	}

	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_hid_enum_t *M_io_hid_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial)
{
	M_io_hid_enum_t *hidenum       = M_io_hid_enum_init();
	JNIEnv          *env           = NULL;
	jobject          manager       = NULL;
	jobject          dev_list      = NULL;
	jobject          set_o         = NULL;
	jobjectArray     str_array_o   = NULL;
	jint             hid_class     = -1;
	jint             per_inf_class = -1;
	size_t           size;
	size_t           i;

	env = M_io_jni_getenv();
	if (env == NULL)
		goto done;

	/* Get the UsbManager. */
	manager = get_usb_manager(env);
	if (manager == NULL)
		goto done;

	/* Get the Usb HID class value. */
	if (!M_io_jni_call_jintField(&hid_class, NULL, 0, env, NULL, "android/hardware/usb/UsbConstants.USB_CLASS_HID") || hid_class == -1)
		goto done;

	/* Get the Usb per interface class value. */
	if (!M_io_jni_call_jintField(&per_inf_class, NULL, 0, env, NULL, "android/hardware/usb/UsbConstants.USB_CLASS_PER_INTERFACE") || per_inf_class == -1)
		goto done;

	/* Get the usb device list. */
	if (!M_io_jni_call_jobject(&dev_list, NULL, 0, env, manager, "android/hardware/usb/UsbManager.getDeviceList", 0))
		goto done;

	/* Turn the keys into a set of keys. */
	if (!M_io_jni_call_jobject(&set_o, NULL, 0, env, dev_list, "java/util/HashMap.keySet", 0))
		goto done;

	/* Turn the set of keys into an array of keys. */
	if (!M_io_jni_call_jobjectArray(&str_array_o, NULL, 0, env, set_o, "java/util/Set.toArray", 0))
		goto done;

	size = M_io_jni_array_length(env, str_array_o);
	if (size == 0)
		goto done;

	/* Iterate though the array of keys and pull out each UsbDevice. */
	for (i=0; i<size; i++) {
		jstring   key_o     = NULL;
		jobject   dev       = NULL;
		jint      dev_class = -1;
		char     *path      = NULL;
		char     *manuf     = NULL;
		char     *product   = NULL;
		char     *serialnum = NULL;
		M_uint16  pid       = 0;
		M_uint16  vid       = 0;

		/* Get a key from the list of keys. */
		key_o = M_io_jni_array_element(env, str_array_o, i);
		if (key_o == NULL)
			goto loop_cleanup;

		/* Get the UsbDevice for the given key. */
		if (!M_io_jni_call_jobject(&dev, NULL, 0, env, dev_list, "java/util/HashMap.get", 1, key_o))
			goto loop_cleanup;

		/* Get the device class. */
		if (!M_io_jni_call_jint(&dev_class, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getDeviceClass", 0) || dev_class == -1)
			goto loop_cleanup;

		/* Check this could be a hid device. */
		if (dev_class != hid_class && dev_class != per_inf_class)
			goto loop_cleanup;

		/* If the device class is determined per interface we need to
 		 * inspect each interface. I there is a hid type interface then
		 * it's a hid device and we can use it. */
		if (dev_class == per_inf_class) {
 			jint   cnt      = 0;
			M_bool have_hid = M_FALSE;
			jint   j;

			if (!M_io_jni_call_jint(&cnt, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getInterfaceCount", 0) || cnt <= 0)
				goto loop_cleanup;

			for (j=0; j<cnt; j++) {
				jobject dev_inf = NULL;

				if (!M_io_jni_call_jobject(&dev_inf, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getInterface", 1, j)) {
					continue;
				}

				if (!M_io_jni_call_jint(&dev_class, NULL, 0, env, dev_inf, "android/hardware/usb/UsbInterface.getInterfaceClass", 0)) {
					M_io_jni_deletelocalref(env, &dev_inf);
					continue;
				}

				M_io_jni_deletelocalref(env, &dev_inf);
				if (dev_class == hid_class) {
					have_hid = M_TRUE;
					break;
				}
			}

			if (!have_hid) {
				goto loop_cleanup;
			}
		}

		if (!M_io_hid_dev_info(env, dev, &path, &manuf, &product, &serialnum, &vid, &pid))
			goto loop_cleanup;

		M_io_hid_enum_add(hidenum,
				/* Info about this enumerated device */
				path, manuf, product, serialnum, vid, pid,
				/* Search/Match criteria */
				vendorid, productids, num_productids, serial);

loop_cleanup:
		M_io_jni_deletelocalref(env, &dev);
		M_io_jni_deletelocalref(env, &key_o);
		M_free(path);
		M_free(manuf);
		M_free(product);
		M_free(serialnum);
	}

done:
	M_io_jni_deletelocalref(env, &set_o);
	M_io_jni_deletelocalref(env, &str_array_o);
	M_io_jni_deletelocalref(env, &dev_list);
	M_io_jni_deletelocalref(env, &manager);

	return hidenum;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_io_hid_get_manufacturer(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL)
		ret = M_strdup(handle->manufacturer);

	M_io_layer_release(layer);
	return ret;
}

char *M_io_hid_get_path(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL)
		ret = M_strdup(handle->path);

	M_io_layer_release(layer);
	return ret;
}

char *M_io_hid_get_product(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL)
		ret = M_strdup(handle->product);

	M_io_layer_release(layer);
	return ret;
}

M_uint16 M_io_hid_get_productid(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint16       ret    = 0;

	if (handle != NULL)
		ret = handle->productid;

	M_io_layer_release(layer);
	return ret;
}

M_uint16 M_io_hid_get_vendorid(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint16       ret    = 0;

	if (handle != NULL)
		ret = handle->vendorid;

	M_io_layer_release(layer);
	return ret;
}

char *M_io_hid_get_serial(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL)
		ret = M_strdup(handle->serial);

	M_io_layer_release(layer);
	return ret;
}

void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	size_t         my_in;
	size_t         my_out;

	if (max_input_size == NULL)
		max_input_size  = &my_in;

	if (max_output_size == NULL)
		max_output_size = &my_out;

	layer  = M_io_hid_get_top_hid_layer(io);
	handle = M_io_layer_get_handle(layer);

	if (handle == NULL) {
		*max_input_size  = 0;
		*max_output_size = 0;
	} else {
		*max_input_size  = handle->max_input_report_size;
		*max_output_size = handle->max_output_report_size;
	}

	M_io_layer_release(layer);
}

/* Expects io to be locked! */
static void M_io_hid_close_connection(M_io_handle_t *handle)
{
	JNIEnv  *env    = M_io_jni_getenv();
	jboolean rv;

	if (env == NULL || handle == NULL || handle->connection == NULL)
		return;

	/* Release Interface. */
	M_io_jni_call_jboolean(&rv, NULL, 0, env, handle->connection, "android/hardware/usb/UsbDeviceConnection.releaseInterface", 1, handle->interface);

	/* Close connection. If read is blocking waiting for data this will cause the read to
	 * return so the thread will stop. */
	M_io_jni_call_jvoid(NULL, 0, env, handle->connection, "android/hardware/usb/UsbDeviceConnection.close", 0);

	/* Destroy the connection. Sets the var to NULL.
	 * If there is a read blocking it will return there was an error
	 * but since we have already set run = false the read loop will
	 * ignore the error and stop running. */
	M_io_jni_delete_globalref(env, &handle->interface);
	M_io_jni_delete_globalref(env, &handle->connection);
}

/* Expects the io layer to be locked already! */
static void M_io_hid_signal_shutdown(M_io_handle_t *handle)
{
	if (!(handle->status & M_IO_HID_STATUS_SYSUP))
		return;

	/* Tell our threads they can stop running. */
	handle->status &= ~(M_IO_HID_STATUS_SYSUP);

	if (handle->status & M_IO_HID_STATUS_WRITERUP) {
		/* And wake up the writer thread. */
		M_thread_mutex_lock(handle->write_lock);
		M_thread_cond_signal(handle->write_cond);
		M_thread_mutex_unlock(handle->write_lock);
	}
}

/* Layer is expected to be locked on entry */
static void M_io_hid_handle_rw_error(M_io_handle_t *handle, M_io_layer_t *layer)
{
	/* Treat a failure as a disconnect. The return of bulkTransfer is always
	 * -1 so we don't know what the error was from. It could have been an
	 * unexpected disconnect or something else. We're always going to close
	 * the device since it's in an unusable state, so it's being disconnected
	 * regardless if that's the reason for the read error. */

	/* We have read and write threads going, if we're in the middle of a write
	 * both read and write bulkTransfer operations will error. */
	M_io_hid_signal_shutdown(handle);
	M_io_hid_close_connection(handle);

	if (!(handle->status & (M_IO_HID_STATUS_READERUP|M_IO_HID_STATUS_WRITERUP))) {
		/* Kill any pending disconnect timer and issue a disconnected signal */
		if (handle->disconnect_timer)
			M_event_timer_remove(handle->disconnect_timer);
		handle->disconnect_timer = NULL;

		if (!handle->in_destroy)
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
	}
}


static void *M_io_hid_read_loop(void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer;
	JNIEnv        *env;
	jbyteArray     data;
	jint           rv;
	size_t         max_len;
	M_bool         is_error = M_FALSE;

	env = M_io_jni_getenv();
	if (env == NULL) {
		M_snprintf(handle->error, sizeof(handle->error), "Failed to start read thread");
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		M_io_layer_release(layer);
		return NULL;
	}

	/* Determine the max len of a read based on if report ids are
	 * used or not. If they're not being used we need to decrease
	 * since the first byte won't be the additional report id. */
	max_len = handle->max_input_report_size;
	if (!handle->uses_reportid)
		max_len--;

	/* Create an array to store the read data. */
	data = (*env)->NewByteArray(env, (jsize)max_len);

	while (handle->status & M_IO_HID_STATUS_SYSUP) {
		/* Wait for data to be read. We have a 0 timeout and will
 		 * exit on error, or if the connection is closed by us. */
		if (!M_io_jni_call_jint(&rv, handle->error, sizeof(handle->error), env, handle->connection, "android/hardware/usb/UsbDeviceConnection.bulkTransfer", 4, handle->ep_in, data, (jint)max_len, 0) || rv < 0) {
			is_error = M_TRUE;
			break;
		}

		/* No data read so nothing to process right now. */
		if (rv == 0) {
			continue;
		}

		/* Fill the read buffer with the data that was read. */
		M_thread_mutex_lock(handle->read_lock);

		/* Copy data read into readbuf */
		M_io_jni_jbyteArray_to_buf(env, data, (size_t)rv /* length */, handle->readbuf);

		/* Zero the read data since the data object is long lived and
		 * it could contain sensitive data. */
		M_io_jni_jbyteArray_zeroize(env, data);

		M_thread_mutex_unlock(handle->read_lock);

		/* Let the caller know there is data to read. */
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
		M_io_layer_release(layer);
	}

	/* Final zeroing of data in case we existed the loop early
	 * and before it was zeroed. */
	M_io_jni_jbyteArray_zeroize(env, data);

	M_io_jni_deletelocalref(env, &data);

	layer = M_io_layer_acquire(handle->io, 0, NULL);
	handle->status &= ~(M_IO_HID_STATUS_READERUP);
	M_io_hid_handle_rw_error(handle, layer);
	M_io_layer_release(layer);

	return NULL;
}


static void *M_io_hid_write_loop(void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer;
	size_t         len;
	size_t         max_len;
	JNIEnv        *env;
	jbyteArray     data;
	jint           rv;
	M_bool         more_data = M_FALSE;
	M_bool         is_error  = M_FALSE;

	env = M_io_jni_getenv();
	if (env == NULL) {
		M_snprintf(handle->error, sizeof(handle->error), "Failed to start write thread");
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		M_io_layer_release(layer);
		return NULL;
	}

	/* Determine the max len of a write based on if report ids are
	 * used or not. If they're not being used we need to decrease
	 * since the first byte won't be the additional report id. */
	max_len = handle->max_output_report_size;
	if (!handle->uses_reportid)
		max_len--;

	/* Create a buffer to put our write data into because we need it
	 * in a Java compatible buffer. */
	data = (*env)->NewByteArray(env, (jsize)max_len);

	while (handle->status & M_IO_HID_STATUS_SYSUP) {
		M_thread_mutex_lock(handle->write_lock);

		/* Wait for data. */
		if (M_buf_len(handle->writebuf) == 0) {
			M_thread_cond_wait(handle->write_cond, handle->write_lock);
		}

		/* We might have received both a signal to write and a signal to disconnect
		 * nearly simultaneously.  Lets go ahead and allow the write
		 * if (!handle->run) {
		 * 	M_thread_mutex_unlock(handle->write_lock);
		 * 	break;
		 * }
		 */

		/* If there isn't anything to write we have nothing to
		 * do right now. */
		if (M_buf_len(handle->writebuf) == 0) {
			M_thread_mutex_unlock(handle->write_lock);
			continue;
		}

		/* Move the buffered write data to the JNI array we'll send. */
		len = M_MIN(M_buf_len(handle->writebuf), max_len);
		(*env)->SetByteArrayRegion(env, data, 0, (jsize)len, (const jbyte *)M_buf_peek(handle->writebuf));

		if (!M_io_jni_call_jint(&rv, handle->error, sizeof(handle->error), env, handle->connection, "android/hardware/usb/UsbDeviceConnection.bulkTransfer", 4, handle->ep_out, data, (jint)len, 0) || rv <= 0) {
			M_thread_mutex_unlock(handle->write_lock);
			is_error = M_TRUE;
			break;
		}

		/* Zero the read data since the data object is long lived and
		 * it could contain sensitive data. */
		M_io_jni_jbyteArray_zeroize(env, data);

		/* Drop the data that was sent from the write buffer. */
		M_buf_drop(handle->writebuf, (size_t)rv);

		/* Use this flag to know if we have more data, and if
 		 * we should send a write event to inform that writing
		 * can happen again. Since this is the only way data can
		 * be removed from the buffer we know this logic can't
		 * change outside of the lock.
		 *
		 * We need to use a flag because we can't be in a lock
		 * at the same time we're in a layer lock. The write_cb
		 * will be in a layer lock and also uses the write_lock.
		 * We don't want to get into a double lock order thread
		 * dead lock. */
		if (M_buf_len(handle->writebuf) > 0) {
			more_data = M_TRUE;
		}
		M_thread_mutex_unlock(handle->write_lock);

		/* We can write again. */
		if (!more_data && handle->status & M_IO_HID_STATUS_SYSUP) {
			layer = M_io_layer_acquire(handle->io, 0, NULL);
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE);
			M_io_layer_release(layer);
		}
		more_data = M_FALSE;
	}

	/* Final zeroing of data in case we existed the loop early
	 * and before it was zeroed. */
	M_io_jni_jbyteArray_zeroize(env, data);

	M_io_jni_deletelocalref(env, &data);

	layer = M_io_layer_acquire(handle->io, 0, NULL);
	handle->status &= ~(M_IO_HID_STATUS_WRITERUP);
	M_io_hid_handle_rw_error(handle, layer);
	M_io_layer_release(layer);

	return NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr)
{
	JNIEnv          *env                     = NULL;
	M_io_handle_t   *handle                  = NULL;
	jobject          manager                 = NULL;
	jobject          dev_list                = NULL;
	jobject          device                  = NULL;
	jobject          ep_in                   = NULL;
	jobject          ep_out                  = NULL;
	jobject          connection              = NULL;
	jobject          interface               = NULL;
	jbyteArray       descrs                  = NULL;
	jstring          sval                    = NULL;
	jint             hid_class               = -1;
	jint             dev_class               = -1;
	jint             dir_in                  = -1;
	jint             dir_out                 = -1;
	unsigned char   *body;
	char            *path                    = NULL;
	char            *manufacturer            = NULL;
	char            *product                 = NULL;
	char            *serial                  = NULL;
	M_uint16         productid               = 0;
	M_uint16         vendorid                = 0;
	size_t           max_input_report_size   = 0;
	size_t           max_output_report_size  = 0;
	size_t           usize                   = 0;
	M_bool           uses_reportid           = M_FALSE;
	M_bool           opened                  = M_FALSE;
	jboolean         rb;
	jint             size                    = 0;
	jint             cnt                     = 0;
	jint             i;

	*ioerr = M_IO_ERROR_SUCCESS;

	if (M_str_isempty(devpath)) {
		*ioerr = M_IO_ERROR_INVALID;
		goto err;
	}

	env = M_io_jni_getenv();
	if (env == NULL) {
		*ioerr = M_IO_ERROR_NOSYSRESOURCES;
		goto err;
	}

	/* Get the UsbManager. */
	manager = get_usb_manager(env);
	if (manager == NULL) {
		*ioerr = M_IO_ERROR_NOSYSRESOURCES;
		goto err;
	}

	/* Get the Usb HID class value. */
	if (!M_io_jni_call_jintField(&hid_class, NULL, 0, env, NULL, "android/hardware/usb/UsbConstants.USB_CLASS_HID") || hid_class == -1) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	/* Get the Usb endpoint direction constants. */
	if (!M_io_jni_call_jintField(&dir_in, handle->error, sizeof(handle->error), env, NULL, "android/hardware/usb/UsbConstants.USB_DIR_IN") || dir_in == -1)
		goto err;

	if (!M_io_jni_call_jintField(&dir_out, handle->error, sizeof(handle->error), env, NULL, "android/hardware/usb/UsbConstants.USB_DIR_OUT") || dir_out == -1)
		goto err;

	/* Get the usb device list. */
	if (!M_io_jni_call_jobject(&dev_list, NULL, 0, env, manager, "android/hardware/usb/UsbManager.getDeviceList", 0) || dev_list == NULL)
		goto err;

	/* Pull out the device we want to operate on. */
	sval = M_io_jni_pchar_to_jstring(env, devpath);
	if (!M_io_jni_call_jobject(&device, NULL, 0, env, dev_list, "java/util/HashMap.get", 1, sval) || device == NULL) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		goto err;
	}
	M_io_jni_deletelocalref(env, &sval);

	/* Find the HID interface. */
	if (!M_io_jni_call_jint(&cnt, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getInterfaceCount", 0) || cnt <= 0)
		goto err;

	for (i=0; i<cnt; i++) {
		jobject dev_inf = NULL;

		if (!M_io_jni_call_jobject(&dev_inf, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getInterface", 1, i)) {
			continue;
		}

		if (!M_io_jni_call_jint(&dev_class, NULL, 0, env, dev_inf, "android/hardware/usb/UsbInterface.getInterfaceClass", 0)) {
			M_io_jni_deletelocalref(env, &dev_inf);
			continue;
		}

		if (dev_class == hid_class) {
			interface = dev_inf;
			break;
		}
		M_io_jni_deletelocalref(env, &dev_inf);
	}

	if (interface == NULL) {
		*ioerr = M_IO_ERROR_PROTONOTSUPPORTED;
		goto err;
	}

	/* Find the in and out endpoints. */
	if (!M_io_jni_call_jint(&cnt, NULL, 0, env, interface, "android/hardware/usb/UsbInterface.getEndpointCount", 0) || cnt <= 0)
		goto err;

	for (i=0; i<cnt; i++) {
		jobject endpoint  = NULL;
		jint    direction = -1;

		if (!M_io_jni_call_jobject(&endpoint, NULL, 0, env, interface, "android/hardware/usb/UsbInterface.getEndpoint", 1, i)) {
			continue;
		}

		if (!M_io_jni_call_jint(&direction, NULL, 0, env, endpoint, "android/hardware/usb/UsbEndpoint.getDirection", 0) || direction < 0) {
			continue;
		}

		if (ep_in == NULL && direction == dir_in) {
			ep_in = endpoint;
		} else if (ep_out == NULL && direction == dir_out) {
			ep_out = endpoint;
		} else {
			M_io_jni_deletelocalref(env, &endpoint);
		}

		if (ep_in != NULL && ep_out != NULL) {
			break;
		}
	}

	/* We require in and out endpoints to be supported. */
	if (ep_in == NULL || ep_out == NULL) {
		*ioerr = M_IO_ERROR_PROTONOTSUPPORTED;
		goto err;
	}

	/* Open the device connection. */
	if (!M_io_jni_call_jobject(&connection, NULL, 0, env, manager, "android/hardware/usb/UsbManager.openDevice", 1, device) || connection == NULL) {
		*ioerr = M_IO_ERROR_CONNREFUSED;
		goto err;
	}
	opened = M_TRUE;

	/* Claim the interface. */
	if (!M_io_jni_call_jboolean(&rb, NULL, 0, env, connection, "android/hardware/usb/UsbDeviceConnection.claimInterface", 2, interface, M_TRUE) || !rb) {
		*ioerr = M_IO_ERROR_CONNREFUSED;
		goto err;
	}

	/* Determine if report ids are used and get the report sizes. While there
	 * is an API function UsbEndpoint.getMaxPacketSize the HID descriptors will
	 * have this info so that's fewer JNI calls. Also, we're not using
	 * UsbDeviceConnection.getRawDescriptors because it returns USB descriptors
	 * not HID descriptors. Finally, there is no API function to get if reports
	 * ids are in use are not. So we need to get the HID descriptors
	 * regardless. */
	descrs = (*env)->NewByteArray(env, 4096); /* 4096 is maximum descriptor size. */
	if (!M_io_jni_call_jint(&size, NULL, 0, env, connection, "android/hardware/usb/UsbDeviceConnection.controlTransfer", 7, 0x81, 0x06, 0x2200, 0x00, descrs, 4096, 2000)) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	body = M_io_jni_jbyteArray_to_puchar(env, descrs, (size_t)size, &usize);
	uses_reportid = hid_uses_report_descriptors((const unsigned char *)body, usize);
	hid_get_max_report_sizes((const unsigned char *)body, usize, &max_input_report_size, &max_output_report_size);
	M_free(body);

	/* Note: We need to include report ID byte in reported size. So, increment both by one. */
	if (max_input_report_size > 0)
		max_input_report_size++;
	if (max_output_report_size > 0)
		max_output_report_size++;

	/* Get the device metadata. */
	if (!M_io_hid_dev_info(env, device, &path, &manufacturer, &product, &serial, &vendorid, &productid))
		goto err;

	/* Setup the handle. */
	handle                         = M_malloc_zero(sizeof(*handle));
	handle->readbuf                = M_buf_create();
	handle->writebuf               = M_buf_create();
	handle->read_lock              = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	handle->write_lock             = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	handle->write_cond             = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	handle->status                 = M_IO_HID_STATUS_SYSUP;
	handle->path                   = path;
	handle->manufacturer           = manufacturer;
	handle->product                = product;
	handle->serial                 = serial;
	handle->productid              = productid;
	handle->vendorid               = vendorid;
	handle->uses_reportid          = uses_reportid;
	handle->max_input_report_size  = max_input_report_size;
	handle->max_output_report_size = max_output_report_size;

	/* Store the Java objects. */
	handle->connection = M_io_jni_create_globalref(env, connection);
	handle->interface  = M_io_jni_create_globalref(env, interface);
	handle->ep_in      = M_io_jni_create_globalref(env, ep_in);
	handle->ep_out     = M_io_jni_create_globalref(env, ep_out);

	M_io_jni_deletelocalref(env, &descrs);
	M_io_jni_deletelocalref(env, &ep_in);
	M_io_jni_deletelocalref(env, &ep_out);
	M_io_jni_deletelocalref(env, &device);
	M_io_jni_deletelocalref(env, &dev_list);
	M_io_jni_deletelocalref(env, &manager);

	return handle;

err:
	/* Try to close the device if it was opened. */
	if (opened) {
		M_io_jni_call_jboolean(&rb, NULL, 0, env, connection, "android/hardware/usb/UsbDeviceConnection.releaseInterface", 1, interface);
		M_io_jni_call_jvoid(NULL, 0, env, connection, "android/hardware/usb/UsbDeviceConnection.close", 0);
	}

	M_io_jni_deletelocalref(env, &sval);
	M_io_jni_deletelocalref(env, &interface);
	M_io_jni_deletelocalref(env, &connection);
	M_io_jni_deletelocalref(env, &device);
	M_io_jni_deletelocalref(env, &dev_list);
	M_io_jni_deletelocalref(env, &ep_in);
	M_io_jni_deletelocalref(env, &ep_out);
	M_io_jni_deletelocalref(env, &descrs);
	M_io_jni_deletelocalref(env, &manager);

	M_free(path);
	M_free(manufacturer);
	M_free(product);
	M_free(serial);

	if (*ioerr == M_IO_ERROR_SUCCESS)
		*ioerr = M_IO_ERROR_ERROR;

	return NULL;
}

M_bool M_io_hid_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}

M_io_state_t M_io_hid_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle->connection == NULL)
		return M_IO_STATE_ERROR;
	return M_IO_STATE_CONNECTED;
}


void M_io_hid_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	JNIEnv        *env    = M_io_jni_getenv();

	if (handle == NULL)
		return;

	if (handle->disconnect_timer) {
		M_event_timer_remove(handle->disconnect_timer);
	}

	handle->disconnect_timer = NULL;

	/* Though we might like to delay cleanup, we can't as we may not have an
	 * event loop at all once this function is called.  Integrators really
	 * need to rely on disconnect instead! */

	handle->in_destroy = M_TRUE;

	M_io_hid_signal_shutdown(handle);
	M_io_hid_close_connection(handle);

	/* Wait for the processing threads to exit. */
	if (handle->write_tid)
		M_thread_join(handle->write_tid, NULL);

	if (handle->read_tid)
		M_thread_join(handle->read_tid, NULL);

	/* Clear everything. */
	M_io_jni_delete_globalref(env, &handle->ep_in);
	M_io_jni_delete_globalref(env, &handle->ep_out);

	M_thread_mutex_destroy(handle->read_lock);
	M_thread_mutex_destroy(handle->write_lock);
	M_thread_cond_destroy(handle->write_cond);
	M_buf_cancel(handle->readbuf);
	M_buf_cancel(handle->writebuf);

	M_free(handle->path);
	M_free(handle->manufacturer);
	M_free(handle->product);
	M_free(handle->serial);

	M_free(handle);
}


M_bool M_io_hid_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	/* Do nothing, all events are generated as soft events. */
	return M_FALSE;
}

M_io_error_t M_io_hid_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         len;

	(void)meta;

	if (handle->connection == NULL || !(handle->status & M_IO_HID_STATUS_SYSUP))
		return M_IO_ERROR_NOTCONNECTED;

	if (buf == NULL || *write_len == 0)
		return M_IO_ERROR_SUCCESS;

	M_thread_mutex_lock(handle->write_lock);

	if (M_buf_len(handle->writebuf) > 0) {
		M_thread_mutex_unlock(handle->write_lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	/* Don't send the report id in the data if we're not using report ids. */
	len = *write_len;
	if (!handle->uses_reportid) {
		buf++;
		len--;
	}
	if (len == 0) {
		M_thread_mutex_unlock(handle->write_lock);
		return M_IO_ERROR_SUCCESS;
	}

	M_buf_add_bytes(handle->writebuf, buf, len);

	M_thread_cond_signal(handle->write_cond);
	M_thread_mutex_unlock(handle->write_lock);
	return M_IO_ERROR_SUCCESS;
}

M_io_error_t M_io_hid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         offset = 0;
	size_t         len;

	(void)meta;

	if (buf == NULL || *read_len == 0)
		return M_IO_ERROR_INVALID;

	if (handle->connection == NULL || !(handle->status & M_IO_HID_STATUS_SYSUP))
		return M_IO_ERROR_NOTCONNECTED;

	M_thread_mutex_lock(handle->read_lock);

	if (M_buf_len(handle->readbuf) == 0) {
		M_thread_mutex_unlock(handle->read_lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	len = M_buf_len(handle->readbuf);
	/* Don't try to read more than we can. */
	len = M_MIN(len, *read_len);

	if (!handle->uses_reportid) {
		/* If we don't use report ids, we must prefix the read buffer with a zero. */
		buf[0] = 0;
		offset = 1;
		/* If we're maxed on the buffer we need to make room for the offset amount. */
		if (*read_len == len) {
			len -= offset;
		}
	}

	/* Copy from the read buffer into the output buffer. */
	M_mem_copy(buf+offset, M_buf_peek(handle->readbuf), len);
	/* Drop what we read. */
	M_buf_drop(handle->readbuf, len);
	/* Our read total is how much we read from the readbuf plus how much we pre-filled. */
	*read_len = len+offset;

	M_thread_mutex_unlock(handle->read_lock);
	return M_IO_ERROR_SUCCESS;
}


/* Now its time to issue a disconnect event for final cleanup if one hasn't already been sent. */
static void M_io_hid_disconnect_runner_step2(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer  = M_io_layer_acquire(handle->io, 0, NULL);

	(void)event;
	(void)type;
	(void)dummy_io;

	M_event_timer_remove(handle->disconnect_timer);

	/* Send disconnect event */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
	M_io_layer_release(layer);
}


/* We have now waited for any writes to finish and exit.  Time to get the read thread to quit. */
static void M_io_hid_disconnect_runner_step1(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_handle_t *handle    = arg;
	M_io_layer_t  *layer     = M_io_layer_acquire(handle->io, 0, NULL);

	(void)event;
	(void)type;
	(void)dummy_io;

	M_event_timer_remove(handle->disconnect_timer);
	handle->disconnect_timer = NULL;

	/* Most likely the writer has exited, but we don't actually need to wait on it.  We'll just go onto
	 * the next step in case the writer is locked in a write.
	 *
	 * M_thread_join(handle->write_tid, NULL);
	 * handle->write_tid = NULL;
	 */

	/* Close connection. If read is blocking waiting for data this will cause the read to
	 * return so the thread will stop. */
	M_io_hid_close_connection(handle);

	handle->disconnect_timer = M_event_timer_oneshot(M_io_get_event(handle->io), 50, M_FALSE, M_io_hid_disconnect_runner_step2, handle);
	M_io_layer_release(layer);
}


M_bool M_io_hid_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL || handle->connection == NULL)
		return M_TRUE;

	/* Disconnect already started */
	if (!(handle->status & M_IO_HID_STATUS_SYSUP))
		return M_FALSE;

	/* Tell our threads they can stop running. And wake up the writer thread. */
	M_io_hid_signal_shutdown(handle);

	/* Enqueue a task to wait 50ms for writes to flush out, then it will start the 
	 * process of killing the read loop and wait another 50ms for that to exit
	 * before issuing a disconnect */
	handle->disconnect_timer = M_event_timer_oneshot(M_io_get_event(handle->io), 50, M_FALSE, M_io_hid_disconnect_runner_step1, handle);

	return M_FALSE;
}


void M_io_hid_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}


M_bool M_io_hid_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t   *handle = M_io_layer_get_handle(layer);
	M_io_t          *io     = M_io_layer_get_io(layer);
	M_thread_attr_t *tattr;

	if (handle->connection == NULL || !(handle->status & M_IO_HID_STATUS_SYSUP))
		return M_FALSE;

	handle->io = io;

	if (!(handle->status & M_IO_HID_STATUS_READERUP)) {
		tattr = M_thread_attr_create();
		M_thread_attr_set_create_joinable(tattr, M_TRUE);
		handle->read_tid = M_thread_create(tattr, M_io_hid_read_loop, handle);
		M_thread_attr_destroy(tattr);
	}

	if (!(handle->status & M_IO_HID_STATUS_WRITERUP)) {
		tattr = M_thread_attr_create();
		M_thread_attr_set_create_joinable(tattr, M_TRUE);
		handle->write_tid = M_thread_create(tattr, M_io_hid_write_loop, handle);
		M_thread_attr_destroy(tattr);
	}

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);

	/* If the connection was already started check if we have any
	 * read data. It might have come in while moving between event loops
	 * and the event might have been lost. */
	if (handle->status & M_IO_HID_STATUS_READERUP) {
		M_thread_mutex_lock(handle->read_lock);
		if (M_buf_len(handle->readbuf) > 0) {
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
		}
		M_thread_mutex_unlock(handle->read_lock);
	}

	handle->status |= M_IO_HID_STATUS_WRITERUP|M_IO_HID_STATUS_READERUP;
	return M_TRUE;
}
