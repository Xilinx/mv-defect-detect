/*
 * Copyright 2022, Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vvas/vvaslogs.h>
#include <vvas/vvas_kernel.h>
#include <gst/vvas/gstvvasusrmeta.h>
#include "vvas_dd_meta.h"

#define DEFAULT_STRIDE_VALUE        1920
#define MAX_SUPPORTED_WIDTH         1920
#define MAX_SUPPORTED_HEIGHT        1080

typedef struct _kern_priv
{
    int log_level;
    int stride_value;
    int cnt;
    VVASFrame *mem;
    VVASFrame *mango_pix;
    VVASFrame *defect_pix;
} KernelPriv;

int32_t xlnx_kernel_start(VVASKernel *handle, int start, VVASFrame *input[MAX_NUM_OBJECT], VVASFrame *output[MAX_NUM_OBJECT]);
int32_t xlnx_kernel_done(VVASKernel *handle);
int32_t xlnx_kernel_init(VVASKernel *handle);
uint32_t xlnx_kernel_deinit(VVASKernel *handle);

uint32_t xlnx_kernel_deinit(VVASKernel *handle)
{
    KernelPriv *kernel_priv;
    kernel_priv = (KernelPriv *)handle->kernel_priv;
    if (kernel_priv->mango_pix)
        vvas_free_buffer (handle, kernel_priv->mango_pix);
    if (kernel_priv->defect_pix)
        vvas_free_buffer (handle, kernel_priv->defect_pix);
    if (kernel_priv->mem)
        vvas_free_buffer (handle, kernel_priv->mem);
    free(kernel_priv);
    return 0;
}

int32_t xlnx_kernel_init(VVASKernel *handle)
{
    json_t *jconfig = handle->kernel_config;
    json_t *val; /* kernel config from app */
    KernelPriv *kernel_priv;

    kernel_priv = (KernelPriv *)calloc(1, sizeof(KernelPriv));
    if (!kernel_priv) {
        printf("Error: Unable to allocate PPE kernel memory\n");
    }
    uint32_t resolution = MAX_SUPPORTED_HEIGHT * MAX_SUPPORTED_WIDTH;
    kernel_priv->mango_pix  = vvas_alloc_buffer (handle, 1*(sizeof(uint32_t)), VVAS_INTERNAL_MEMORY, DEFAULT_MEM_BANK, NULL);
    kernel_priv->defect_pix = vvas_alloc_buffer (handle, 1*(sizeof(uint32_t)), VVAS_INTERNAL_MEMORY, DEFAULT_MEM_BANK, NULL);
    kernel_priv->mem   = vvas_alloc_buffer (handle, 1*(sizeof(uint32_t)), VVAS_INTERNAL_MEMORY, DEFAULT_MEM_BANK, NULL);
    /* parse config */
    val = json_object_get (jconfig, "debug_level");
    if (!val || !json_is_integer (val))
	    kernel_priv->log_level = LOG_LEVEL_WARNING;
    else
	    kernel_priv->log_level = json_integer_value (val);
    LOG_MESSAGE (LOG_LEVEL_INFO, kernel_priv->log_level, "VVAS PPE: debug_level %d", kernel_priv->log_level);

    val = json_object_get (jconfig, "stride_value");
    if (!val || !json_is_integer (val))
            kernel_priv->stride_value = DEFAULT_STRIDE_VALUE;
    else
            kernel_priv->stride_value = json_integer_value (val);
    LOG_MESSAGE (LOG_LEVEL_INFO, kernel_priv->log_level, "VVAS PPE: stride_value %d", kernel_priv->stride_value);

    handle->kernel_priv = (void *)kernel_priv;
    handle->is_multiprocess = 1;
    return 0;
}

int32_t xlnx_kernel_start(VVASKernel *handle, int start, VVASFrame *input[MAX_NUM_OBJECT], VVASFrame *output[MAX_NUM_OBJECT])
{
    KernelPriv *kernel_priv;
    int ret;
    VVASFrame *outframe = output[0];
    VVASFrame *inframe = input[0];
    kernel_priv = (KernelPriv *)handle->kernel_priv;

    if (inframe->app_priv == NULL)
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "inframe app_priv is NULL");

    /* read meta from buffer */
    GstVvasUsrMeta *meta = (GstVvasUsrMeta *) gst_buffer_get_vvas_usr_meta ((GstBuffer *) inframe->app_priv);
    if (meta == NULL) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "Meta data is not available");
        return FALSE;
    }
    dd_meta *dmeta = (dd_meta *) meta->usr_data;
    uint32_t mango_pixel = dmeta->mango_pixels;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, kernel_priv->log_level, "Frame Count %d: Mango Pixels %d", ++(kernel_priv->cnt), mango_pixel);

    /* start kernel */
    LOG_MESSAGE (LOG_LEVEL_DEBUG, kernel_priv->log_level, "kernel start: img in %ld, fwd in %ld", input[0]->paddr[0], input[0]->paddr[1]);

    ret = vvas_kernel_start (handle, "ppppuuu", input[0]->paddr[0], input[0]->paddr[1], \
                             output[0]->paddr[0], kernel_priv->mem->paddr[0], \
                             input[0]->props.height, output[0]->props.width, \
                             kernel_priv->stride_value);
    if (ret < 0) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "Failed to issue execute command");
        return FALSE;
    }

    LOG_MESSAGE (LOG_LEVEL_DEBUG, kernel_priv->log_level, "kernel done");

    /* wait for kernel completion */
    ret = vvas_kernel_done (handle, 1000);
    if (ret < 0) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "Failed to receive response from kernel");
        return FALSE;
    }

    /* retrieve meta from kernel */
    uint32_t *defect_pixel = kernel_priv->mem->vaddr[0];
    dmeta = (dd_meta *) calloc(1, sizeof(dd_meta *));
    dmeta->defect_pixels = *defect_pixel;
    dmeta->mango_pixels = mango_pixel;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, kernel_priv->log_level, "Frame Count %d: Defect Pixels %d", kernel_priv->cnt, *defect_pixel);

    if (outframe->app_priv == NULL)
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "outframe app_priv is NULL");

    /* write meta to buffer */
    meta = (GstVvasUsrMeta *) gst_buffer_add_vvas_usr_meta ((GstBuffer *) outframe->app_priv, dd_meta_copy, dd_meta_release, (void *) dmeta);
    if (meta == NULL) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "Meta data is not available");
        return FALSE;
    }

    return TRUE;
}

int32_t xlnx_kernel_done(VVASKernel *handle)
{
    /* dummy */
    return 0;
}

