/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include "ComputeCopyImageOp.h"
ComputeCopyImageOp::ComputeCopyImageOp() {}

ComputeCopyImageOp::~ComputeCopyImageOp() {}

ComputeCopyImageOp::ComputeCopyImageOp(const InitParams &init_params)
    : ComputeOp(init_params) {}

void ComputeCopyImageOp::execute() {
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

    copyHostBufferToDeviceImage(image_, hostBuffer);
	VkDeviceMemory testMemory;
	VkBuffer testBuffer;
    createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &testBuffer,
                         &testMemory, bufferSize);

	copyDeviceImageToHostBuffer(testBuffer, image_);
  }


  vkQueueWaitIdle(queue);

  //TODO: exit clean up.
}
