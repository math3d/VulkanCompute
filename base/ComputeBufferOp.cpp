/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include "ComputeBufferOp.h"
#define USE_INPUT 1
#define USE_FILTER 1

ComputeBufferOp::ComputeBufferOp() {}

ComputeBufferOp::~ComputeBufferOp() {}

ComputeBufferOp::ComputeBufferOp(const InitParams &init_params)
    : ComputeOp(init_params) {}

void ComputeBufferOp::execute() {
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

    createBufferWithData(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &deviceBuffer_,
                         &deviceMemory_, bufferSize);

    copyHostBufferToDeviceBuffer(deviceBuffer_, hostBuffer_, bufferSize);
#if USE_READBACK_INPUT
    // Debug only.
    copyDeviceBufferToHostBuffer(deviceBuffer_, params_.computeInput.data(),
                                 bufferSize, params_.inputWidth,
                                 params_.inputHeight);
#endif
  }

  // Copy filter data to VRAM using a staging buffer.
  {
    createBufferWithData(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &filterHostBuffer_,
        &filterHostMemory_, filterBufferSize, params_.computeFilter.data());

    createBufferWithData(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &filterDeviceBuffer_,
        &filterDeviceMemory_, filterBufferSize);

    // Copy to staging buffer.
    copyHostBufferToDeviceBuffer(filterDeviceBuffer_, filterHostBuffer_,
                           filterBufferSize);
#if USE_READBACK_INPUT
    // Debug only.
    copyDeviceBufferToHostBuffer(filterDeviceBuffer_,
                                 params_.computeFilter.data(), filterBufferSize,
                                 params_.filterWidth, params_.filterHeight);
#endif
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
  prepareBufferToBufferPipeline(deviceBuffer_, filterDeviceBuffer_,
                                outputDeviceBuffer_);

  // Command buffer creation (for compute work submission).
  prepareCommandBuffer(outputDeviceBuffer_, outputHostBuffer_,
                       outputHostMemory_, outputBufferSize);
  copyDeviceBufferToHostBuffer(outputDeviceBuffer_,
                               params_.computeOutput.data(), outputBufferSize,
                               params_.outputWidth, params_.outputHeight);
  vkQueueWaitIdle(queue_);
}
