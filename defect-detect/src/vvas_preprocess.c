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

#define DEFAULT_MAX_VALUE	255
#define DEFAULT_STRIDE_VALUE	1920
#define NORMALIZE_THRESHOLD 13
#define MAX_HEIGHT 1080

typedef struct _kern_priv
{
    int threshold;
    int max_value;
    int stride_value;
    int log_level;
    int cnt;
    VVASFrame *mem, *tmp;
} PreProcessingKernelPriv;

int32_t xlnx_kernel_start(VVASKernel *handle, int start, VVASFrame *input[MAX_NUM_OBJECT], VVASFrame *output[MAX_NUM_OBJECT]);
int32_t xlnx_kernel_done(VVASKernel *handle);
int32_t xlnx_kernel_init(VVASKernel *handle);
uint32_t xlnx_kernel_deinit(VVASKernel *handle);

uint32_t xlnx_kernel_deinit(VVASKernel *handle)
{
    PreProcessingKernelPriv *kernel_priv;
    kernel_priv = (PreProcessingKernelPriv *)handle->kernel_priv;
    if (kernel_priv->mem)
        vvas_free_buffer (handle, kernel_priv->mem);
    if (kernel_priv->tmp)
        vvas_free_buffer (handle, kernel_priv->tmp);
    if (kernel_priv)
        free(kernel_priv);
    return 0;
}

int32_t xlnx_kernel_init(VVASKernel *handle)
{
    json_t *jconfig = handle->kernel_config;
    json_t *val; /* kernel config from app */
    PreProcessingKernelPriv *kernel_priv;
    uint32_t resolution;

    kernel_priv = (PreProcessingKernelPriv *)calloc(1, sizeof(PreProcessingKernelPriv));
    if (!kernel_priv) {
        printf("Error: Unable to allocate PPE kernel memory\n");
    }

    /* parse config */
    val = json_object_get (jconfig, "debug_level");
    if (!val || !json_is_integer (val))
	    kernel_priv->log_level = LOG_LEVEL_WARNING;
    else
	    kernel_priv->log_level = json_integer_value (val);
    LOG_MESSAGE (LOG_LEVEL_INFO, kernel_priv->log_level, "VVAS PPE: debug_level %d", kernel_priv->log_level);

    val = json_object_get (jconfig, "max_value");
    if (!val || !json_is_integer (val))
	    kernel_priv->max_value = DEFAULT_MAX_VALUE;
    else
	    kernel_priv->max_value = json_integer_value (val);
    LOG_MESSAGE (LOG_LEVEL_INFO, kernel_priv->log_level, "VVAS PPE: max_value %d", kernel_priv->max_value);
    val = json_object_get (jconfig, "stride_value");
    if (!val || !json_is_integer (val))
	    kernel_priv->stride_value = DEFAULT_STRIDE_VALUE;
    else
	    kernel_priv->stride_value = json_integer_value (val);
    LOG_MESSAGE (LOG_LEVEL_INFO, kernel_priv->log_level, "VVAS PPE: stride_value %d", kernel_priv->stride_value);

    kernel_priv->mem = vvas_alloc_buffer (handle, 1*(sizeof(uint32_t)), VVAS_INTERNAL_MEMORY, DEFAULT_MEM_BANK, NULL);

    resolution = kernel_priv->stride_value * MAX_HEIGHT;
    kernel_priv->tmp = vvas_alloc_buffer (handle, resolution*(sizeof(uint8_t)), VVAS_INTERNAL_MEMORY, DEFAULT_MEM_BANK, NULL);

    handle->kernel_priv = (void *)kernel_priv;
    handle->is_multiprocess = 1;

    return 0;
}

int32_t xlnx_kernel_start(VVASKernel *handle, int start, VVASFrame *input[MAX_NUM_OBJECT], VVASFrame *output[MAX_NUM_OBJECT])
{
    PreProcessingKernelPriv *kernel_priv;
    int ret;
    VVASFrame *inframe  = input[0];
    VVASFrame *outframe = output[0];
    kernel_priv = (PreProcessingKernelPriv *)handle->kernel_priv;
    uint64_t fwd;

    if (inframe->app_priv == NULL)
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "inframe app_priv is NULL");

    /* read meta from buffer */
    GstVvasUsrMeta *meta = (GstVvasUsrMeta *) gst_buffer_get_vvas_usr_meta ((GstBuffer *) inframe->app_priv);
    if (meta == NULL) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "Meta data is not available");
        return FALSE;
    }
    dd_meta *dmeta = (dd_meta *) meta->usr_data;

    LOG_MESSAGE(LOG_LEVEL_INFO, kernel_priv->log_level, "Frame count %d: Otsu Threshold %d", ++(kernel_priv->cnt), dmeta->threshold);

    /* prepare kernel args */
    kernel_priv->threshold = dmeta->threshold - NORMALIZE_THRESHOLD;

    /* Use temporary buffer if GRAY8 format is used, otherwise use second plane in case of NV16 */
    if (output[0]->n_planes == 1) {
        fwd = kernel_priv->tmp->paddr[0];
    } else if (output[0]->n_planes == 2) {
        fwd = output[0]->paddr[1];
    } else {
        LOG_MESSAGE(LOG_LEVEL_INFO, kernel_priv->log_level, "n_planes value %d not supported", output[0]->n_planes);
        return FALSE;
    }

    /* start kernel */
    LOG_MESSAGE (LOG_LEVEL_DEBUG, kernel_priv->log_level, "kernel start: img out %ld, fwd out %ld", output[0]->paddr[0], fwd);

    ret = vvas_kernel_start (handle, "ppppuuuuu", input[0]->paddr[0], output[0]->paddr[0], \
                             fwd, kernel_priv->mem->paddr[0], \
                             kernel_priv->threshold, kernel_priv->max_value, input[0]->props.height, \
                             input[0]->props.width, kernel_priv->stride_value);
    if (ret < 0) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "Failed to issue execute command");
        return FALSE;
    }

    /* wait for kernel completion */
    ret = vvas_kernel_done (handle, 1000);
    if (ret < 0) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, kernel_priv->log_level, "Failed to receive response from kernel");
        return FALSE;
    }

    LOG_MESSAGE (LOG_LEVEL_DEBUG, kernel_priv->log_level, "kernel done");

    /* retrieve meta from kernel */
    uint32_t *mango_pix = kernel_priv->mem->vaddr[0];
    dmeta = (dd_meta *) calloc(1, sizeof(dd_meta *));
    dmeta->mango_pixels = *mango_pix;

    LOG_MESSAGE(LOG_LEVEL_INFO, kernel_priv->log_level, "Frame count %d: Mango Pixels %d", kernel_priv->cnt, *mango_pix);

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

