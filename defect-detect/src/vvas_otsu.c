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

struct bgr2y8_params {
    int32_t black_Vmax;
    int32_t black_Smax;
    int32_t brown_Hmax;
    int32_t brown_Vmax;
    int32_t Smin;
    int32_t Smax;
    int32_t darkgreen_Vmax;
    int32_t darkgreen_Hmin;
    int32_t darkgreen_Hmax;
    int32_t green_Hmax;
    int32_t green_Hmin;
    int32_t green_Vmax;
};

typedef struct _kern_priv
{
    int log_level;
    int cnt;
    VVASFrame *otsuval;
    VVASFrame *array_params;
} OtsuKernelPriv;

int32_t  xlnx_kernel_start(VVASKernel *handle, int start, VVASFrame *input[MAX_NUM_OBJECT], VVASFrame *output[MAX_NUM_OBJECT]);
int32_t  xlnx_kernel_done(VVASKernel *handle);
int32_t  xlnx_kernel_init(VVASKernel *handle);
uint32_t xlnx_kernel_deinit(VVASKernel *handle);

uint32_t xlnx_kernel_deinit(VVASKernel *handle)
{
    OtsuKernelPriv *kernel_priv;
    kernel_priv = (OtsuKernelPriv *)handle->kernel_priv;
    if (kernel_priv->otsuval)
        vvas_free_buffer (handle, kernel_priv->otsuval);
    if (kernel_priv->array_params)
        vvas_free_buffer (handle, kernel_priv->array_params);
    free(kernel_priv);
    return 0;
}

int32_t xlnx_kernel_init(VVASKernel *handle)
{
    json_t *jconfig = handle->kernel_config;
    json_t *val; /* kernel config from app */
    OtsuKernelPriv *kernel_priv;
    struct bgr2y8_params def_params = {
        .black_Vmax = 20,
        .black_Smax = 70,
        .brown_Hmax = 15,
        .brown_Vmax = 40,
        .Smin = 60,
        .Smax = 90,
        .darkgreen_Vmax = 35,
        .darkgreen_Hmin = 30,
        .darkgreen_Hmax = 45,
        .green_Hmax = 90,
        .green_Hmin = 50,
        .green_Vmax = 45
    };

    kernel_priv = (OtsuKernelPriv *)calloc(1, sizeof(OtsuKernelPriv));
    if (!kernel_priv) {
        printf("Error: Unable to allocate PPE kernel memory\n");
    }
    kernel_priv->otsuval = vvas_alloc_buffer (handle, 1*(sizeof(uint32_t)), VVAS_INTERNAL_MEMORY, DEFAULT_MEM_BANK, NULL);
    kernel_priv->array_params = vvas_alloc_buffer (handle, sizeof(def_params), VVAS_INTERNAL_MEMORY, DEFAULT_MEM_BANK, NULL);
    memcpy(kernel_priv->array_params->vaddr[0], &def_params, sizeof(def_params));

    /* parse config */
    val = json_object_get (jconfig, "debug_level");
    if (!val || !json_is_integer (val))
	    kernel_priv->log_level = LOG_LEVEL_WARNING;
    else
	    kernel_priv->log_level = json_integer_value (val);
    LOG_MESSAGE (LOG_LEVEL_INFO, kernel_priv->log_level, "VVAS PPE: debug_level %d", kernel_priv->log_level);

    handle->kernel_priv = (void *)kernel_priv;
    handle->is_multiprocess = 1;
    return 0;
}

int32_t xlnx_kernel_start(VVASKernel *handle, int start, VVASFrame *input[MAX_NUM_OBJECT], VVASFrame *output[MAX_NUM_OBJECT])
{
    OtsuKernelPriv *kernel_priv;
    int ret;
    uint32_t *thr;
    float sigma = 0.0;
    GstVvasUsrMeta *meta = NULL;
    dd_meta *dmeta = NULL;
    VVASFrame *outframe = output[0];
    kernel_priv = (OtsuKernelPriv *)handle->kernel_priv;

    /* start kernel */
    ret = vvas_kernel_start (handle, "ppuufpp", input[0]->paddr[0], \
                             output[0]->paddr[0], input[0]->props.height, input[0]->props.width, sigma, \
                             kernel_priv->otsuval->paddr[0], kernel_priv->array_params->paddr[0]);
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

    /* retrieve dd_meta from kernel */
    thr =  kernel_priv->otsuval->vaddr[0];
    dmeta = (dd_meta *) calloc(1, sizeof(dd_meta *));
    dmeta->threshold = *thr;

    LOG_MESSAGE (LOG_LEVEL_INFO, kernel_priv->log_level, "Frame count %d: Otsu threshold %d", ++(kernel_priv->cnt) , *thr);

    /* add meta to buffer */
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
