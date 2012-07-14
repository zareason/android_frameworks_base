/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <hardware/hwcomposer.h>
#include <utils/Errors.h>

#include "Layer.h"
#include "SurfaceTextureLayer.h"

namespace android {
// ---------------------------------------------------------------------------


SurfaceTextureLayer::SurfaceTextureLayer(GLuint tex, const sp<Layer>& layer)
    : SurfaceTexture(tex, true, GL_TEXTURE_EXTERNAL_OES, false), mLayer(layer) {
#ifdef ALLWINNER_HARDWARE
    usehwcomposer = false;
    usehwinit     = false;
#endif
}

SurfaceTextureLayer::~SurfaceTextureLayer() {
}


status_t SurfaceTextureLayer::setDefaultBufferSize(uint32_t w, uint32_t h)
{
    //LOGD("%s, w=%u, h=%u", __PRETTY_FUNCTION__, w, h);
    return SurfaceTexture::setDefaultBufferSize(w, h);
}

status_t SurfaceTextureLayer::setDefaultBufferFormat(uint32_t format)
{
    mDefaultFormat = format;
    return NO_ERROR;
}

status_t SurfaceTextureLayer::setBufferCount(int bufferCount) {
    status_t res = SurfaceTexture::setBufferCount(bufferCount);
    return res;
}

#ifdef QCOM_HARDWARE
int SurfaceTextureLayer::query(int what, int* value) {
    int ret = SurfaceTexture::query(what, value);

    sp<Layer> layer(mLayer.promote());
    if (layer == NULL) return NO_INIT;

    switch (what) {
    case NATIVE_WINDOW_TRANSFORM_HINT:
        *value = layer->getTransformHint();
        ret = NO_ERROR;
        break;
    default:
        // for later use
        break;
    }

    return ret;
}
#endif

status_t SurfaceTextureLayer::queueBuffer(int buf, int64_t timestamp,
        uint32_t* outWidth, uint32_t* outHeight, uint32_t* outTransform) {

    status_t res = SurfaceTexture::queueBuffer(buf, timestamp,
            outWidth, outHeight, outTransform);
    sp<Layer> layer(mLayer.promote());
    if (layer != NULL) {
        *outTransform = layer->getTransformHint();
    }
    return res;
}

status_t SurfaceTextureLayer::dequeueBuffer(int *buf,
        uint32_t w, uint32_t h, uint32_t format, uint32_t usage) {

    status_t res(NO_INIT);
    sp<Layer> layer(mLayer.promote());
    if (layer != NULL) {
        if (format == 0)
            format = mDefaultFormat;
        uint32_t effectiveUsage = layer->getEffectiveUsage(usage);
        //LOGD("%s, w=%u, h=%u, format=%u, usage=%08x, effectiveUsage=%08x",
        //        __PRETTY_FUNCTION__, w, h, format, usage, effectiveUsage);
        res = SurfaceTexture::dequeueBuffer(buf, w, h, format, effectiveUsage);
    }
    return res;
}

status_t SurfaceTextureLayer::connect(int api,
        uint32_t* outWidth, uint32_t* outHeight, uint32_t* outTransform) {
    status_t err = SurfaceTexture::connect(api,
            outWidth, outHeight, outTransform);
    if (err == NO_ERROR) {
        sp<Layer> layer(mLayer.promote());
        if (layer != NULL) {
            uint32_t orientation = layer->getOrientation();
            if (orientation & Transform::ROT_INVALID) {
                orientation = 0;
            }
            *outTransform = orientation;
        }
        switch(api) {
#ifdef ALLWINNER_HARDWARE
            case NATIVE_WINDOW_API_MEDIA_HW:
            case NATIVE_WINDOW_API_CAMERA_HW:
                usehwcomposer = true;
                break;
#endif
            case NATIVE_WINDOW_API_MEDIA:
            case NATIVE_WINDOW_API_CAMERA:
                // Camera preview and videos are rate-limited on the producer
                // side.  If enabled for this build, we use async mode to always
                // show the most recent frame at the cost of requiring an
                // additional buffer.
#ifndef NEVER_DEFAULT_TO_ASYNC_MODE
                err = setSynchronousMode(false);
                break;
#endif
                // fall through to set synchronous mode when not defaulting to
                // async mode.
            default:
                err = setSynchronousMode(true);
                break;
        }
        if (err != NO_ERROR) {
            disconnect(api);
        }
    }
    return err;
}

#ifdef ALLWINNER_HARDWARE
status_t SurfaceTextureLayer::disconnect(int api)
{
    status_t err = SurfaceTexture::disconnect(api);

    switch (api)
    {
        case NATIVE_WINDOW_API_MEDIA_HW:
        case NATIVE_WINDOW_API_CAMERA_HW:
        {
            sp<Layer> layer(mLayer.promote());
            usehwcomposer = false;
            usehwinit     = false;
            if (layer != NULL)
                layer->setTextureInfo(0,0,0);
        }
        default:
            break;
    }
    return err;
}

int SurfaceTextureLayer::setParameter(uint32_t cmd,uint32_t value)
{
    int res = 0;

    SurfaceTexture::setParameter(cmd,value);

    sp<Layer> layer(mLayer.promote());
    if (layer != NULL)
    {
        if(usehwcomposer)
        {
            if(cmd == HWC_LAYER_SETINITPARA)
            {
                layerinitpara_t  *layer_info;
                layer_info = (layerinitpara_t  *)value;

                if(IsHardwareRenderSupport())
                {
                    layer->setTextureInfo(layer_info->w,layer_info->h,layer_info->format);
                    usehwinit = true;
                }
            }

            if(usehwinit == true)
            {
                res = layer->setDisplayParameter(cmd,value);
            }
        }
    }

    return res;
}


uint32_t SurfaceTextureLayer::getParameter(uint32_t cmd)
{
    uint32_t res = 0;

    sp<Layer> layer(mLayer.promote());
    if (layer != NULL)
        res = layer->getDisplayParameter(cmd);

    return res;
}
#endif
// ---------------------------------------------------------------------------
}; // namespace android
