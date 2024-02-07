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

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"
#include "m_io_serial_int.h"

M_io_error_t M_io_serial_set_baud(M_io_t *io, M_io_serial_baud_t baud)
{
    M_io_layer_t  *layer;
    M_io_handle_t *handle;
    M_io_error_t   err;

    layer  = M_io_layer_acquire(io, 0, M_IO_SERIAL_NAME);
    if (layer == NULL)
        return M_IO_ERROR_INVALID;

    handle = M_io_layer_get_handle(layer);

    err    = M_io_serial_handle_set_baud(handle, baud);

    M_io_layer_release(layer);
    return err;
}


M_io_error_t M_io_serial_set_flowcontrol(M_io_t *io, M_io_serial_flowcontrol_t flowcontrol)
{
    M_io_layer_t  *layer;
    M_io_handle_t *handle;
    M_io_error_t   err;

    layer  = M_io_layer_acquire(io, 0, M_IO_SERIAL_NAME);
    if (layer == NULL)
        return M_IO_ERROR_INVALID;

    handle = M_io_layer_get_handle(layer);

    err    = M_io_serial_handle_set_flowcontrol(handle, flowcontrol);

    M_io_layer_release(layer);
    return err;
}


M_io_error_t M_io_serial_set_mode(M_io_t *io, M_io_serial_mode_t mode)
{
    M_io_layer_t  *layer;
    M_io_handle_t *handle;
    M_io_error_t   err;

    layer  = M_io_layer_acquire(io, 0, M_IO_SERIAL_NAME);
    if (layer == NULL)
        return M_IO_ERROR_INVALID;

    handle = M_io_layer_get_handle(layer);

    err    = M_io_serial_handle_set_mode(handle, mode);

    M_io_layer_release(layer);
    return err;
}


M_io_error_t M_io_serial_get_flowcontrol(M_io_t *io, M_io_serial_flowcontrol_t *flowcontrol)
{
    M_io_layer_t  *layer;
    M_io_handle_t *handle;

    layer        = M_io_layer_acquire(io, 0, M_IO_SERIAL_NAME);
    if (layer == NULL || flowcontrol == NULL)
        return M_IO_ERROR_INVALID;

    handle       = M_io_layer_get_handle(layer);

    *flowcontrol = M_io_serial_handle_get_flowcontrol(handle);

    M_io_layer_release(layer);
    return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_serial_get_mode(M_io_t *io, M_io_serial_mode_t *mode)
{
    M_io_layer_t  *layer;
    M_io_handle_t *handle;

    layer        = M_io_layer_acquire(io, 0, M_IO_SERIAL_NAME);
    if (layer == NULL || mode == NULL)
        return M_IO_ERROR_INVALID;

    handle       = M_io_layer_get_handle(layer);

    *mode        = M_io_serial_handle_get_mode(handle);

    M_io_layer_release(layer);
    return M_IO_ERROR_SUCCESS;
}



M_io_error_t M_io_serial_get_baud(M_io_t *io, M_io_serial_baud_t *baud)
{
    M_io_layer_t  *layer;
    M_io_handle_t *handle;

    layer        = M_io_layer_acquire(io, 0, M_IO_SERIAL_NAME);
    if (layer == NULL || baud == NULL)
        return M_IO_ERROR_INVALID;

    handle       = M_io_layer_get_handle(layer);

    *baud        = M_io_serial_handle_get_baud(handle);

    M_io_layer_release(layer);
    return M_IO_ERROR_SUCCESS;

}


static void M_io_serial_enum_free_port(void *arg)
{
    M_io_serial_enum_port_t *port = arg;
    M_free(port->path);
    M_free(port->name);
    M_free(port);
}


M_io_serial_enum_t *M_io_serial_enum_init(void)
{
    M_io_serial_enum_t *serenum = M_malloc_zero(sizeof(*serenum));
    struct M_list_callbacks listcbs = {
        NULL,
        NULL,
        NULL,
        M_io_serial_enum_free_port
    };
    serenum->ports = M_list_create(&listcbs, M_LIST_NONE);
    return serenum;
}


void M_io_serial_enum_add(M_io_serial_enum_t *serenum, const char *path, const char *name)
{
    M_io_serial_enum_port_t *port;

    if (serenum == NULL || M_str_isempty(path))
        return;

    /* If a display name is not available, use the path */
    if (M_str_isempty(name))
        name = path;

    port       = M_malloc_zero(sizeof(*port));
    port->name = M_strdup(name);
    port->path = M_strdup(path);
    M_list_insert(serenum->ports, port);
}


void M_io_serial_enum_destroy(M_io_serial_enum_t *serenum)
{
    if (serenum == NULL)
        return;
    M_list_destroy(serenum->ports, M_TRUE);
    M_free(serenum);
}


size_t M_io_serial_enum_count(const M_io_serial_enum_t *serenum)
{
    if (serenum == NULL)
        return 0;
    return M_list_len(serenum->ports);
}


const char *M_io_serial_enum_path(const M_io_serial_enum_t *serenum, size_t idx)
{
    const M_io_serial_enum_port_t *port;
    if (serenum == NULL)
        return NULL;
    port = M_list_at(serenum->ports, idx);
    if (port == NULL)
        return NULL;
    return port->path;
}


const char *M_io_serial_enum_name(const M_io_serial_enum_t *serenum, size_t idx)
{
    const M_io_serial_enum_port_t *port;
    if (serenum == NULL)
        return NULL;
    port = M_list_at(serenum->ports, idx);
    if (port == NULL)
        return NULL;
    return port->name;
}
