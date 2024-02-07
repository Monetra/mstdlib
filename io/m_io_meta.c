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
#include "m_io_meta.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
    const char                     *layer_name;
    size_t                          layer_idx;
    void                           *data;
    M_io_meta_layer_data_destroy_t  data_destroy;
} M_io_meta_data_t;

struct M_io_meta {
    M_list_t *metas;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_io_meta_data_t *M_io_meta_data_create(const char *layer_name, size_t layer_idx, void *data, M_io_meta_layer_data_destroy_t data_destroy)
{
    M_io_meta_data_t *mdata;

    if (data == NULL)
        return NULL;

    mdata               = M_malloc(sizeof(*mdata));
    mdata->layer_name   = layer_name;
    mdata->layer_idx    = layer_idx;
    mdata->data         = data;
    mdata->data_destroy = data_destroy;

    return mdata;
}

static void M_io_meta_data_destroy(void *p)
{
    M_io_meta_data_t *mdata = p;

    if (p == NULL)
        return;

    mdata->data_destroy(mdata->data);
    M_free(mdata);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_io_meta_insert_layer_data_idx(M_io_meta_t *meta, const char *layer_name, size_t layer_idx, void *data, M_io_meta_layer_data_destroy_t data_destroy)
{
    M_io_meta_data_t *mdata;

    if (meta == NULL || M_str_isempty(layer_name) || data == NULL)
        return;

    mdata = M_io_meta_data_create(layer_name, layer_idx, data, data_destroy);
    M_list_insert(meta->metas, mdata);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_io_meta_insert_layer_data(M_io_meta_t *meta, M_io_layer_t *layer, void *data, M_io_meta_layer_data_destroy_t data_destroy)
{
    if (meta == NULL || layer == NULL || data == NULL)
        return;

    M_io_meta_insert_layer_data_idx(meta, M_io_layer_get_name(layer), M_io_layer_get_index(layer), data, data_destroy);
}

void *M_io_meta_get_layer_data(M_io_meta_t *meta, M_io_layer_t *layer)
{
    void                   *data     = NULL;
    const M_io_meta_data_t *mdata;
    const char             *layer_name;
    M_io_t                 *io;
    const char             *const_temp;
    size_t                  layer_idx;
    size_t                  i;
    size_t                  len;
    M_bool                  is_first = M_TRUE;

    if (meta == NULL || layer == NULL)
        return NULL;

    layer_name = M_io_layer_get_name(layer);
    layer_idx  = M_io_layer_get_index(layer);

    /* Determine if this is the first layer */
    io  = M_io_layer_get_io(layer);
    len = M_io_layer_count(io);
    for (i=len; i-->len; ) {
        const_temp = M_io_layer_name(io, i);
        if (M_str_eq(layer_name, const_temp)) {
            if (i != layer_idx) {
                is_first = M_FALSE;
            }
            break;
        }
    }

    /* Get the layer's meta object if it exists. */
    len = M_list_len(meta->metas);
    for (i=0; i<len; i++) {
        mdata = M_list_at(meta->metas, i);

        if (M_str_eq(mdata->layer_name, layer_name)) {
            if ((mdata->layer_idx == M_IO_LAYER_FIND_FIRST_ID && is_first) ||
                mdata->layer_idx == layer_idx)
            {
                data = mdata->data;
                break;
            }
        }
    }

    return data;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_meta_t *M_io_meta_create(void)
{
    M_io_meta_t *meta;
    struct M_list_callbacks cbs = {
        NULL,
        NULL,
        NULL,
        M_io_meta_data_destroy
    };

    meta        = M_malloc(sizeof(*meta));
    meta->metas = M_list_create(&cbs, M_LIST_NONE);

    return meta;
}

void M_io_meta_destroy(M_io_meta_t *meta)
{
    if (meta == NULL)
        return;
    M_list_destroy(meta->metas, M_TRUE);
    M_free(meta);
}
