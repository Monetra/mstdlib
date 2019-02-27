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

struct M_io_handle {
	jobject           connection;  /*!< UsbDeviceConnection */
	jobject           interface;   /*!< UsbInterface */
	jobject           in_req;      /*!< UsbRequest */
	jobject           out_req;     /*!< UsbRequest */
	jobject           in_buffer;   /*!< ByteBuffer backing for reads. */
	jobject           out_buffer;  /*!< ByteBuffer backing for writes. */
	M_io_t           *io;
	M_buf_t          *readbuf;     /*!< Reads are transferred via a buffer. */
	M_buf_t          *writebuf;    /*!< Write are transferred via a buffer. */
	M_thread_mutex_t *data_lock;   /*!< Lock when manipulating data buffers. */
	M_bool            run;         /*!< Should the process thread continue running. */
	M_threadid_t      process_tid; /*!< Thread id for the process thread. */
	char              error[256];  /*!< Error buffer for description of last system error. */

	char             *path;
	char             *manufacturer;
	char             *product;
	char             *serial;
	M_uint16          productid;
	M_uint16          vendorid;
	size_t            max_input_report_size;
	size_t            max_output_report_size;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static jobject get_usb_manager(JNIEnv *env)
{
	jobject app_context = NULL;
	jobject sname       = NULL;
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
	return manager;
}

/* Should be in a data_lock. */
static M_bool M_io_hid_queue_read(JNIEnv *env, M_io_handle_t *handle)
{
	jbyteArray     data = NULL;
	jbyte         *body;
	jobject        rv   = NULL;
	M_bool ret = M_FALSE;

	/* Zero the buffer. */
	data = (*env)->NewByteArray(env, (jsize)handle->max_input_report_size);
	body = (*env)->GetByteArrayElements(env, data, 0);
	M_mem_set(body, 0, handle->max_input_report_size);
	if (!M_io_jni_call_jobject(&rv, handle->error, sizeof(handle->error), env, handle->in_buffer, "java/nio/ByteBuffer.put", 3, data, 0, 0))
		goto done;
	M_io_jni_deletelocalref(env, &rv);
	rv = NULL;

	/* Clear the buffer's internal markers. This doesn't 0 the memory which is why we did that manually. */
	if (!M_io_jni_call_jobject(&rv, handle->error, sizeof(handle->error), env, handle->in_buffer, "java/nio/ByteBuffer.clear", 0))
		goto done;
	M_io_jni_deletelocalref(env, &rv);
	rv = NULL;

	/* Queue that we want to read. */
	if (!M_io_jni_call_jvoid(handle->error, sizeof(handle->error), env, handle->in_req, "android/hardware/usb/UsbRequest.queue", 1, handle->in_buffer))
		goto done;

	ret = M_TRUE;

done:
	return ret;
}

/* Should be in a data_lock. */
static M_bool M_io_hid_queue_write(JNIEnv *env, M_io_handle_t *handle)
{
	jbyte     *body;
	jbyteArray data = NULL;
	jobject    rv   = NULL;
	size_t     len;
	M_bool     ret  = M_FALSE;

	if (env == NULL)
		env = M_io_jni_getenv();
	if (env == NULL)
		goto done;

	len = M_buf_len(handle->writebuf);
	if (len == 0)
		return M_TRUE;

	/* Clear the out buffer. */
	if (!M_io_jni_call_jobject(&rv, handle->error, sizeof(handle->error), env, handle->out_buffer, "java/nio/ByteBuffer.clear", 0))
		goto done;
	M_io_jni_deletelocalref(env, &rv);
	rv = NULL;

	/* Fill the write buffer with the data we want written. */
	len  = M_MIN(len, handle->max_output_report_size);
	data = (*env)->NewByteArray(env, (jsize)len);
	body = (*env)->GetByteArrayElements(env, data, 0);
	M_mem_move(body, M_buf_peek(handle->writebuf), len);

	if (!M_io_jni_call_jobject(&rv, handle->error, sizeof(handle->error), env, handle->out_buffer, "java/nio/ByteBuffer.put", 3, data, 0, len))
		goto done;
	M_io_jni_deletelocalref(env, rv);
	rv = NULL;
	
	if (!M_io_jni_call_jvoid(handle->error, sizeof(handle->error), env, handle->out_req, "android/hardware/usb/UsbRequest.queue", 1, handle->out_buffer))
		goto done;

	ret = M_TRUE;

done:
	M_io_jni_deletelocalref(env, &data);
	return ret;
}

static void M_io_hid_close_device(JNIEnv *env, M_io_handle_t *handle)
{
	jboolean rv;

	if (handle == NULL || handle->connection == NULL)
		return;

	if (env == NULL)
		env = M_io_jni_getenv();

	/* Tell the processing thread it should stop running. */
	handle->run = M_FALSE;

	/* Close any pending requests we have. */
	M_io_jni_call_jboolean(&rv, NULL, 0, env, handle->in_req, "android/hardware/usb/UsbRequest.cancel", 0);
	M_io_jni_call_jvoid(NULL, 0, env, handle->in_req, "android/hardware/usb/UsbRequest.close", 0);
	M_io_jni_call_jboolean(&rv, NULL, 0, env, handle->out_req, "android/hardware/usb/UsbRequest.cancel", 0);
	M_io_jni_call_jvoid(NULL, 0, env, handle->out_req, "android/hardware/usb/UsbRequest.close", 0);

	/* Stop the processing thread. */
	M_thread_join(handle->process_tid, NULL);

	/* Release Interface. */
	M_io_jni_call_jboolean(&rv, NULL, 0, env, handle->connection, "android/hardware/usb/UsbDeviceConnection.releaseInterface", 1, handle->interface);

	/* Close connection */
	M_io_jni_call_jvoid(NULL, 0, env, handle->connection, "android/hardware/usb/UsbDeviceConnection.close", 0);

	/* Clear everything. */
	M_io_jni_delete_globalref(env, handle->connection);
	M_io_jni_delete_globalref(env, handle->interface);
	M_io_jni_delete_globalref(env, handle->in_req);
	M_io_jni_delete_globalref(env, handle->out_req);
	M_io_jni_delete_globalref(env, handle->in_buffer);
	M_io_jni_delete_globalref(env, handle->out_buffer);

	handle->connection = NULL;
	handle->interface  = NULL;
	handle->in_req     = NULL;
	handle->out_req    = NULL;
	handle->in_buffer  = NULL;
	handle->out_buffer = NULL;
}

static M_bool M_io_hid_dev_info(JNIEnv *env, jobject device,
		char **path, char **manuf, char **product, char **serial,
		M_uint16 *vendorid, M_uint16 *productid)
{
	jstring sval;
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
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getDeviceName", 0)) {
			goto err;
		}
		*path = M_io_jni_jstring_to_pchar(env, sval);
	}

	if (manuf != NULL) {
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getManufacturerName", 0)) {
			goto err;
		}
		*manuf = M_io_jni_jstring_to_pchar(env, sval);
	}

	if (product != NULL) {
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getProductName", 0)) {
			goto err;
		}
		*product = M_io_jni_jstring_to_pchar(env, sval);
	}

	if (serial != NULL) {
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getSerialNumber", 0)) {
			goto err;
		}
		*serial = M_io_jni_jstring_to_pchar(env, sval);
	}

	if (vendorid != NULL) {
		/* Pull out the vendor id. */
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

		/* Check if this is a hid device. */
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
			jint   j;
			M_bool have_hid = M_FALSE;

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

	if (handle != NULL) {
		ret = M_strdup(handle->manufacturer);
	}

	M_io_layer_release(layer);
	return ret;
}

char *M_io_hid_get_path(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL) {
		ret = M_strdup(handle->path);
	}

	M_io_layer_release(layer);
	return ret;
}

char *M_io_hid_get_product(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL) {
		ret = M_strdup(handle->product);
	}

	M_io_layer_release(layer);
	return ret;
}

M_uint16 M_io_hid_get_productid(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint16       ret    = 0;

	if (handle != NULL) {
		ret = handle->productid;
	}

	M_io_layer_release(layer);
	return ret;
}

M_uint16 M_io_hid_get_vendorid(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint16       ret    = 0;

	if (handle != NULL) {
		ret = handle->vendorid;
	}

	M_io_layer_release(layer);
	return ret;
}

char *M_io_hid_get_serial(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_hid_get_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL) {
		ret = M_strdup(handle->serial);
	}

	M_io_layer_release(layer);
	return ret;
}

void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	size_t         my_in;
	size_t         my_out;

	if (max_input_size == NULL) {
		max_input_size  = &my_in;
	}
	if (max_output_size == NULL) {
		max_output_size = &my_out;
	}

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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_io_hid_process_loop_read_resp(JNIEnv *env, M_io_handle_t *handle)
{
	M_io_layer_t  *layer;
	unsigned char *dbuf;
	jbyteArray     data = NULL;
	jobject        rv   = NULL;
	jint           len;
	size_t         slen;
	M_bool         ret  = M_FALSE;

	/* Flip the buffer and find out how much data was read. */
	if (!M_io_jni_call_jobject(&rv, handle->error, sizeof(handle->error), env, handle->in_buffer, "java/nio/ByteBuffer.flip", 0))
		goto done;
	M_io_jni_deletelocalref(env, &rv);
	rv = NULL;

	if (!M_io_jni_call_jint(&len, handle->error, sizeof(handle->error), env, handle->in_buffer, "java/nio/ByteBuffer.remaining", 0))
		goto done;

	if (len <= 0) {
		ret = M_TRUE;
		goto done;
	}

	data = (*env)->NewByteArray(env, (jsize)len);
	if (!M_io_jni_call_jobject(&rv, handle->error, sizeof(handle->error), env, handle->in_buffer, "java/nio/ByteBuffer.get", 1, data))
		goto done;
	M_io_jni_deletelocalref(env, &rv);
	rv = NULL;

	/* Direct write the data into our read buffer. */
	slen = (size_t)len;
	dbuf = M_buf_direct_write_start(handle->readbuf, &slen);
	(*env)->GetByteArrayRegion(env, data, 0, (jsize)len, (jbyte *)dbuf);
	M_buf_direct_write_end(handle->readbuf, (size_t)len);

	/* Pass along we have read data. */
	layer = M_io_layer_acquire(handle->io, 0, NULL);
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
	M_io_layer_release(layer);

	/* Queue up another read. */
	ret = M_io_hid_queue_read(env, handle);

done:
	M_io_jni_deletelocalref(env, &rv);
	M_io_jni_deletelocalref(env, &data);

	return ret;
}

static M_bool M_io_hid_process_loop_write_resp(JNIEnv *env, M_io_handle_t *handle)
{
	M_io_layer_t *layer;
	jobject       rv  = NULL;
	jint          len;
	M_bool        ret = M_TRUE;

	/* Flip the buffer and find out how much data was written. */
	if (!M_io_jni_call_jobject(&rv, handle->error, sizeof(handle->error), env, handle->out_buffer, "java/nio/ByteBuffer.flip", 0))
		goto done;
	M_io_jni_deletelocalref(env, &rv);
	rv = NULL;

	if (!M_io_jni_call_jint(&len, handle->error, sizeof(handle->error), env, handle->out_buffer, "java/nio/ByteBuffer.remaining", 0))
		goto done;

	/* Drop all data that was written. */
	M_buf_drop(handle->writebuf, M_buf_len(handle->writebuf)-(size_t)len);

	M_io_jni_delete_globalref(env, handle->out_buffer);
	handle->out_buffer = NULL;

	if (M_buf_len(handle->writebuf) != 0) {
		/* We still have data that hasn't been written. We want to try writing it again. */
		ret = M_io_hid_queue_write(env, handle);
	} else {
		/* Pass along we can write data again. */
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE);
		M_io_layer_release(layer);
	}

done:
	return ret;
}

static void *M_io_hid_process_loop(void *arg)
{
	M_io_handle_t *handle     = arg;
	M_io_layer_t  *layer;
	M_bool         disconnect = M_FALSE;
	jobject        env;
	jint           dir_in      = -1;
	jint           dir_out     = -1;

	env = M_io_jni_getenv();
	if (env == NULL) {
		M_snprintf(handle->error, sizeof(handle->error), "Failed to get ENV");
		goto err;
	}

	/* Get the in and out direction constants so we can determine the direction when we have an event. */
	if (!M_io_jni_call_jintField(&dir_in, handle->error, sizeof(handle->error), env, NULL, "android/hardware/usb/UsbConstants.USB_DIR_IN") || dir_in == -1)
		goto err;

	if (!M_io_jni_call_jintField(&dir_out, handle->error, sizeof(handle->error), env, NULL, "android/hardware/usb/UsbConstants.USB_DIR_OUT") || dir_out == -1)
		goto err;

	/* Queue a read operation and start waiting for data. */
	M_io_hid_queue_read(env, handle);

	while (handle->run) {
		jobject response  = NULL;
		jobject endpoint  = NULL;
		jint    direction = -1;
		M_bool  ret       = M_FALSE;

		/* Blocking call. */
		if (!M_io_jni_call_jobject(&response, handle->error, sizeof(handle->error), env, handle->connection, "android/hardware/usb/UsbDeviceConnection.requestWait", 0) || response == NULL) {
			if (!handle->run) {
				/* We're closing so we won't get a response back. */
				break;
			}
			disconnect = M_TRUE;
			goto err;
		}

		/* Determine if this is a read or write response. */
		if (!M_io_jni_call_jobject(&endpoint, handle->error, sizeof(handle->error), env, response, "android/hardware/usb/UsbRequest.getEndpoint", 0) || endpoint == NULL)
			goto err;
		
		if (!M_io_jni_call_jint(&direction, handle->error, sizeof(handle->error), env, endpoint, "android/hardware/usb/UsbEndpoint.getDirection", 0))
			goto err;

		/* Process the response. */
		M_thread_mutex_lock(handle->data_lock);
		if (direction == dir_in) {
			ret = M_io_hid_process_loop_read_resp(env, handle);
		} else if (direction == dir_out) {
			ret = M_io_hid_process_loop_write_resp(env, handle);
		} else {
			M_snprintf(handle->error, sizeof(handle->error), "%s", "Unknown endpoint direction");
			ret = M_FALSE;
		}
		M_thread_mutex_unlock(handle->data_lock);

		/* Clean up. */
		M_io_jni_deletelocalref(env, &endpoint);
		M_io_jni_deletelocalref(env, &response);

		if (!ret)
			goto err;
	}

	return NULL;

err:
	layer = M_io_layer_acquire(handle->io, 0, NULL);
	M_io_hid_close_device(env, handle);
	if (disconnect) {
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
	} else {
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
	}
	M_io_layer_release(layer);

	return NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr)
{
	JNIEnv          *env                     = NULL;
	M_io_handle_t   *handle                  = NULL;
	M_thread_attr_t *tattr;
	jobject          manager                 = NULL;
	jobject          dev_list                = NULL;
	jobject          device                  = NULL;
	jobject          ep_in                   = NULL;
	jobject          ep_out                  = NULL;
	jobject          connection              = NULL;
	jobject          interface               = NULL;
	jobject          in_req                  = NULL;
	jobject          out_req                 = NULL;
	jobject          in_buffer               = NULL;
	jobject          out_buffer              = NULL;
	jstring          sval                    = NULL;
	jint             hid_class               = -1;
	jint             dev_class               = -1;
	jint             dir_in                  = -1;
	jint             dir_out                 = -1;
	jint             report_size             = -1;
	char            *path                    = NULL;
	char            *manufacturer            = NULL;
	char            *product                 = NULL;
	char            *serial                  = NULL;
	M_uint16         productid               = 0;
	M_uint16         vendorid                = 0;
	size_t           max_input_report_size   = 32;
	size_t           max_output_report_size  = 32;
	M_bool           opened                  = M_FALSE;
	jboolean rb;
	jint   cnt      = 0;
	jint   i;

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

	/* Get the Usb endpoint directions. */
	if (!M_io_jni_call_jintField(&dir_in, handle->error, sizeof(handle->error), env, NULL, "android/hardware/usb/UsbConstants.USB_DIR_IN") || dir_in == -1)
		goto err;

	if (!M_io_jni_call_jintField(&dir_out, handle->error, sizeof(handle->error), env, NULL, "android/hardware/usb/UsbConstants.USB_DIR_OUT") || dir_out == -1)
		goto err;

	/* Get the usb device list. */
	if (!M_io_jni_call_jobject(&dev_list, NULL, 0, env, manager, "android/hardware/usb/UsbManager.getDeviceList", 0) || dev_list == NULL) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	/* Pull out the device we want to operate on. */
	sval = M_io_jni_pchar_to_jstring(env, devpath);
	if (!M_io_jni_call_jobject(&device, NULL, 0, env, dev_list, "java/util/HashMap.get", 1, sval) || device == NULL) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		goto err;
	}
	M_io_jni_deletelocalref(env, &sval);
	sval = NULL;

	/* Find the HID interface. */
	if (!M_io_jni_call_jint(&cnt, NULL, 0, env, device, "android/hardware/usb/UsbDevice.getInterfaceCount", 0) || cnt <= 0) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

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
	if (!M_io_jni_call_jint(&cnt, NULL, 0, env, interface, "android/hardware/usb/UsbInterface.getEndpointCount", 0) || cnt <= 0) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	for (i=0; i<cnt; i++) {
		jobject endpoint = NULL;
		jint    direction;

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
	if (!M_io_jni_call_jboolean(&rb, NULL, 0, env, connection, "android/hardware/usb/UsbDeviceConnection.claimInterface", 2, interface, M_FALSE) || !rb) {
		*ioerr = M_IO_ERROR_CONNREFUSED;
		goto err;
	}

	/* Create and initialize the read UsbRequests. */
	if (!M_io_jni_new_object(&in_req, NULL, 0, env, "android/hardware/usb/UsbRequest.<init>", 0) || in_req == NULL) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	if (!M_io_jni_call_jboolean(&rb, NULL, 0, env, in_req, "android/hardware/usb/UsbRequest.initialize", 2, connection, ep_in) || !rb) {
		*ioerr = M_IO_ERROR_CONNREFUSED;
		goto err;
	}

	if (!M_io_jni_new_object(&out_req, NULL, 0, env, "android/hardware/usb/UsbRequest.<init>", 0) || out_req == NULL) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	if (!M_io_jni_call_jboolean(&rb, NULL, 0, env, out_req, "android/hardware/usb/UsbRequest.initialize", 2, connection, ep_out) || !rb) {
		*ioerr = M_IO_ERROR_CONNREFUSED;
		goto err;
	}

	/* Get report sizes. */
	if (!M_io_jni_call_jint(&report_size, NULL, 0, env, ep_in, "android/hardware/usb/UsbEndpoint.getMaxPacketSize", 0) || report_size <= 0) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}
	max_input_report_size = (size_t)report_size;

	if (!M_io_jni_call_jint(&report_size, NULL, 0, env, ep_out, "android/hardware/usb/UsbEndpoint.getMaxPacketSize", 0) || report_size <= 0) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}
	max_output_report_size = (size_t)report_size;

	/* Create the read and write backing byte buffers. */
	if (!M_io_jni_call_jobject(&in_buffer, NULL, 0, env, NULL, "java/nio/ByteBuffer.allocate", 1, (jint)max_input_report_size) || in_buffer == NULL) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	if (!M_io_jni_call_jobject(&out_buffer, NULL, 0, env, NULL, "java/nio/ByteBuffer.allocate", 1, (jint)max_output_report_size) || out_buffer == NULL) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	/* Get the device metadata. */
	if (!M_io_hid_dev_info(env, device, &path, &manufacturer, &product, &serial, &vendorid, &productid))
		goto err;

	/* Setup the handle. */
	handle                         = M_malloc_zero(sizeof(*handle));
	handle->readbuf                = M_buf_create();
	handle->writebuf               = M_buf_create();
	handle->data_lock              = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	handle->run                    = M_TRUE;
	handle->path                   = path;
	handle->manufacturer           = manufacturer;
	handle->product                = product;
	handle->serial                 = serial;
	handle->productid              = productid;
	handle->vendorid               = vendorid;
	handle->max_input_report_size  = max_input_report_size;
	handle->max_output_report_size = max_output_report_size;

	/* Store the Java objects. */
	handle->connection = M_io_jni_create_globalref(env, connection);
	handle->interface  = M_io_jni_create_globalref(env, interface);
	handle->in_req     = M_io_jni_create_globalref(env, in_req);
	handle->out_req    = M_io_jni_create_globalref(env, out_req);
	handle->in_buffer  = M_io_jni_create_globalref(env, in_buffer);
	handle->out_buffer = M_io_jni_create_globalref(env, out_buffer);

	/* Start the processing thread. */
	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	handle->process_tid = M_thread_create(tattr, M_io_hid_process_loop, handle);
	M_thread_attr_destroy(tattr);

	M_io_jni_deletelocalref(env, &ep_in);
	M_io_jni_deletelocalref(env, &ep_out);
	M_io_jni_deletelocalref(env, &device);
	M_io_jni_deletelocalref(env, &dev_list);
	return handle;

err:
	/* Try to close the device if it was opened. */
	if (opened)
		M_io_jni_call_jvoid(NULL, 0, env, connection, "android/hardware/usb/UsbDeviceConnection.close", 0);

	M_io_jni_deletelocalref(env, &sval);
	M_io_jni_deletelocalref(env, &connection);
	M_io_jni_deletelocalref(env, &device);
	M_io_jni_deletelocalref(env, &dev_list);
	M_io_jni_deletelocalref(env, &interface);
	M_io_jni_deletelocalref(env, &ep_in);
	M_io_jni_deletelocalref(env, &ep_out);
	M_io_jni_deletelocalref(env, &in_req);
	M_io_jni_deletelocalref(env, &out_req);
	M_io_jni_deletelocalref(env, &in_buffer);
	M_io_jni_deletelocalref(env, &out_buffer);

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

	M_io_hid_close_device(NULL, handle);

	M_thread_mutex_destroy(handle->data_lock);
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

	(void)meta;

	M_thread_mutex_lock(handle->data_lock);

	/* Note: We have to finish with the previous write before we can start the next one.
	 * if in_write is false the write thread will be blocked from reading from or modifying the writebuf because
	 * we're holding the lock. The write thread will only do something with writebuf when in_write is true or
	 * it holds the lock.
	 */
	if (M_buf_len(handle->writebuf) > 0) {
		M_thread_mutex_unlock(handle->data_lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	if (handle->connection == NULL) {
		M_thread_mutex_unlock(handle->data_lock);
		return M_IO_ERROR_NOTCONNECTED;
	}

	if (buf == NULL || *write_len == 0) {
		M_thread_mutex_unlock(handle->data_lock);
		return M_IO_ERROR_SUCCESS;
	}

	M_buf_add_bytes(handle->writebuf, buf, *write_len);
	M_io_hid_queue_write(NULL, handle);

	M_thread_mutex_unlock(handle->data_lock);
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

	if (handle->connection == NULL)
		return M_IO_ERROR_NOTCONNECTED;

	M_thread_mutex_lock(handle->data_lock);

	if (M_buf_len(handle->readbuf) == 0) {
		M_thread_mutex_unlock(handle->data_lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	len = M_buf_len(handle->readbuf);
	/* Don't try to read more than we can. */
	len = M_MIN(len, *read_len);

	/* Copy from the read buffer into the output buffer. */
	M_mem_copy(buf+offset, M_buf_peek(handle->readbuf), len);
	/* Drop what we read. */
	M_buf_drop(handle->readbuf, len);
	/* Our read total is how much we read from the readbuf plus how much we pre-filled. */
	*read_len = len+offset;

	M_thread_mutex_unlock(handle->data_lock);
	return M_IO_ERROR_SUCCESS;
}

M_bool M_io_hid_disconnect_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_TRUE;
}

void M_io_hid_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_hid_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);

	if (handle->connection == NULL)
		return M_FALSE;

	handle->io = io;

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);
	return M_TRUE;
}
