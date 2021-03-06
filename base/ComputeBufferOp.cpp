/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include "ComputeBufferOp.h"
#include "Utils.h"
#include <time.h>

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
    TIME("execute:createBufferWithData",
         createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &hostBuffer_,
                              &hostMemory_, bufferSize,
                              params_.computeInput.data()));

    TIME("execute:createBufferWithData",
         createBufferWithData(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              &deviceBuffer_, &deviceMemory_, bufferSize));

    TIME("execute:copyHostBufferToDeviceBuffer",
         copyHostBufferToDeviceBuffer(deviceBuffer_, hostBuffer_, bufferSize));
#ifdef USE_READBACK_INPUT
    // Debug only.
    copyDeviceBufferToHostBuffer(deviceBuffer_, params_.computeInput.data(),
                                 bufferSize, params_.inputWidth,
                                 params_.inputHeight);
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

    TIME("execute:createBufferWithData",
         createBufferWithData(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              &filterDeviceBuffer_, &filterDeviceMemory_,
                              filterBufferSize));

    // Copy to staging buffer.
    TIME("execute:copyHostBufferToDeviceBuffer",
         copyHostBufferToDeviceBuffer(filterDeviceBuffer_, filterHostBuffer_,
                                      filterBufferSize));
#ifdef USE_READBACK_INPUT
    // Debug only.
    copyDeviceBufferToHostBuffer(filterDeviceBuffer_,
                                 params_.computeFilter.data(), filterBufferSize,
                                 params_.filterWidth, params_.filterHeight);
#endif
  }

  {

    TIME("execute:createBufferWithData",
         createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                              &outputHostBuffer_, &outputHostMemory_,
                              outputBufferSize));

    TIME("execute:createBufferWithData",
         createBufferWithData(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              &outputDeviceBuffer_, &outputDeviceMemory_,
                              outputBufferSize));
  }

  // Prepare compute pipeline.
  TIME("execute:prepareBufferToBufferPipeline",
       prepareBufferToBufferPipeline(deviceBuffer_, filterDeviceBuffer_,
                                     outputDeviceBuffer_));

  // Command buffer creation (for compute work submission).
  TIME("execute:prepareCommandBuffer",
       prepareCommandBuffer(outputDeviceBuffer_, outputHostBuffer_,
                            outputHostMemory_, outputBufferSize));
  TIME("execute:copyDeviceBufferToHostBuffer",
       copyDeviceBufferToHostBuffer(
           outputDeviceBuffer_, params_.computeOutput.data(), outputBufferSize,
           params_.outputWidth, params_.outputHeight));
  vkQueueWaitIdle(queue_);
}
