/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include "ComputeImageOp.h"
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
    createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &hostBuffer_,
                         &hostMemory_, bufferSize, params_.computeInput.data());

    createDeviceImage(image_, params_.inputWidth, params_.inputHeight);
    createSampler(image_, sampler_, view_);

    copyHostBufferToDeviceImage(image_, hostBuffer_, params_.inputWidth,
                                params_.inputHeight);
#if USE_READBACK_INPUT
    // Debug only.
    copyDeviceImageToHostBuffer(image_, params_.computeInput.data(), bufferSize,
                                params_.inputWidth, params_.inputHeight);
#endif
  }
  // Copy filter data to VRAM using a staging buffer.
  {
    createBufferWithData(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &filterHostBuffer_,
        &filterHostMemory_, filterBufferSize, params_.computeFilter.data());

    createDeviceImage(filterImage_, params_.filterWidth, params_.filterHeight);
    createSampler(filterImage_, filterSampler_, filterView_);

    // Copy to staging buffer
    copyHostBufferToDeviceImage(filterImage_, filterHostBuffer_,
                                params_.filterWidth, params_.filterHeight);
#if USE_READBACK_INPUT
    // Debug only.
    copyDeviceImageToHostBuffer(filterImage_, params_.computeFilter.data(),
                                filterBufferSize, params_.filterWidth,
                                params_.filterHeight);
#endif
  }
  {
    createTextureTarget(params_.outputWidth, params_.outputHeight,
                        imageFormat_);
  }
  // Prepare compute pipeline.
  prepareImageToImagePipeline();

  // Command buffer creation (for compute work submission).
  prepareImageToImageCommandBuffer();
  copyDeviceImageToHostBuffer(outputImage_, params_.computeOutput.data(),
                              outputBufferSize, params_.outputWidth,
                              params_.outputHeight);
  vkQueueWaitIdle(queue_);
}
