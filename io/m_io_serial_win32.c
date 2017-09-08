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
#include <objbase.h>
#include <initguid.h>
#include <Setupapi.h>
#include "m_io_w32overlap.h"
#include "m_io_win32_common.h"
#include "m_io_serial_int.h"

struct M_io_handle_w32 {
	/* Settings */
	char                      path[1024];
	M_io_serial_flowcontrol_t flowcontrol;
	M_io_serial_mode_t        mode;
	M_io_serial_baud_t        baud;
	enum M_io_serial_flags    flags;

	/* Original state */
	DCB                       options;
	COMMTIMEOUTS              cto;
};

static void M_io_serial_cleanup(M_io_handle_t *handle)
{
	if (handle->rhandle != M_EVENT_INVALID_HANDLE && handle->priv) {
		if (!(handle->priv->flags & M_IO_SERIAL_FLAG_NO_FLUSH_ON_CLOSE)) {
			/* Flush any pending data so a close doesn't hang */
			PurgeComm(handle->rhandle, PURGE_RXABORT|PURGE_RXCLEAR|PURGE_TXABORT|PURGE_TXCLEAR);
		}

		if (!(handle->priv->flags & M_IO_SERIAL_FLAG_NO_RESTORE_ON_CLOSE)) {
			/* Restore comm state to what is was prior to us messing with it */
			SetCommState(handle->rhandle, &handle->priv->options);
			SetCommTimeouts(handle->rhandle, &handle->priv->cto);
		}

		/* Clear any communications errors, depending on the comm settings, if this
		 * doesn't happen, it could prevent the device from being reopened.
		 * fAbortOnError I'm looking at you.*/
		ClearCommError(handle->rhandle, NULL, NULL);
	}

	M_free(handle->priv);
	handle->priv = NULL;
}

static const struct {
	M_io_serial_baud_t baud;
	DWORD              speed;
} M_io_serial_baud_conversion[] = {
	/* NOTE, must be listed in increasing order, and terminated with M_IO_SERIAL_BAUD_0 */
	{ M_IO_SERIAL_BAUD_110,     CBR_110    },
	{ M_IO_SERIAL_BAUD_300,     CBR_300    },
	{ M_IO_SERIAL_BAUD_600,     CBR_600    },
	{ M_IO_SERIAL_BAUD_1200,    CBR_1200   },
	{ M_IO_SERIAL_BAUD_2400,    CBR_2400   },
	{ M_IO_SERIAL_BAUD_4800,    CBR_4800   },
	{ M_IO_SERIAL_BAUD_9600,    CBR_9600   },
	{ M_IO_SERIAL_BAUD_9600,    CBR_14400  },
	{ M_IO_SERIAL_BAUD_19200,   CBR_19200  },
	{ M_IO_SERIAL_BAUD_38400,   CBR_38400  },
	{ M_IO_SERIAL_BAUD_57600,   CBR_57600  },

	{ M_IO_SERIAL_BAUD_115200,  CBR_115200 },
	{ M_IO_SERIAL_BAUD_128000,  CBR_128000 },
	{ M_IO_SERIAL_BAUD_256000,  CBR_256000 },
	{ M_IO_SERIAL_BAUD_0,       0          },
};


static DWORD M_io_serial_resolve_baud(M_io_serial_baud_t baud)
{
	size_t i;

	for (i=0; M_io_serial_baud_conversion[i].baud != M_IO_SERIAL_BAUD_0; i++) {
		/* Since our enum values are the same as the baud rate, this is safe.
		 * Some OS's may not support all baud rates so we want to choose the
		 * next highest */
		if (baud != M_IO_SERIAL_BAUD_0 && M_io_serial_baud_conversion[i].baud >= baud)
			return M_io_serial_baud_conversion[i].speed;
	}

	/* Requested BAUD_0, and we must be on it as that is the only reason the
	 * loop terminates.  However windows doesn't support BAUD_0, so we set
	 * it to 115200 (and we'll set the DTR bits off elsewhere) */
	if (M_io_serial_baud_conversion[i].baud == M_IO_SERIAL_BAUD_0)
		return CBR_115200;

	/* We must have requested a baud rate *greater* than supported.  Lets just return
	 * the highest supported, which is the entry before BAUD_0 */
	return M_io_serial_baud_conversion[i-1].speed;
}


static void M_io_serial_handle_set_baud_int(DCB *options, M_io_handle_t *handle, M_io_serial_baud_t baud)
{
	/* Windows doesn't actually support a BAUD0, we really just use it to drop DTR,
	 * so we tell windows explicitly to drop DTR */
	if (baud == M_IO_SERIAL_BAUD_0) {
		options->fDtrControl = DTR_CONTROL_DISABLE;
	} else {
		options->fDtrControl = (handle->priv->flowcontrol == M_IO_SERIAL_FLOWCONTROL_HARDWARE)?DTR_CONTROL_HANDSHAKE:DTR_CONTROL_ENABLE;
		options->BaudRate    = M_io_serial_resolve_baud(baud);
	}
}

M_io_error_t M_io_serial_handle_set_baud(M_io_handle_t *handle, M_io_serial_baud_t baud)
{
	DCB options;

	if (handle == NULL || handle->rhandle == M_EVENT_INVALID_HANDLE)
		return M_IO_ERROR_INVALID;

	M_mem_set(&options, 0, sizeof(options));
	options.DCBlength = sizeof(DCB);
	if (!GetCommState(handle->rhandle, &options)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	M_io_serial_handle_set_baud_int(&options, handle, baud);

	if (!SetCommState(handle->rhandle, &options)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	handle->priv->baud = baud;

	return M_IO_ERROR_SUCCESS;
}


static void M_io_serial_handle_set_flowcontrol_int(DCB *options, M_io_handle_t *handle, M_io_serial_flowcontrol_t flowcontrol)
{
	(void)handle;
	/* Clear all Flow Control settings */
	options->fOutX           = FALSE;
	options->fInX            = FALSE;
	options->fOutxCtsFlow    = FALSE;
	options->fOutxDsrFlow    = FALSE;
	options->fDsrSensitivity = FALSE;
	options->fRtsControl     = RTS_CONTROL_ENABLE;
	options->fDtrControl     = DTR_CONTROL_ENABLE;

	switch (flowcontrol) {
		case M_IO_SERIAL_FLOWCONTROL_NONE:
			break;
		case M_IO_SERIAL_FLOWCONTROL_HARDWARE:
			options->fOutxCtsFlow    = TRUE;
			options->fOutxDsrFlow    = TRUE;
			options->fDsrSensitivity = TRUE;
			options->fRtsControl     = RTS_CONTROL_HANDSHAKE;
			options->fDtrControl     = DTR_CONTROL_HANDSHAKE;
			break;
		case M_IO_SERIAL_FLOWCONTROL_SOFTWARE:
			options->fOutX           = TRUE;
			options->fInX            = TRUE;
			break;
	}
}


M_io_error_t M_io_serial_handle_set_flowcontrol(M_io_handle_t *handle, M_io_serial_flowcontrol_t flowcontrol)
{
	DCB options;

	if (handle == NULL || handle->rhandle == M_EVENT_INVALID_HANDLE)
		return M_IO_ERROR_INVALID;

	M_mem_set(&options, 0, sizeof(options));
	options.DCBlength = sizeof(DCB);
	if (!GetCommState(handle->rhandle, &options)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	M_io_serial_handle_set_flowcontrol_int(&options, handle, flowcontrol);

	if (!SetCommState(handle->rhandle, &options)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	handle->priv->flowcontrol = flowcontrol;

	return M_IO_ERROR_SUCCESS;
}


static M_io_error_t M_io_serial_handle_set_mode_int(DCB *options, M_io_handle_t *handle, M_io_serial_mode_t mode)
{
	(void)handle;

	switch (mode & M_IO_SERIAL_MODE_MASK_BITS) {
		case M_IO_SERIAL_MODE_BITS_8:
			options->ByteSize = 8;
			break;
		case M_IO_SERIAL_MODE_BITS_7:
			options->ByteSize = 7;
			break;
		default:
			return M_IO_ERROR_INVALID;
	}

	switch (mode & M_IO_SERIAL_MODE_MASK_PARITY) {
		case M_IO_SERIAL_MODE_PARITY_NONE:
			options->fParity = FALSE;
			options->Parity  = NOPARITY;
			break;
		case M_IO_SERIAL_MODE_PARITY_EVEN:
			options->fParity = TRUE;
			options->Parity = EVENPARITY;
			break;
		case M_IO_SERIAL_MODE_PARITY_ODD:
			options->fParity = TRUE;
			options->Parity  = ODDPARITY;
			break;
		case M_IO_SERIAL_MODE_PARITY_SPACE:
			options->fParity = TRUE;
			options->Parity  = SPACEPARITY;
			break;
		case M_IO_SERIAL_MODE_PARITY_MARK:
			options->fParity = TRUE;
			options->Parity  = MARKPARITY;
			break;
		default:
			return M_IO_ERROR_NOTIMPL;
	}

	switch (mode & M_IO_SERIAL_MODE_MASK_STOPBITS) {
		case M_IO_SERIAL_MODE_STOPBITS_1:
			options->StopBits = ONESTOPBIT;
			break;
		case M_IO_SERIAL_MODE_STOPBITS_2:
			options->StopBits = TWOSTOPBITS;
			break;
	}

	return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_serial_handle_set_mode(M_io_handle_t *handle, M_io_serial_mode_t mode)
{
	DCB          options;
	M_io_error_t err;

	if (handle == NULL || handle->rhandle == M_EVENT_INVALID_HANDLE)
		return M_IO_ERROR_INVALID;

	M_mem_set(&options, 0, sizeof(options));
	options.DCBlength = sizeof(DCB);

	if (!GetCommState(handle->rhandle, &options)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	err = M_io_serial_handle_set_mode_int(&options, handle, mode);
	if (err != M_IO_ERROR_SUCCESS)
		return err;

	if (!SetCommState(handle->rhandle, &options)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	handle->priv->mode = mode;

	return M_IO_ERROR_SUCCESS;
}


static M_io_error_t M_io_serial_handle_set_defaults(DCB *options, M_io_handle_t *handle)
{
	/* From "Remarks" under MSDN for "COMMTIMEOUTS structure":
	 * 
	 * If an application sets ReadIntervalTimeout and ReadTotalTimeoutMultiplier to MAXDWORD
	 * and sets ReadTotalTimeoutConstant to a value greater than zero and less than MAXDWORD,
	 * one of the following occurs when the ReadFile function is called:
	 *
	 * - If there are any bytes in the input buffer, ReadFile returns immediately with the bytes
	 *   in the buffer.
	 * - If there are no bytes in the input buffer, ReadFile waits until a byte arrives and then
	 *   returns immediately.
	 * - If no bytes arrive within the time specified by ReadTotalTimeoutConstant, ReadFile times
	 *   out.
	 * 
	 * NOTE: We use overlapped i/o so I'd think we want these maxed out
	 */
	COMMTIMEOUTS cto = {
		MAXDWORD,   /* ReadIntervalTimeout         */
		MAXDWORD,   /* ReadTotalTimeoutMultiplier  */
		MAXDWORD-1, /* ReadTotalTimeoutConstant    */
		MAXDWORD,   /* WriteTotalTimeoutMultiplier */
		MAXDWORD-1  /* WriteTotalTimeoutConstant   */
	};

	if (handle->priv->flags & M_IO_SERIAL_FLAG_BUSY_POLLING) {
		cto.ReadTotalTimeoutConstant    = 0;
		cto.ReadTotalTimeoutMultiplier  = 0;
		cto.WriteTotalTimeoutConstant   = 0;
		cto.WriteTotalTimeoutMultiplier = 0;
	} else if (handle->priv->flags & M_IO_SERIAL_FLAG_ASYNC_TIMEOUT) {
		/* 1s timeout */
		cto.ReadTotalTimeoutConstant  = 1000;
		cto.WriteTotalTimeoutConstant = 1000;
	}

	if (!SetCommTimeouts(handle->rhandle, &cto)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}


	/* Set DCB options */
	options->BaudRate         = CBR_115200;
	options->ByteSize         = 8;
	options->Parity           = NOPARITY;
	options->StopBits         = ONESTOPBIT;
	options->fBinary          = TRUE;
	options->fNull            = FALSE;
	options->fAbortOnError    = FALSE; /* We don't want to call ClearCommError, so this needs to be false.  There is
	                                    * also some evidence that this might survive a device close if set to true,
	                                    * meaning you might not ever be able to re-open a device if you're never calling
	                                    * ClearCommError */
	options->fOutX            = FALSE;
	options->fInX             = FALSE;
	options->fOutxCtsFlow     = FALSE;
	options->fOutxDsrFlow     = FALSE;
	options->fDtrControl      = DTR_CONTROL_ENABLE;
	options->fDsrSensitivity  = FALSE;
	options->fRtsControl      = RTS_CONTROL_ENABLE;

	return M_IO_ERROR_SUCCESS;
}


M_io_serial_flowcontrol_t M_io_serial_handle_get_flowcontrol(M_io_handle_t *handle)
{
	return handle->priv->flowcontrol;
}

M_io_serial_mode_t M_io_serial_handle_get_mode(M_io_handle_t *handle)
{
	return handle->priv->mode;
}

M_io_serial_baud_t M_io_serial_handle_get_baud(M_io_handle_t *handle)
{
	return handle->priv->baud;
}


static M_io_error_t M_io_serial_handle_configure(M_io_handle_t *handle)
{
	DCB          options;
	M_io_error_t err;

	/* Get the currently-configured serial options */
	M_mem_set(&options, 0, sizeof(options));
	options.DCBlength = sizeof(DCB);
	if (!GetCommState(handle->rhandle, &options)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	/* Save initial state so we can restore it later */
	M_mem_copy(&handle->priv->options, &options, sizeof(options));
	if (!GetCommTimeouts(handle->rhandle, &handle->priv->cto)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	err = M_io_serial_handle_set_defaults(&options, handle);
	if (err != M_IO_ERROR_SUCCESS) {
		return err;
	}

	M_io_serial_handle_set_flowcontrol_int(&options, handle, handle->priv->flowcontrol);

	M_io_serial_handle_set_baud_int(&options, handle, handle->priv->baud);

	err = M_io_serial_handle_set_mode_int(&options, handle, handle->priv->mode);
	if (err != M_IO_ERROR_SUCCESS) {
		return err;
	}

	if (!SetCommState(handle->rhandle, &options)) {
		handle->last_error_sys = GetLastError();
		return M_io_win32_err_to_ioerr(handle->last_error_sys);
	}

	return M_IO_ERROR_SUCCESS;
}


static M_bool M_io_serial_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	HANDLE         shandle;
	M_io_error_t   err;

	if (handle->rhandle == M_EVENT_INVALID_HANDLE && handle->whandle == M_EVENT_INVALID_HANDLE) {
		
		DWORD cfflags = FILE_FLAG_NO_BUFFERING;

		if (!(handle->priv->flags & M_IO_SERIAL_FLAG_BUSY_POLLING)) {
			cfflags |= FILE_FLAG_OVERLAPPED;
		}
		shandle = CreateFile(handle->priv->path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, cfflags, NULL);
		if (shandle == M_EVENT_INVALID_HANDLE) {
			handle->last_error_sys = GetLastError();
			goto fail;
		}

		if (handle->priv->flags & M_IO_SERIAL_FLAG_BUSY_POLLING) {
			M_io_w32overlap_busyemu_update_handle(handle, shandle, shandle);
		} else {
			M_io_w32overlap_update_handle(handle, shandle, shandle);
		}

		/* Configure initial serial port settings */
		err = M_io_serial_handle_configure(handle);
		if (err != M_IO_ERROR_SUCCESS) {
			if (!(handle->priv->flags & M_IO_SERIAL_FLAG_IGNORE_TERMIOS_FAILURE) || err == M_IO_ERROR_NOTIMPL || err == M_IO_ERROR_INVALID) {
				goto fail;
			}
		}

	}
	
	return (handle->priv->flags & M_IO_SERIAL_FLAG_BUSY_POLLING)?M_io_w32overlap_busyemu_init_cb(layer):M_io_w32overlap_init_cb(layer);

fail:
	if (handle->priv->flags & M_IO_SERIAL_FLAG_BUSY_POLLING) {
		M_io_w32overlap_busyemu_close(layer);
	} else {
		M_io_w32overlap_close(layer);
	}

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);

	return M_TRUE; /* not usage issue */
}


M_io_error_t M_io_serial_create(M_io_t **io_out, const char *path, M_io_serial_baud_t baud, M_io_serial_flowcontrol_t flowcontrol, M_io_serial_mode_t mode, M_uint32 flags)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;

	if (io_out == NULL || M_str_isempty(path))
		return M_IO_ERROR_INVALID;

	/* NOTE: we delay actual opening until attached to event handle so we can propagate
	 *       an actual OS error if a failure occurs */
	if (flags & M_IO_SERIAL_FLAG_BUSY_POLLING) {
		handle                = M_io_w32overlap_busyemu_init_handle(NULL, NULL);
	} else {
		handle                = M_io_w32overlap_init_handle(NULL, NULL);
	}
	handle->priv              = M_malloc_zero(sizeof(*handle->priv));
	handle->priv->flags       = flags;
	handle->priv->baud        = baud;
	handle->priv->flowcontrol = flowcontrol;
	handle->priv->mode        = mode;
	handle->priv_cleanup      = M_io_serial_cleanup;
	M_str_cpy(handle->priv->path, sizeof(handle->priv->path), path);

	*io_out   = M_io_init(M_IO_TYPE_STREAM);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_serial_init_cb);

	if (flags & M_IO_SERIAL_FLAG_BUSY_POLLING) {
M_printf("WARNING: Running in Busy Polling mode for %s.  This is not recommended.\n", path); fflush(stdout);
		M_io_callbacks_reg_read(callbacks, M_io_w32overlap_busyemu_read_cb);
		M_io_callbacks_reg_write(callbacks, M_io_w32overlap_busyemu_write_cb);
		M_io_callbacks_reg_processevent(callbacks, M_io_w32overlap_busyemu_process_cb);
		M_io_callbacks_reg_unregister(callbacks, M_io_w32overlap_busyemu_unregister_cb);
		M_io_callbacks_reg_disconnect(callbacks, M_io_w32overlap_busyemu_disconnect_cb);
		M_io_callbacks_reg_destroy(callbacks, M_io_w32overlap_busyemu_destroy_cb);
		M_io_callbacks_reg_state(callbacks, M_io_w32overlap_busyemu_state_cb);
		M_io_callbacks_reg_errormsg(callbacks, M_io_w32overlap_busyemu_errormsg_cb);
	} else {
		M_io_callbacks_reg_read(callbacks, M_io_w32overlap_read_cb);
		M_io_callbacks_reg_write(callbacks, M_io_w32overlap_write_cb);
		M_io_callbacks_reg_processevent(callbacks, M_io_w32overlap_process_cb);
		M_io_callbacks_reg_unregister(callbacks, M_io_w32overlap_unregister_cb);
		M_io_callbacks_reg_disconnect(callbacks, M_io_w32overlap_disconnect_cb);
		M_io_callbacks_reg_destroy(callbacks, M_io_w32overlap_destroy_cb);
		M_io_callbacks_reg_state(callbacks, M_io_w32overlap_state_cb);
		M_io_callbacks_reg_errormsg(callbacks, M_io_w32overlap_errormsg_cb);
	}
	M_io_layer_add(*io_out, M_IO_SERIAL_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return M_IO_ERROR_SUCCESS;
}


DEFINE_GUID(GUID_CLASS_MODEM, 0x4d36e96d, 0xe325, 0x11ce, 0xbf,
	0xc1, 0x08, 0x0, 0x2b, 0xe1, 0x03, 0x18);

static void M_io_serial_enum_modems(M_io_serial_enum_t *serenum)
{
	HKEY  classkey;
	HKEY  subclasskey;
	DWORD index = 0;
	char  subkey[128];
	DWORD subkey_len;


	/* Open the registry class key that matches the GUID_CLASS_MODEM */
	/* This is apparently a setup/installer class, not a device class, don't ask why :/ */
	classkey = SetupDiOpenClassRegKeyExA(&GUID_CLASS_MODEM, KEY_READ, DIOCR_INSTALLER, NULL, NULL);
	if (classkey == INVALID_HANDLE_VALUE) {
		return;
	}

	/* scan through each key under GUID_CLASS_MODEM, _not_ recursively! */
	subkey_len = sizeof(subkey);
	while (RegEnumKeyEx(classkey, index++, subkey, &subkey_len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
		char  AttachedTo[1024];
		char  FriendlyName[1024];
		char  Model[1024];
		DWORD AttachedTo_len;
		DWORD FriendlyName_len;
		DWORD Model_len;

		if (RegOpenKeyEx(classkey, subkey, 0, KEY_READ, &subclasskey) != ERROR_SUCCESS) {
			if (subclasskey != NULL)
				RegCloseKey(subclasskey);
			subclasskey = NULL;
			/* Need to ignore failures on opening certain keys for reading.  On Windows
			 * Vista, the 'Properties' keys are not allowed even for administrators */
			subkey_len = sizeof(subkey);
			continue;
		}

		AttachedTo_len   = sizeof(AttachedTo);
		FriendlyName_len = sizeof(FriendlyName);
		Model_len        = sizeof(Model);

		M_mem_set(AttachedTo,   0, AttachedTo_len);
		M_mem_set(FriendlyName, 0, FriendlyName_len);
		M_mem_set(Model,        0, Model_len);

		RegQueryValueEx(subclasskey, "AttachedTo",   NULL, NULL, (BYTE *)AttachedTo,   &AttachedTo_len);
		RegQueryValueEx(subclasskey, "FriendlyName", NULL, NULL, (BYTE *)FriendlyName, &FriendlyName_len);
		RegQueryValueEx(subclasskey, "Model",        NULL, NULL, (BYTE *)Model,        &Model_len);

		if (!M_str_isempty(AttachedTo) && (!M_str_isempty(FriendlyName) || !M_str_isempty(Model))) {
			char        path[1024];

			M_snprintf(path, sizeof(path), "\\\\.\\%s", AttachedTo);
			M_io_serial_enum_add(serenum, path, M_str_isempty(FriendlyName)?Model:FriendlyName);
		}
		RegCloseKey(subclasskey);
		subclasskey = NULL;

		/* Windows keeps resetting this on every call, don't forget to set it back! */
		subkey_len = sizeof(subkey);
	}
	RegCloseKey(classkey);
}


// see http://msdn.microsoft.com/en-us/library/ms791134.aspx for list of GUID classes
#ifndef GUID_DEVINTERFACE_COMPORT
	DEFINE_GUID(GUID_DEVINTERFACE_COMPORT, 0x86E0D1E0, 0x8089, 0x11D0, 0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73 );
#endif
#ifndef GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR
	DEFINE_GUID(GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR, 0x4D36E978, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 );
#endif

static void M_io_serial_enum_serial(M_io_serial_enum_t *serenum)
{
	HDEVINFO         hDevInfo = INVALID_HANDLE_VALUE;
	HKEY             regkey;
	SP_DEVINFO_DATA  devdata;
	DWORD            idx = 0;

	/* Get list of PRESENT INTERFACE COM PORT devices */
	hDevInfo = SetupDiGetClassDevsA(&GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR, NULL, NULL, DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE) {
		return;
	}

	devdata.cbSize = sizeof(devdata);
	while (SetupDiEnumDeviceInfo(hDevInfo, idx++, &devdata)) {
		char  temp[256];
		char  path[256];
		char  frname[1024];
		char  descr[1024];
		DWORD temp_len;

		regkey = SetupDiOpenDevRegKey(hDevInfo, &devdata, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
		if (regkey == INVALID_HANDLE_VALUE) {
			devdata.cbSize = sizeof(devdata);
			continue;
		}

		M_mem_set(temp, 0, sizeof(temp));
		temp_len = sizeof(temp);
		RegQueryValueEx(regkey, "PortName", NULL, NULL, (BYTE *)temp, &temp_len);
		RegCloseKey(regkey);

		/* This seems to enumerate Parallel/Printer ports as well, skip those */
		if (M_str_isempty(temp) || M_str_eq_max(temp, "LPT", 3)) {
			devdata.cbSize = sizeof(devdata);
			continue;
		}

		/* Prefix any "COM" entries with \\.\, anything else should already have the right prefix */
		M_snprintf(path, sizeof(path), "%s%s", M_str_eq_max(temp, "COM", 3)?"\\\\.\\":"", temp);

		/* Get friendly device name */
		M_mem_set(frname, 0, sizeof(frname));
		SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devdata, SPDRP_FRIENDLYNAME, NULL, (PBYTE)frname, sizeof(frname)-1, NULL);

		/* Get device description */
		M_mem_set(descr, 0, sizeof(descr));
		SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devdata, SPDRP_DEVICEDESC, NULL, (PBYTE)descr, sizeof(descr)-1, NULL);

		/* Determine if device is USB ... why? I don't know */
		M_mem_set(temp, 0, sizeof(temp));
		if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devdata, SPDRP_LOCATION_INFORMATION, NULL, (PBYTE)temp, sizeof(temp)-1, NULL)) {
			if (M_str_eq_max(temp, "USB", 3)) {
				/* XXX: Do we care if it is USB connected? */
			}
		}

		if (M_str_isempty(frname) && M_str_isempty(descr))
			M_snprintf(frname, sizeof(frname), "%s", path);


		M_io_serial_enum_add(serenum, path, M_str_isempty(frname)?descr:frname);

		devdata.cbSize = sizeof(devdata);
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);
}


M_io_serial_enum_t *M_io_serial_enum(M_bool include_modems)
{
	M_io_serial_enum_t        *serenum;

	serenum = M_io_serial_enum_init();

	if (include_modems)
		M_io_serial_enum_modems(serenum);

	M_io_serial_enum_serial(serenum);
	return serenum;
}

