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


void::ComputeImageOp::execute() {
  // Prepare storage buffers.
  const VkDeviceSize bufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);
  const VkDeviceSize filterBufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);
  const VkDeviceSize outputBufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);

#ifdef USE_INPUT
  // Copy input data to VRAM using a staging buffer.
  printf("%s,%d Input:\n", __FUNCTION__, __LINE__);
  {
    createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &hostBuffer_,
                         &hostMemory_, bufferSize, params_.computeInput.data());

    createDeviceImage(image_);
    createSampler(image_, sampler_, view_);

    copyHostBufferToDeviceImage(image_, hostBuffer_);
    // Debug only.
    copyDeviceImageToHostBuffer(image_, bufferSize);
  }
#endif
#ifdef USE_FILTER
  // Copy filter data to VRAM using a staging buffer.
  {
    createBufferWithData(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &filterHostBuffer_,
        &filterHostMemory_, filterBufferSize, params_.computeFilter.data());

    createDeviceImage(filterImage_);
    createSampler(filterImage_, filterSampler_, filterView_);

    // Copy to staging buffer
    copyHostBufferToDeviceImage(filterImage_, filterHostBuffer_);
    // Debug only.
    copyDeviceImageToHostBuffer(filterImage_, filterBufferSize);
  }
#endif
  {
    prepareTextureTarget(width, height, imageFormat_);
  }

  // Prepare compute pipeline.
  prepareImageToImagePipeline();

  // Command buffer creation (for compute work submission).
  prepareComputeImageToImageCommandBuffer();
  printf("%s,%d; Output: \n", __FUNCTION__, __LINE__);
  copyDeviceImageToHostBuffer(outputImage_, outputBufferSize);

  vkQueueWaitIdle(queue_);
}
