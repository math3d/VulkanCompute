/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#define USE_TIME
#include "ComputeImageOp.h"

#ifdef USE_TIME
#include <time.h>
#include "Utils.h"
#else
#define TIME
#endif

#define USE_INPUT 1
#define USE_FILTER 1

ComputeImageOp::ComputeImageOp() {}

ComputeImageOp::~ComputeImageOp() {}

ComputeImageOp::ComputeImageOp(const InitParams &init_params)
    : ComputeOp(init_params) {}

void ComputeImageOp::execute() {
  // Prepare storage buffers.
  const VkDeviceSize bufferSize =
      (params_.inputWidth * params_.inputHeight) * sizeof(uint32_t);
  const VkDeviceSize filterBufferSize =
      (params_.filterWidth * params_.filterHeight) * sizeof(uint32_t);
  const VkDeviceSize outputBufferSize =
      (params_.outputWidth * params_.outputHeight) * sizeof(uint32_t);

  // Copy input data to VRAM using a staging buffer.
  {
    TIME("execute:createBufferWithData",
         createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &hostBuffer_,
                              &hostMemory_, bufferSize,
                              params_.computeInput.data()));

    TIME("execute:createDeviceImage",
         createDeviceImage(image_, params_.inputWidth, params_.inputHeight));
    TIME("execute:createSampler", createSampler(image_, sampler_, view_));

    TIME("execute:copyHostBufferToDeviceImage",
         copyHostBufferToDeviceImage(image_, hostBuffer_, params_.inputWidth,
                                     params_.inputHeight));
#ifdef USE_READBACK_INPUT
    // Debug only.
    copyDeviceImageToHostBuffer(image_, params_.computeInput.data(), bufferSize,
                                params_.inputWidth, params_.inputHeight);
#endif
  }
  // Copy filter data to VRAM using a staging buffer.
  {
    TIME("execute:createBufferWithData",
         createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                              &filterHostBuffer_, &filterHostMemory_,
                              filterBufferSize, params_.computeFilter.data()));

    TIME("execute:createDeviceImage",
         createDeviceImage(filterImage_, params_.filterWidth,
                           params_.filterHeight));
    TIME("execute:createSampler",
         createSampler(filterImage_, filterSampler_, filterView_));

    // Copy to staging buffer
    TIME("execute:copyHostBufferToDeviceImage",
         copyHostBufferToDeviceImage(filterImage_, filterHostBuffer_,
                                     params_.filterWidth,
                                     params_.filterHeight));
#ifdef USE_READBACK_INPUT
    // Debug only.
    copyDeviceImageToHostBuffer(filterImage_, params_.computeFilter.data(),
                                filterBufferSize, params_.filterWidth,
                                params_.filterHeight);
#endif
  }
  {
    TIME("execute:createTextureTarget",
         createTextureTarget(params_.outputWidth, params_.outputHeight,
                             imageFormat_));
  }
  // Prepare compute pipeline.
  TIME("execute:prepareImageToImagePipeline", prepareImageToImagePipeline());

  // Command buffer creation (for compute work submission).
  TIME("execute:prepareImageToImageCommandBuffer",
       prepareImageToImageCommandBuffer());
  TIME("execute:copyDeviceImageToHostBuffer",
       copyDeviceImageToHostBuffer(outputImage_, params_.computeOutput.data(),
                                   outputBufferSize, params_.outputWidth,
                                   params_.outputHeight));
  vkQueueWaitIdle(queue_);
}
