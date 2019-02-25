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

struct M_io_handle {
};


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
		jstring  key_o     = NULL;
		jobject  dev       = NULL;
		jstring  sval      = NULL;
		jint     dev_class = -1;
		char    *dev_name  = NULL;
		char    *manuf     = NULL;
		char    *product   = NULL;
		char    *serialnum = NULL;
		jint     pid       = -1;
		jint     vid       = -1;

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

		/* Pull out the device name. */
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getDeviceName", 0))
			goto loop_cleanup;
		dev_name = M_io_jni_jstring_to_pchar(env, sval);

		/* Pull out the manufacturer name. */
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getManufacturerName", 0))
			goto loop_cleanup;
		manuf = M_io_jni_jstring_to_pchar(env, sval);

		/* Pull out the product id. */
		if (!M_io_jni_call_jint(&pid, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getProductId", 0) || pid == -1)
			goto loop_cleanup;

		/* Pull out the product name. */
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getProductName", 0))
			goto loop_cleanup;
		product = M_io_jni_jstring_to_pchar(env, sval);

		/* Pull out the serial number. */
		if (!M_io_jni_call_jobject(&sval, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getSerialNumber", 0))
			goto loop_cleanup;
		serialnum = M_io_jni_jstring_to_pchar(env, sval);

		/* Pull out the vendor id. */
		if (!M_io_jni_call_jint(&vid, NULL, 0, env, dev, "android/hardware/usb/UsbDevice.getVendorId", 0) || pid == -1)
			goto loop_cleanup;

		M_io_hid_enum_add(hidenum,
				/* Info about this enumerated device */
				dev_name, manuf, product, serialnum, (M_uint16)vid, (M_uint16)pid,
				/* Search/Match criteria */
				vendorid, productids, num_productids, serial);
loop_cleanup:
		M_io_jni_deletelocalref(env, &dev);
		M_io_jni_deletelocalref(env, &key_o);
		M_free(dev_name);
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

char *M_io_hid_get_manufacturer(M_io_t *io)
{
	(void)io;
	return NULL;
}

char *M_io_hid_get_path(M_io_t *io)
{
	(void)io;
	return NULL;
}

char *M_io_hid_get_product(M_io_t *io)
{
	(void)io;
	return NULL;
}

M_uint16 M_io_hid_get_productid(M_io_t *io)
{
	(void)io;
	return 0;
}

M_uint16 M_io_hid_get_vendorid(M_io_t *io)
{
	(void)io;
	return 0;
}

char *M_io_hid_get_serial(M_io_t *io)
{
	(void)io;
	return NULL;
}

void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size)
{
	(void)io;
	if (max_input_size != NULL)
		*max_input_size = 0;
	if (max_output_size != NULL)
		*max_output_size = 0;
}

M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr)
{
	(void)devpath;
	*ioerr = M_IO_ERROR_NOTIMPL;
	return NULL;
}

M_bool M_io_hid_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	(void)layer;
	(void)error;
	(void)err_len;
	return M_FALSE;
}

M_io_state_t M_io_hid_state_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_IO_STATE_ERROR;
}

void M_io_hid_destroy_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_hid_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	return M_FALSE;
}

M_io_error_t M_io_hid_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)write_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

M_io_error_t M_io_hid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)read_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
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
	(void)layer;
	return M_FALSE;
}
