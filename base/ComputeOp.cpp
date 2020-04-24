/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#if defined(_WIN32)
#pragma comment(linker, "/subsystem:console")
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
#include "VulkanAndroid.h"
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#endif

#include <algorithm>
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "ComputeOp.h"
#include "VulkanTools.h"
#include <vulkan/vulkan.h>

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
android_app *androidapp;
#endif

#define DEBUG (!NDEBUG)

#define BUFFER_ELEMENTS 32
const int BUFFER_NUMBER = 3;
const int width = 4;
const int height = 8;
const int mipLevels = 1;


ComputeOp::InitParams::InitParams() = default;

ComputeOp::InitParams::InitParams(const InitParams &other) = default;

VkResult ComputeOp::createBufferWithData(
    VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
    VkBuffer *buffer, VkDeviceMemory *memory, VkDeviceSize size, void *data) {
  // Create the buffer handle
  VkBufferCreateInfo bufferCreateInfo =
      vks::initializers::bufferCreateInfo(usageFlags, size);
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer));

  // Create the memory backing up the buffer handle
  VkMemoryRequirements memReqs;
  VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
  vkGetBufferMemoryRequirements(device, *buffer, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  // Find a memory type index that fits the properties of the buffer
  bool memTypeFound = false;
  for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
    if ((memReqs.memoryTypeBits & 1) == 1) {
      if ((deviceMemoryProperties.memoryTypes[i].propertyFlags &
           memoryPropertyFlags) == memoryPropertyFlags) {
        memAlloc.memoryTypeIndex = i;
        memTypeFound = true;
      }
    }
    memReqs.memoryTypeBits >>= 1;
  }
  assert(memTypeFound);
  VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, memory));

  if (data != nullptr) {
    void *mapped;
    VK_CHECK_RESULT(vkMapMemory(device, *memory, 0, size, 0, &mapped));
    memcpy(mapped, data, size);
  if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
    // Flush writes to host visible buffer
    void *mapped;
    //vkMapMemory(device, *memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
    mappedRange.memory = *memory;
    mappedRange.offset = 0;
    mappedRange.size = VK_WHOLE_SIZE;
    vkFlushMappedMemoryRanges(device, 1, &mappedRange);
    //vkUnmapMemory(device, *memory);
  }
    vkUnmapMemory(device, *memory);
  }

  VK_CHECK_RESULT(vkBindBufferMemory(device, *buffer, *memory, 0));

  return VK_SUCCESS;
}

VkResult ComputeOp::copyBufferHostToDevice(VkBuffer &deviceBuffer,
                                           VkBuffer &hostBuffer,
                                           const VkDeviceSize &bufferSize) {
  // Copy to staging buffer.
  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
          commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
  VkCommandBuffer copyCmd;
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &copyCmd));
  VkCommandBufferBeginInfo cmdBufInfo =
      vks::initializers::commandBufferBeginInfo();
  VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

  VkBufferCopy copyRegion = {};
  copyRegion.size = bufferSize;
  vkCmdCopyBuffer(copyCmd, hostBuffer, deviceBuffer, 1, &copyRegion);
  VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

  VkSubmitInfo submitInfo = vks::initializers::submitInfo();
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &copyCmd;
  VkFenceCreateInfo fenceInfo =
      vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
  VkFence fence;
  VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));

  // Submit to the queue.
  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
  VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

  vkDestroyFence(device, fence, nullptr);
  vkFreeCommandBuffers(device, commandPool, 1, &copyCmd);
  return VK_SUCCESS;
}

inline VkImageCreateInfo initImageCreateInfo() {
  VkImageCreateInfo imageCreateInfo{};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  return imageCreateInfo;
}

inline VkMemoryAllocateInfo memoryAllocateInfo() {
  VkMemoryAllocateInfo memAllocInfo{};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  return memAllocInfo;
}

void flushCommandBuffer(VkDevice device, VkCommandPool commandPool,
                        VkCommandBuffer commandBuffer, VkQueue queue,
                        bool free) {
  if (commandBuffer == VK_NULL_HANDLE) {
    return;
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  VK_CHECK_RESULT(vkQueueWaitIdle(queue));

  if (free) {
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
  }
}

VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool,
                                    VkCommandBufferLevel level, bool begin) {
  VkCommandBuffer cmdBuffer;

  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(commandPool, level, 1);

  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

  // If requested, also start the new command buffer
  if (begin) {
    VkCommandBufferBeginInfo cmdBufInfo =
        vks::initializers::commandBufferBeginInfo();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
  }

  return cmdBuffer;
}

VkResult ComputeOp::createImage(VkImage &image) {
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  // Create optimal tiled target image on the device
  VkImageCreateInfo imageCreateInfo = initImageCreateInfo();
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.format = format;
  imageCreateInfo.mipLevels = mipLevels;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  // Set initial layout of the image to undefined
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateInfo.extent = {width, height, 1};
  imageCreateInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
  VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &image));
  VkMemoryRequirements memReqs = {};
  VkMemoryAllocateInfo memAllocInfo = memoryAllocateInfo();
  vkGetImageMemoryRequirements(device, image, &memReqs);
  memAllocInfo.allocationSize = memReqs.size;
  memAllocInfo.memoryTypeIndex = getMemoryType(
      memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(
      vkAllocateMemory(device, &memAllocInfo, nullptr, &deviceMemory));
  VK_CHECK_RESULT(vkBindImageMemory(device, image, deviceMemory, 0));
  return VK_SUCCESS;
}

inline VkSamplerCreateInfo samplerCreateInfo() {
  VkSamplerCreateInfo samplerCreateInfo{};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.maxAnisotropy = 1.0f;
  return samplerCreateInfo;
}

VkResult ComputeOp::createSampler(VkImage &image, VkSampler &sampler, VkImageView &view) {
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  // Create a texture sampler
  // In Vulkan textures are accessed by samplers
  // This separates all the sampling information from the texture data. This
  // means you could have multiple sampler objects for the same texture with
  // different settings Note: Similar to the samplers available with
  // OpenGL 3.3
  VkSamplerCreateInfo samplerInfo = samplerCreateInfo();
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerInfo.minLod = 0.0f;
  // Set max level-of-detail to mip level count of the texture
  samplerInfo.maxLod = mipLevels;
  // Enable anisotropic filtering
  // This feature is optional, so we must check if it's supported on the
  // device
  /* TODO
  if (vulkanDevice->features.samplerAnisotropy) {
    // Use max. level of anisotropy for this example
    sampler.maxAnisotropy =
        vulkanDevice->properties.limits.maxSamplerAnisotropy;
    sampler.anisotropyEnable = VK_TRUE;
  } else
  */
  {
    // The device does not support anisotropic filtering
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.anisotropyEnable = VK_FALSE;
  }
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));

  // Create image view
  // Textures are not directly accessed by the shaders and
  // are abstracted by image views containing additional
  // information and sub resource ranges
  VkImageViewCreateInfo viewInfo = vks::initializers::imageViewCreateInfo();
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                         VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  // The subresource range describes the set of mip levels (and array layers)
  // that can be accessed through this image view It's possible to create
  // multiple image views for a single image referring to different (and/or
  // overlapping) ranges of the image
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;
  // Linear tiling usually won't support mip maps
  // Only set mip map count if optimal tiling is used
  viewInfo.subresourceRange.levelCount = mipLevels; // : 1;
  // The view will be based on the texture's image
  viewInfo.image = image;
  VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &view));
  return VK_SUCCESS;
}

VkResult ComputeOp::copyHostBufferToDeviceImage(VkImage &image,
                                                VkBuffer &stagingBuffer) {

  // Setup buffer copy regions for each mip level
  std::vector<VkBufferImageCopy> bufferCopyRegions;
  uint32_t offset = 0;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

  for (uint32_t i = 0; i < mipLevels; i++) {
    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = i;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = offset;

    bufferCopyRegions.push_back(bufferCopyRegion);

    // TODO: fix offset.
  }

  VkCommandBuffer copyCmd = createCommandBuffer(
      device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

  // Image memory barriers for the texture image

  // The sub resource range describes the regions of the image that will be
  // transitioned using the memory barriers below
  VkImageSubresourceRange subresourceRange = {};
  // Image only contains color data
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  // Start at first mip level
  subresourceRange.baseMipLevel = 0;
  // We will transition on all mip levels
  subresourceRange.levelCount = mipLevels;
  // The 2D texture only has one layer
  subresourceRange.layerCount = 1;

  // Transition the texture image layout to transfer target, so we can
  // safely copy our buffer data to it.
  VkImageMemoryBarrier imageMemoryBarrier =
      vks::initializers::imageMemoryBarrier();
  ;
  imageMemoryBarrier.image = image;
  imageMemoryBarrier.subresourceRange = subresourceRange;
  imageMemoryBarrier.srcAccessMask = 0;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  // Insert a memory dependency at the proper pipeline stages that will
  // execute the image layout transition Source pipeline stage is host
  // write/read exection (VK_PIPELINE_STAGE_HOST_BIT) Destination pipeline
  // stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
  vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);

  // Copy mip levels from staging buffer
  vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(bufferCopyRegions.size()),
                         bufferCopyRegions.data());

  // Once the data has been uploaded we transfer to the texture image to the
  // shader read layout, so it can be sampled from
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

  // Insert a memory dependency at the proper pipeline stages that will
  // execute the image layout transition Source pipeline stage stage is copy
  // command exection (VK_PIPELINE_STAGE_TRANSFER_BIT) Destination pipeline
  // stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
  vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);
  

  // Store current layout for later reuse
  // texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  flushCommandBuffer(device, commandPool, copyCmd, queue, true);

  // Clean up staging resources
  //vkFreeMemory(device, stagingMemory, nullptr);
  //vkDestroyBuffer(device, stagingBuffer, nullptr);
  return VK_SUCCESS;
}

VkResult ComputeOp::copyDeviceImageToHostBuffer(VkBuffer &stagingBuffer,
                                                VkImage &image) {

  // Setup buffer copy regions for each mip level
  std::vector<VkBufferImageCopy> bufferCopyRegions;
  uint32_t offset = 0;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

  for (uint32_t i = 0; i < mipLevels; i++) {
    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = i;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = offset;

    bufferCopyRegions.push_back(bufferCopyRegion);

    // TODO: fix offset.
  }

  // Create the image
		// Create the linear tiled destination image to copy to and to read the memory from
  VkImageCreateInfo imageCreateCI(vks::initializers::imageCreateInfo());
  imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
  // Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
  imageCreateCI.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageCreateCI.extent.width = width;
  imageCreateCI.extent.height = height;
  imageCreateCI.extent.depth = 1;
  imageCreateCI.arrayLayers = 1;
  imageCreateCI.mipLevels = 1;
  imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
  imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  VkImage dstImage;
  VK_CHECK_RESULT(vkCreateImage(device, &imageCreateCI, nullptr, &dstImage));
  // Create memory to back up the image
  VkMemoryRequirements memRequirements;
  VkMemoryAllocateInfo memAllocInfo(vks::initializers::memoryAllocateInfo());
  VkDeviceMemory dstImageMemory;
  vkGetImageMemoryRequirements(device, dstImage, &memRequirements);
  memAllocInfo.allocationSize = memRequirements.size;
  // Memory must be host visible to copy from
  memAllocInfo.memoryTypeIndex = getMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImageMemory));
  VK_CHECK_RESULT(vkBindImageMemory(device, dstImage, dstImageMemory, 0));



  VkCommandBuffer copyCmd = createCommandBuffer(
      device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);


  // Transition destination image to transfer destination layout
  vks::tools::insertImageMemoryBarrier(
	  copyCmd,
	  dstImage,
	  0,
	  VK_ACCESS_TRANSFER_WRITE_BIT,
	  VK_IMAGE_LAYOUT_UNDEFINED,
	  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	  VK_PIPELINE_STAGE_TRANSFER_BIT,
	  VK_PIPELINE_STAGE_TRANSFER_BIT,
	  VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
  
  // Transition swapchain image from present to transfer source layout
  vks::tools::insertImageMemoryBarrier(
	  copyCmd,
	  image,
	  VK_ACCESS_MEMORY_READ_BIT,
	  VK_ACCESS_TRANSFER_READ_BIT,
	  VK_IMAGE_LAYOUT_GENERAL,
	  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	  VK_PIPELINE_STAGE_TRANSFER_BIT,
	  VK_PIPELINE_STAGE_TRANSFER_BIT,
	  VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });


  // Otherwise use image copy (requires us to manually flip components)
  VkImageCopy imageCopyRegion{};
  imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageCopyRegion.srcSubresource.layerCount = 1;
  imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageCopyRegion.dstSubresource.layerCount = 1;
  imageCopyRegion.extent.width = width;
  imageCopyRegion.extent.height = height;
  imageCopyRegion.extent.depth = 1;
  
  // Issue the copy command
  vkCmdCopyImage(
	  copyCmd,
	  image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	  dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	  1,
	  &imageCopyRegion);

  // Transition destination image to general layout, which is the required layout for mapping the image memory later on
  vks::tools::insertImageMemoryBarrier(
	  copyCmd,
	  dstImage,
	  VK_ACCESS_TRANSFER_WRITE_BIT,
	  VK_ACCESS_MEMORY_READ_BIT,
	  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	  VK_IMAGE_LAYOUT_GENERAL,
	  VK_PIPELINE_STAGE_TRANSFER_BIT,
	  VK_PIPELINE_STAGE_TRANSFER_BIT,
	  VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
  
  flushCommandBuffer(device, commandPool, copyCmd, queue, true);

 
  // Get layout of the image (including row pitch)
  VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
  VkSubresourceLayout subResourceLayout;
  vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);
  
  // Map image memory so we can start copying from it
  const char* data;
  vkMapMemory(device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
  //void * mapped;
  std::vector<uint32_t> out;
  memcpy(out.data(), data, 4);
  //data += subResourceLayout.offset;

  std::ofstream file("abc.txt", std::ios::out | std::ios::binary);
  
  // ppm header
  file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";
  
  // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
  bool colorSwizzle = false;
  // Check if source is BGR 
  // Note: Not complete, only contains most common and basic BGR surface formats for demonstation purposes
  /*
  if (!supportsBlit)
  {
	  std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
	  colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChain.colorFormat) != formatsBGR.end());
  }
  */
  
  // ppm binary pixel data
  for (uint32_t y = 0; y < height; y++) 
  {
	  unsigned int *row = (unsigned int*)data;
	  for (uint32_t x = 0; x < width; x++) 
	  {
		  if (colorSwizzle) 
		  { 
			  file.write((char*)row+2, 1);
			  file.write((char*)row+1, 1);
			  file.write((char*)row, 1);
		  }
		  else
		  {
			  file.write((char*)row, 3);
		  }
		  row++;
	  }
	  data += subResourceLayout.rowPitch;
  }
  file.close();
  
  std::cout << "Screenshot saved to disk" << std::endl;
  
  // Clean up resources
  vkUnmapMemory(device, dstImageMemory);
  vkFreeMemory(device, dstImageMemory, nullptr);


  // Clean up staging resources
  //vkFreeMemory(device, stagingMemory, nullptr);
  //vkDestroyBuffer(device, stagingBuffer, nullptr);
  return VK_SUCCESS;
}

VkResult ComputeOp::copyDeviceBufferToHostBuffer(VkBuffer &hostBuffer,
                                                VkBuffer &deviceBuffer) {

  return VK_SUCCESS;
}

VkResult ComputeOp::prepareComputeCommandBuffer(
    VkBuffer &outputDeviceBuffer, VkBuffer &outputHostBuffer,
    VkDeviceMemory &outputHostMemory, const VkDeviceSize &bufferSize) {
  VkCommandBufferBeginInfo cmdBufInfo =
      vks::initializers::commandBufferBeginInfo();

  VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

  // Barrier to ensure that input buffer transfer is finished before compute
  // shader reads from it.
  VkBufferMemoryBarrier bufferBarrier =
      vks::initializers::bufferMemoryBarrier();
  bufferBarrier.buffer = outputDeviceBuffer;
  bufferBarrier.size = VK_WHOLE_SIZE;
  bufferBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
  bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_FLAGS_NONE, 0,
                       nullptr, 1, &bufferBarrier, 0, nullptr);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipelineLayout, 0, 1, &descriptorSet, 0, 0);

  vkCmdDispatch(commandBuffer, BUFFER_ELEMENTS, 1, 1);

  // Barrier to ensure that shader writes are finished before buffer is read
  // back from GPU.
  bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bufferBarrier.buffer = outputDeviceBuffer;
  bufferBarrier.size = VK_WHOLE_SIZE;
  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FLAGS_NONE, 0,
                       nullptr, 1, &bufferBarrier, 0, nullptr);

  // Read back to host visible buffer.
  VkBufferCopy copyRegion = {};
  copyRegion.size = bufferSize;
  vkCmdCopyBuffer(commandBuffer, outputDeviceBuffer, outputHostBuffer, 1,
                  &copyRegion);

  // Barrier to ensure that buffer copy is finished before host reading from
  // it.
  bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bufferBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  bufferBarrier.buffer = outputHostBuffer;
  bufferBarrier.size = VK_WHOLE_SIZE;
  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_HOST_BIT, VK_FLAGS_NONE, 0, nullptr, 1,
                       &bufferBarrier, 0, nullptr);

  VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

  // Submit compute work.
  vkResetFences(device, 1, &fence);
  const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
  computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
  computeSubmitInfo.commandBufferCount = 1;
  computeSubmitInfo.pCommandBuffers = &commandBuffer;
  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &computeSubmitInfo, fence));
  VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

  // Make device writes visible to the host.
  void *mapped;
  vkMapMemory(device, outputHostMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
  VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
  mappedRange.memory = outputHostMemory;
  mappedRange.offset = 0;
  mappedRange.size = VK_WHOLE_SIZE;
  vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);

  // Copy to output.
  memcpy(params_.computeOutput.data(), mapped, bufferSize);
  vkUnmapMemory(device, outputHostMemory);
  return VK_SUCCESS;
}

/**
 * Get the index of a memory type that has all the requested property bits set
 *
 * @param typeBits Bitmask with bits set for each memory type supported by the
 * resource to request for (from VkMemoryRequirements)
 * @param properties Bitmask of properties for the memory type to request
 * @param (Optional) memTypeFound Pointer to a bool that is set to true if a
 * matching memory type has been found
 *
 * @return Index of the requested memory type
 *
 * @throw Throws an exception if memTypeFound is null and no memory type could
 * be found that supports the requested properties
 */
uint32_t ComputeOp::getMemoryType(uint32_t typeBits,
                                  VkMemoryPropertyFlags properties,
                                  VkBool32 *memTypeFound) {
  for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) ==
          properties) {
        if (memTypeFound) {
          *memTypeFound = true;
        }
        return i;
      }
    }
    typeBits >>= 1;
  }

  if (memTypeFound) {
    *memTypeFound = false;
    return 0;
  } else {
    throw std::runtime_error("Could not find a matching memory type");
  }
}

VkResult ComputeOp::prepareDebugLayer() {

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  LOG("loading vulkan lib");
  vks::android::loadVulkanLibrary();
#endif

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Vulkan add example";
  appInfo.pEngineName = "ComputeOp";
  appInfo.apiVersion = VK_API_VERSION_1_0;

  // Vulkan instance creation (without surface extensions).
  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pApplicationInfo = &appInfo;

  uint32_t layerCount = 0;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  const char *validationLayers[] = {
      "VK_LAYER_GOOGLE_threading",      "VK_LAYER_LUNARG_parameter_validation",
      "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation",
      "VK_LAYER_LUNARG_swapchain",      "VK_LAYER_GOOGLE_unique_objects"};
  layerCount = 6;
#else
  const char *validationLayers[] = {"VK_LAYER_LUNARG_standard_validation"};
  layerCount = 1;
#endif
#if DEBUG
  // Check if layers are available
  uint32_t instanceLayerCount;
  vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
  std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
  vkEnumerateInstanceLayerProperties(&instanceLayerCount,
                                     instanceLayers.data());

  bool layersAvailable = true;
  for (auto layerName : validationLayers) {
    bool layerAvailable = false;
    for (auto instanceLayer : instanceLayers) {
      if (strcmp(instanceLayer.layerName, layerName) == 0) {
        layerAvailable = true;
        break;
      }
    }
    if (!layerAvailable) {
      layersAvailable = false;
      break;
    }
  }

  if (layersAvailable) {
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
    const char *validationExt = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
    instanceCreateInfo.enabledLayerCount = layerCount;
    instanceCreateInfo.enabledExtensionCount = 1;
    instanceCreateInfo.ppEnabledExtensionNames = &validationExt;
  }
#endif
  VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  vks::android::loadVulkanFunctions(instance);
#endif
#if DEBUG
  if (layersAvailable) {
    VkDebugReportCallbackCreateInfoEXT debugReportCreateInfo = {};
    debugReportCreateInfo.sType =
        VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debugReportCreateInfo.flags =
        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debugReportCreateInfo.pfnCallback =
        (PFN_vkDebugReportCallbackEXT)debugMessageCallback;

    // We have to explicitly load this function.
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
    assert(vkCreateDebugReportCallbackEXT);
    VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(
        instance, &debugReportCreateInfo, nullptr, &debugReportCallback));
  }
#endif
  return VK_SUCCESS;
}
VkResult ComputeOp::prepareDevice() {
  // Physical device (always use first).
  uint32_t deviceCount = 0;
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
  std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &deviceCount,
                                             physicalDevices.data()));
  physicalDevice = physicalDevices[0];

  // VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
  LOG("GPU: %s\n", deviceProperties.deviceName);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

  // Request a single compute queue.
  const float defaultQueuePriority(0.0f);
  VkDeviceQueueCreateInfo queueCreateInfo = {};
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilyProperties.data());
  for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size());
       i++) {
    if (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
      queueFamilyIndex = i;
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = i;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &defaultQueuePriority;
      break;
    }
  }
  // Create logical device.
  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  VK_CHECK_RESULT(
      vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

  // Get a compute queue.
  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

  // Compute command pool.
  VkCommandPoolCreateInfo cmdPoolInfo = {};
  cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
  cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VK_CHECK_RESULT(
      vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool));
  return VK_SUCCESS;
}

VkResult ComputeOp::preparePipeline(VkBuffer &deviceBuffer,
                                    VkBuffer &filterDeviceBuffer,
                                    VkBuffer &outputDeviceBuffer) {
  // SIZE.
  std::vector<VkDescriptorPoolSize> poolSizes = {
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            BUFFER_NUMBER),
  };

  VkDescriptorPoolCreateInfo descriptorPoolInfo =
      vks::initializers::descriptorPoolCreateInfo(
          static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 1);
  VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr,
                                         &descriptorPool));

  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
  };
  VkDescriptorSetLayoutCreateInfo descriptorLayout =
      vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout,
                                              nullptr, &descriptorSetLayout));

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
      vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo,
                                         nullptr, &pipelineLayout));

  VkDescriptorSetAllocateInfo allocInfo =
      vks::initializers::descriptorSetAllocateInfo(descriptorPool,
                                                   &descriptorSetLayout, 1);
  VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

  VkDescriptorBufferInfo outputBufferDescriptor = {outputDeviceBuffer, 0,
                                                   VK_WHOLE_SIZE};
  VkDescriptorBufferInfo bufferDescriptor = {deviceBuffer, 1, VK_WHOLE_SIZE};

  VkDescriptorBufferInfo filterBufferDescriptor = {filterDeviceBuffer, 2,
                                                   VK_WHOLE_SIZE};

  std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
      vks::initializers::writeDescriptorSet(descriptorSet,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            0, &outputBufferDescriptor),
      vks::initializers::writeDescriptorSet(descriptorSet,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            1, &bufferDescriptor),
      vks::initializers::writeDescriptorSet(descriptorSet,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            2, &filterBufferDescriptor),
  };
  vkUpdateDescriptorSets(
      device, static_cast<uint32_t>(computeWriteDescriptorSets.size()),
      computeWriteDescriptorSets.data(), 0, NULL);

  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo,
                                        nullptr, &pipelineCache));

  // Create pipeline
  VkComputePipelineCreateInfo computePipelineCreateInfo =
      vks::initializers::computePipelineCreateInfo(pipelineLayout, 0);

  // Pass SSBO size via specialization constant
  struct SpecializationData {
    uint32_t BUFFER_ELEMENT_COUNT = BUFFER_ELEMENTS;
  } specializationData;
  VkSpecializationMapEntry specializationMapEntry =
      vks::initializers::specializationMapEntry(0, 0, sizeof(uint32_t));
  VkSpecializationInfo specializationInfo =
      vks::initializers::specializationInfo(1, &specializationMapEntry,
                                            sizeof(SpecializationData),
                                            &specializationData);

  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  shaderStage.module = vks::tools::loadShader(
      androidapp->activity->assetManager,
      (getAssetPath() + params_.shader_path).c_str(), device);
#else
  shaderStage.module = vks::tools::loadShader(
      (getAssetPath() + params_.shader_path).c_str(), device);
#endif
  shaderStage.pName = "main";
  shaderStage.pSpecializationInfo = &specializationInfo;
  shaderModule = shaderStage.module;

  assert(shaderStage.module != VK_NULL_HANDLE);
  computePipelineCreateInfo.stage = shaderStage;
  VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1,
                                           &computePipelineCreateInfo, nullptr,
                                           &pipeline));

  // Create a command buffer for compute operations
  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
          commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &commandBuffer));

  // Fence for compute CB sync
  VkFenceCreateInfo fenceCreateInfo =
      vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
  return VK_SUCCESS;
}

VkResult ComputeOp::prepareImagePipeline(VkBuffer &deviceBuffer,
                                         VkBuffer &filterDeviceBuffer,
                                         VkBuffer &outputDeviceBuffer) {
  // SIZE.
  std::vector<VkDescriptorPoolSize> poolSizes = {
      // Compute UBO
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                            2),
      // Graphics image samplers
      //vks::initializers::descriptorPoolSize(
      //    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
      // Storage buffer for the scene primitives
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            2),
  };

  VkDescriptorPoolCreateInfo descriptorPoolInfo =
      vks::initializers::descriptorPoolCreateInfo(
          static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 1);
  VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr,
                                         &descriptorPool));

  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
      // Binding 0: Storage image (raytraced output)
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
      // Binding 1: Uniform buffer block
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
      // Binding 2: Shader storage buffer for the spheres
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2),
  };

  VkDescriptorSetLayoutCreateInfo descriptorLayout =
      vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout,
                                              nullptr, &descriptorSetLayout));

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
      vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo,
                                         nullptr, &pipelineLayout));

  VkDescriptorSetAllocateInfo allocInfo =
      vks::initializers::descriptorSetAllocateInfo(descriptorPool,
                                                   &descriptorSetLayout, 1);
  VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

  // Setup a descriptor image info for the current texture to be used as a
  // combined image sampler
  VkDescriptorImageInfo imageDescriptor;
  imageDescriptor.imageView = view_;
  imageDescriptor.sampler = sampler_;
  imageDescriptor.imageLayout = imageLayout;

  VkDescriptorImageInfo filterImageDescriptor;
  filterImageDescriptor.imageView = filterView_;
  filterImageDescriptor.sampler = filterSampler_;
  filterImageDescriptor.imageLayout = filterImageLayout;

  VkDescriptorBufferInfo outputBufferDescriptor = {outputDeviceBuffer, 0,
                                                   VK_WHOLE_SIZE};

  std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
      vks::initializers::writeDescriptorSet(descriptorSet,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            0, &outputBufferDescriptor),
      vks::initializers::writeDescriptorSet(
          descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
          &imageDescriptor),
      vks::initializers::writeDescriptorSet(
          descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2,
          &filterImageDescriptor),
  };

  vkUpdateDescriptorSets(
      device, static_cast<uint32_t>(computeWriteDescriptorSets.size()),
      computeWriteDescriptorSets.data(), 0, NULL);

  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo,
                                        nullptr, &pipelineCache));

  // Create pipeline
  VkComputePipelineCreateInfo computePipelineCreateInfo =
      vks::initializers::computePipelineCreateInfo(pipelineLayout, 0);

  // Pass SSBO size via specialization constant
  struct SpecializationData {
    uint32_t BUFFER_ELEMENT_COUNT = BUFFER_ELEMENTS;
  } specializationData;
  VkSpecializationMapEntry specializationMapEntry =
      vks::initializers::specializationMapEntry(0, 0, sizeof(uint32_t));
  VkSpecializationInfo specializationInfo =
      vks::initializers::specializationInfo(1, &specializationMapEntry,
                                            sizeof(SpecializationData),
                                            &specializationData);

  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  shaderStage.module = vks::tools::loadShader(
      androidapp->activity->assetManager,
      (getAssetPath() + params_.shader_path).c_str(), device);
#else
  shaderStage.module = vks::tools::loadShader(
      (getAssetPath() + params_.shader_path).c_str(), device);
#endif
  shaderStage.pName = "main";
  shaderStage.pSpecializationInfo = &specializationInfo;
  shaderModule = shaderStage.module;

  assert(shaderStage.module != VK_NULL_HANDLE);
  computePipelineCreateInfo.stage = shaderStage;
  VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1,
                                           &computePipelineCreateInfo, nullptr,
                                           &pipeline));

  // Create a command buffer for compute operations
  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
          commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &commandBuffer));

  // Fence for compute CB sync
  VkFenceCreateInfo fenceCreateInfo =
      vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
  return VK_SUCCESS;
}

ComputeOp::ComputeOp() {}

void ComputeOp::summary() {
  LOG("Compute input:\n");
  for (auto v : params_.computeInput) {
    LOG("%d \t", v);
  }
  std::cout << std::endl;

  LOG("Compute filter:\n");
  for (auto v : params_.computeFilter) {
    LOG("%d \t", v);
  }
  std::cout << std::endl;

  LOG("Compute output:\n");
  for (auto v : params_.computeOutput) {
    LOG("%d \t", v);
  }
  std::cout << std::endl;
}

ComputeOp::ComputeOp(const InitParams &init_params) : params_(init_params) {
  prepareDebugLayer();
  // Vulkan device creation.
  prepareDevice();
}

void ComputeOp::execute() {
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

    createBufferWithData(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &deviceBuffer,
                         &deviceMemory, bufferSize);

    copyBufferHostToDevice(deviceBuffer, hostBuffer, bufferSize);
  }

  // Copy filter data to VRAM using a staging buffer.
  {
    createBufferWithData(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &filterHostBuffer,
        &filterHostMemory, bufferSize, params_.computeFilter.data());

    createBufferWithData(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &filterDeviceBuffer,
        &filterDeviceMemory, filterBufferSize);

    // Copy to staging buffer.
    copyBufferHostToDevice(filterDeviceBuffer, filterHostBuffer,
                           filterBufferSize);
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
  preparePipeline(deviceBuffer, filterDeviceBuffer, outputDeviceBuffer);

  // Command buffer creation (for compute work submission).
  prepareComputeCommandBuffer(outputDeviceBuffer, outputHostBuffer,
                              outputHostMemory, outputBufferSize);

  vkQueueWaitIdle(queue);

  // Output buffer contents.
  // summary();
}

ComputeOp::~ComputeOp() {
  // Clean up.
  vkDestroyBuffer(device, deviceBuffer, nullptr);
  vkFreeMemory(device, deviceMemory, nullptr);
  vkDestroyBuffer(device, hostBuffer, nullptr);
  vkFreeMemory(device, hostMemory, nullptr);

  vkDestroyBuffer(device, filterDeviceBuffer, nullptr);
  vkFreeMemory(device, filterDeviceMemory, nullptr);
  vkDestroyBuffer(device, filterHostBuffer, nullptr);
  vkFreeMemory(device, filterHostMemory, nullptr);

  vkDestroyBuffer(device, outputDeviceBuffer, nullptr);
  vkFreeMemory(device, outputDeviceMemory, nullptr);

  vkDestroyBuffer(device, outputHostBuffer, nullptr);
  vkFreeMemory(device, outputHostMemory, nullptr);

  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
  vkDestroyDescriptorPool(device, descriptorPool, nullptr);
  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyPipelineCache(device, pipelineCache, nullptr);
  vkDestroyFence(device, fence, nullptr);
  vkDestroyCommandPool(device, commandPool, nullptr);
  vkDestroyShaderModule(device, shaderModule, nullptr);
  vkDestroyDevice(device, nullptr);
#if DEBUG
  if (debugReportCallback) {
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallback =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
    assert(vkDestroyDebugReportCallback);
    vkDestroyDebugReportCallback(instance, debugReportCallback, nullptr);
  }
#endif
  vkDestroyInstance(instance, nullptr);
}
