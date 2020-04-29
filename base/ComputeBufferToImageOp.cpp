/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include "ComputeBufferToImageOp.h"

ComputeBufferToImageOp::ComputeBufferToImageOp() {}

ComputeBufferToImageOp::~ComputeBufferToImageOp() {}

ComputeBufferToImageOp::ComputeBufferToImageOp(const InitParams &init_params)
    : ComputeOp(init_params) {}


void ComputeBufferToImageOp::execute() {
  // Prepare storage buffers.
  const VkDeviceSize bufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);
  const VkDeviceSize filterBufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);
  const VkDeviceSize outputBufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);

  // Copy input data to VRAM using a staging buffer.
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

  // Copy filter data to VRAM using a staging buffer.
  {
    createBufferWithData(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &filterHostBuffer_,
        &filterHostMemory_, bufferSize, params_.computeFilter.data());

    createDeviceImage(filterImage_);
    createSampler(filterImage_, filterSampler_, filterView_);

    // Copy to staging buffer
    copyHostBufferToDeviceImage(filterImage_, filterHostBuffer_);
    // Debug only.
    copyDeviceImageToHostBuffer(filterImage_, bufferSize);
  }

  {

    createBufferWithData(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &outputHostBuffer_,
        &outputHostMemory_, outputBufferSize);

    createBufferWithData(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &outputDeviceBuffer_,
        &outputDeviceMemory_, outputBufferSize);
  }

  // Prepare compute pipeline.
  prepareImageToBufferPipeline(deviceBuffer_, filterDeviceBuffer_,
                               outputDeviceBuffer_);

  // Command buffer creation (for compute work submission).
  prepareComputeCommandBuffer(outputDeviceBuffer_, outputHostBuffer_,
                              outputHostMemory_, outputBufferSize);

  vkQueueWaitIdle(queue_);

  // Output buffer contents.
  // summary();
}
