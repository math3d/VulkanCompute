/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include "ComputeImageOp.h"
ComputeImageOp::ComputeImageOp() {}

ComputeImageOp::~ComputeImageOp() {}

ComputeImageOp::ComputeImageOp(const InitParams &init_params)
    : ComputeOp(init_params) {}

void ComputeImageOp::execute() {
  // Prepare storage buffers.
  const VkDeviceSize bufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);
  const VkDeviceSize filterBufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);
  const VkDeviceSize outputBufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);

  // Copy input data to VRAM using a staging buffer.
  {
    createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &hostBuffer,
                         &hostMemory, bufferSize, params_.computeInput.data());

    createImage(image_);
    createSampler(image_, sampler_, view_);

    copyHostBufferToDeviceImage(image_, deviceMemory, hostBuffer, hostMemory);
  }

  // Copy filter data to VRAM using a staging buffer.
  {
    createBufferWithData(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &filterHostBuffer,
        &filterHostMemory, bufferSize, params_.computeFilter.data());

    createImage(filterImage_);
    createSampler(filterImage_, filterSampler_, filterView_);

    // Copy to staging buffer
    copyHostBufferToDeviceImage(filterImage_, filterDeviceMemory, filterHostBuffer,
                                filterHostMemory);
  }

  {

    createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &outputHostBuffer,
                         &outputHostMemory, outputBufferSize);

    createBufferWithData(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &outputDeviceBuffer,
        &outputDeviceMemory, outputBufferSize);
  }

  // Prepare compute pipeline.
  prepareImagePipeline(deviceBuffer, filterDeviceBuffer, outputDeviceBuffer);

  // Command buffer creation (for compute work submission).
  prepareComputeCommandBuffer(outputDeviceBuffer, outputHostBuffer,
                              outputHostMemory, outputBufferSize);

  vkQueueWaitIdle(queue);

  // Output buffer contents.
  // summary();
}
