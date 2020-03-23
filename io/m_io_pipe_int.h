#ifndef __M_IO_PIPE_INT_H__
#define __M_IO_PIPE_INT_H__

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"

M_EVENT_HANDLE M_io_pipe_get_fd(M_io_t *io);

#endif
