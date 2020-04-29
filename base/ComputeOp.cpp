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

template class ComputeOp<int>;
template class ComputeOp<float>;

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
android_app *androidapp;
#endif

#define DEBUG (!NDEBUG)

#define BUFFER_ELEMENTS 32
const int BUFFER_NUMBER = 3;
#define USE_INPUT 1
#define USE_FILTER 1

inline VkImageCreateInfo initImageCreateInfo()
{
  VkImageCreateInfo imageCreateInfo{};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  return imageCreateInfo;
}

inline VkMemoryAllocateInfo memoryAllocateInfo()
{
  VkMemoryAllocateInfo memAllocInfo{};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  return memAllocInfo;
}

inline VkSamplerCreateInfo samplerCreateInfo()
{
  VkSamplerCreateInfo samplerCreateInfo{};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.maxAnisotropy = 1.0f;
  return samplerCreateInfo;
}

void flushCommandBuffer(VkDevice device, VkCommandPool commandPool,
                        VkCommandBuffer commandBuffer, VkQueue queue,
                        bool free)
{
  if (commandBuffer == VK_NULL_HANDLE)
  {
    return;
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  VkFenceCreateInfo fenceInfo =
      vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
  VkFence fence;
  VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));
  // Submit to the queue
  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
  VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

  vkDestroyFence(device, fence, nullptr);

  // VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  VK_CHECK_RESULT(vkQueueWaitIdle(queue));

  if (free)
  {
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
  }
}

VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool,
                                    VkCommandBufferLevel level, bool begin)
{
  VkCommandBuffer cmdBuffer;

  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(commandPool, level, 1);

  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

  // If requested, also start the new command buffer
  if (begin)
  {
    VkCommandBufferBeginInfo cmdBufInfo =
        vks::initializers::commandBufferBeginInfo();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
  }

  return cmdBuffer;
}
#if 0
template<class T>
ComputeOp<T>::InitParams::InitParams() = default;

template<class T>
ComputeOp<T>::InitParams::InitParams(const ComputeOp<T>::InitParams &other) = default;
#endif

template <class T>
VkResult ComputeOp<T>::createBufferWithData(
    VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
    VkBuffer *buffer, VkDeviceMemory *memory, VkDeviceSize size, void *data)
{
  // Create the buffer handle
  VkBufferCreateInfo bufferCreateInfo =
      vks::initializers::bufferCreateInfo(usageFlags, size);
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VK_CHECK_RESULT(vkCreateBuffer(device_, &bufferCreateInfo, nullptr, buffer));

  // Create the memory backing up the buffer handle
  VkMemoryRequirements memReqs;
  VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
  vkGetBufferMemoryRequirements(device_, *buffer, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  // Find a memory type index that fits the properties of the buffer
  bool memTypeFound = false;
  for (uint32_t i = 0; i < deviceMemoryProperties_.memoryTypeCount; i++)
  {
    if ((memReqs.memoryTypeBits & 1) == 1)
    {
      if ((deviceMemoryProperties_.memoryTypes[i].propertyFlags &
           memoryPropertyFlags) == memoryPropertyFlags)
      {
        memAlloc.memoryTypeIndex = i;
        memTypeFound = true;
      }
    }
    memReqs.memoryTypeBits >>= 1;
  }
  assert(memTypeFound);
  VK_CHECK_RESULT(vkAllocateMemory(device_, &memAlloc, nullptr, memory));

  if (data != nullptr)
  {
    void *mapped;
    VK_CHECK_RESULT(vkMapMemory(device_, *memory, 0, size, 0, &mapped));
    memcpy(mapped, data, size);
    if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
    {
      // Flush writes to host visible buffer
      void *mapped;
      // vkMapMemory(device_, *memory, 0, VK_WHOLE_SIZE, 0, &mapped);
      VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
      mappedRange.memory = *memory;
      mappedRange.offset = 0;
      mappedRange.size = VK_WHOLE_SIZE;
      vkFlushMappedMemoryRanges(device_, 1, &mappedRange);
      // vkUnmapMemory(device_, *memory);
    }
    vkUnmapMemory(device_, *memory);
  }

  VK_CHECK_RESULT(vkBindBufferMemory(device_, *buffer, *memory, 0));

  return VK_SUCCESS;
}

// Prepare a texture target that is used to store compute shader calculations
// prepareTextureTarget
// For Image2Image.
template <class T>
VkResult ComputeOp<T>::prepareTextureTarget(uint32_t width, uint32_t height,
                                            VkFormat format)
{
  VkFormatProperties formatProperties;

  // Get device properties for the requested texture format
  vkGetPhysicalDeviceFormatProperties(physicalDevice_, format,
                                      &formatProperties);
  // Check if requested image format supports image storage operations
  assert(formatProperties.optimalTilingFeatures &
         VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

  // Prepare blit target texture

  VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.format = format;
  imageCreateInfo.extent = {width, height, 1};
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  // Image will be sampled in the fragment shader and used as storage target in
  // the compute shader
  imageCreateInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
  imageCreateInfo.flags = 0;
  // Sharing mode exclusive means that ownership of the image does not need to
  // be explicitly transferred between the compute and graphics queue
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
  VkMemoryRequirements memReqs;

  VK_CHECK_RESULT(
      vkCreateImage(device_, &imageCreateInfo, nullptr, &outputImage_));

  vkGetImageMemoryRequirements(device_, outputImage_, &memReqs);
  memAllocInfo.allocationSize = memReqs.size;
  memAllocInfo.memoryTypeIndex = getMemoryType(
      memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(device_, &memAllocInfo, nullptr,
                                   &outputImageDeviceMemory_));
  VK_CHECK_RESULT(
      vkBindImageMemory(device_, outputImage_, outputImageDeviceMemory_, 0));

  VkCommandBuffer layoutCmd = createCommandBuffer(
      device_, commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

  outputImageLayout_ = VK_IMAGE_LAYOUT_GENERAL;
  vks::tools::setImageLayout(layoutCmd, outputImage_, VK_IMAGE_ASPECT_COLOR_BIT,
                             VK_IMAGE_LAYOUT_UNDEFINED, outputImageLayout_);

  // flushCommandBuffer(layoutCmd, queue, true);
  flushCommandBuffer(device_, commandPool_, layoutCmd, queue_, true);

  // Create sampler
  VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler.addressModeV = sampler.addressModeU;
  sampler.addressModeW = sampler.addressModeU;
  sampler.mipLodBias = 0.0f;
  sampler.maxAnisotropy = 1.0f;
  sampler.compareOp = VK_COMPARE_OP_NEVER;
  sampler.minLod = 0.0f;
  sampler.maxLod = 1;
  sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  VK_CHECK_RESULT(
      vkCreateSampler(device_, &sampler, nullptr, &outputImageSampler_));

  // Create image view
  VkImageViewCreateInfo viewInfo = vks::initializers::imageViewCreateInfo();
  viewInfo.image = VK_NULL_HANDLE;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                         VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  viewInfo.image = outputImage_;
  VK_CHECK_RESULT(
      vkCreateImageView(device_, &viewInfo, nullptr, &outputImageView_));

  /*
      // Initialize a descriptor for later use
      tex->descriptor.imageLayout = tex->imageLayout;
      tex->descriptor.imageView = tex->view;
      tex->descriptor.sampler = tex->sampler;
      tex->device = vulkanDevice;
      */
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::copyBufferHostToDevice(VkBuffer &deviceBuffer,
                                              VkBuffer &hostBuffer,
                                              const VkDeviceSize &bufferSize)
{
  // Copy to staging buffer.
  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
          commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
  VkCommandBuffer copyCmd;
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device_, &cmdBufAllocateInfo, &copyCmd));
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
  VK_CHECK_RESULT(vkCreateFence(device_, &fenceInfo, nullptr, &fence));

  // Submit to the queue.
  VK_CHECK_RESULT(vkQueueSubmit(queue_, 1, &submitInfo, fence));
  VK_CHECK_RESULT(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX));

  vkDestroyFence(device_, fence, nullptr);
  vkFreeCommandBuffers(device_, commandPool_, 1, &copyCmd);
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::createDeviceImage(VkImage &image)
{
  VkFormat format = imageFormat_;
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
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
  VK_CHECK_RESULT(vkCreateImage(device_, &imageCreateInfo, nullptr, &image));
  VkMemoryRequirements memReqs = {};
  VkMemoryAllocateInfo memAllocInfo = memoryAllocateInfo();
  vkGetImageMemoryRequirements(device_, image, &memReqs);
  memAllocInfo.allocationSize = memReqs.size;
  memAllocInfo.memoryTypeIndex = getMemoryType(
      memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkDeviceMemory deviceMemory;
  // VkMemoryAllocateInfo memAllocInfo =
  // vks::initializers::memoryAllocateInfo();

  memAllocInfo.allocationSize = memReqs.size;
  memAllocInfo.memoryTypeIndex = getMemoryType(
      memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  // VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo,
  // nullptr, &deviceMemory));

  VK_CHECK_RESULT(
      vkAllocateMemory(device_, &memAllocInfo, nullptr, &deviceMemory));
  VK_CHECK_RESULT(vkBindImageMemory(device_, image, deviceMemory, 0));
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::createSampler(VkImage &image, VkSampler &sampler,
                                     VkImageView &view)
{
  VkFormat format = imageFormat_;
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
  VK_CHECK_RESULT(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler));

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
  VK_CHECK_RESULT(vkCreateImageView(device_, &viewInfo, nullptr, &view));
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::copyHostBufferToDeviceImage(VkImage &image,
                                                   VkBuffer &hostBuffer)
{

  // Setup buffer copy regions for each mip level
  std::vector<VkBufferImageCopy> bufferCopyRegions;
  uint32_t offset = 0;
  VkFormat format = imageFormat_;

  for (uint32_t i = 0; i < mipLevels; i++)
  {
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
      device_, commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

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
  vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);

  // Copy mip levels from staging buffer
  vkCmdCopyBufferToImage(copyCmd, hostBuffer, image,
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
  vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);

  // Store current layout for later reuse
  // texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  flushCommandBuffer(device_, commandPool_, copyCmd, queue_, true);

  // Clean up staging resources
  // vkFreeMemory(device, stagingMemory, nullptr);
  // vkDestroyBuffer(device, stagingBuffer, nullptr);
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::copyDeviceImageToHostBuffer(VkImage &image)
{

  // Setup buffer copy regions for each mip level
  std::vector<VkBufferImageCopy> bufferCopyRegions;
  uint32_t offset = 0;
  VkFormat format = imageFormat_;

  for (uint32_t i = 0; i < mipLevels; i++)
  {
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
  // Create the linear tiled destination image to copy to and to read the memory
  // from
  VkImageCreateInfo imageCreateCI(vks::initializers::imageCreateInfo());
  imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
  // Note that vkCmdBlitImage (if supported) will also do format conversions if
  // the swapchain color format would differ
  imageCreateCI.format = imageFormat_;
  imageCreateCI.extent.width = width;
  imageCreateCI.extent.height = height;
  imageCreateCI.extent.depth = 1;
  imageCreateCI.arrayLayers = 1;
  imageCreateCI.mipLevels = 1;
  imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
  imageCreateCI.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  VkImage dstImage;
  VK_CHECK_RESULT(vkCreateImage(device_, &imageCreateCI, nullptr, &dstImage));
  // Create memory to back up the image
  VkMemoryRequirements memRequirements;
  VkMemoryAllocateInfo memAllocInfo(vks::initializers::memoryAllocateInfo());
  VkDeviceMemory dstImageMemory;
  vkGetImageMemoryRequirements(device_, dstImage, &memRequirements);
  memAllocInfo.allocationSize = memRequirements.size;
  // Memory must be host visible to copy from
  memAllocInfo.memoryTypeIndex = getMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK_RESULT(
      vkAllocateMemory(device_, &memAllocInfo, nullptr, &dstImageMemory));
  VK_CHECK_RESULT(vkBindImageMemory(device_, dstImage, dstImageMemory, 0));

  VkCommandBuffer copyCmd = createCommandBuffer(
      device_, commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

  // Transition destination image to transfer destination layout
  vks::tools::insertImageMemoryBarrier(
      copyCmd, dstImage, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  // Transition swapchain image from present to transfer source layout
  vks::tools::insertImageMemoryBarrier(
      copyCmd, image, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

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
  vkCmdCopyImage(copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);

  // Transition destination image to general layout, which is the required
  // layout for mapping the image memory later on
  vks::tools::insertImageMemoryBarrier(
      copyCmd, dstImage, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  flushCommandBuffer(device_, commandPool_, copyCmd, queue_, true);

  // Get layout of the image (including row pitch)
  VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
  VkSubresourceLayout subResourceLayout;
  vkGetImageSubresourceLayout(device_, dstImage, &subResource,
                              &subResourceLayout);

  // Map image memory so we can start copying from it
  printf("\n%s,%d, Read from memory:\n", __func__, __LINE__);
  const int *data;
  vkMapMemory(device_, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void **)&data);
#if 1
  T out[32];
  memcpy(out, data, 32 * sizeof(T));
  for (uint32_t y = 0; y < 32; y++)
  {
    printf("%f", (T) * (out + y));
    if (y != 31)
      printf(",");
  }
#endif

#ifdef SAVE_TO_FILE
  data += subResourceLayout.offset;

  std::ofstream file("deviceimage.ppm", std::ios::out | std::ios::binary);

  // ppm header
  file << "P6\n"
       << width << "\n"
       << height << "\n"
       << 255 << "\n";

  // If source is BGR (destination is always RGB) and we can't use blit (which
  // does automatic conversion), we'll have to manually swizzle color components
  bool colorSwizzle = false;
  bool supportsBlit = false;
  // Check if source is BGR
  // Note: Not complete, only contains most common and basic BGR surface formats
  // for demonstation purposes
  /*
  if (!supportsBlit)
  {
          std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB,
  VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM }; colorSwizzle =
  (std::find(formatsBGR.begin(), formatsBGR.end(), swapChain.colorFormat) !=
  formatsBGR.end());
  }
  */

  // ppm binary pixel data
  printf("\n Read pixel in PPM format:\n");
  for (uint32_t y = 0; y < height; y++)
  {
    T *row = (T *)data;
    for (uint32_t x = 0; x < width; x++)
    {
      if (colorSwizzle)
      {
        file.write((char *)row + 2, 1);
        file.write((char *)row + 1, 1);
        file.write((char *)row, 1);
      }
      else
      {
        file.write((char *)row, 3);
      }
      printf("%f,", (T)(*row));
      row++;
    }
    printf("\n");
    data += subResourceLayout.rowPitch;
  }
  file.close();

  std::cout << "Screenshot saved to disk" << std::endl;
#endif

  // Clean up resources
  vkUnmapMemory(device_, dstImageMemory);
  vkFreeMemory(device_, dstImageMemory, nullptr);
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::copyDeviceBufferToHostBuffer(VkBuffer &deviceBuffer, const VkDeviceSize &bufferSize)
{
  VkBuffer hostBuffer;
  VkDeviceMemory hostMemory;
#if 1
  vkQueueWaitIdle(queue_);

  createBufferWithData(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &hostBuffer,
                       &hostMemory, bufferSize);

  // Copy to staging buffer
  VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
  VkCommandBuffer copyCmd;
  VK_CHECK_RESULT(vkAllocateCommandBuffers(device_, &cmdBufAllocateInfo, &copyCmd));
  VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
  VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

  VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();

  // Barrier to ensure that shader writes are finished before buffer is read back from GPU
  bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bufferBarrier.buffer = deviceBuffer;
  bufferBarrier.size = VK_WHOLE_SIZE;
  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(
      copyCmd,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_FLAGS_NONE,
      0, nullptr,
      1, &bufferBarrier,
      0, nullptr);

  VkBufferCopy copyRegion = {};
  copyRegion.size = bufferSize;
  vkCmdCopyBuffer(copyCmd, hostBuffer, deviceBuffer, 1, &copyRegion);

  // Barrier to ensure that buffer copy is finished before host reading from it
  bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bufferBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  bufferBarrier.buffer = hostBuffer;
  bufferBarrier.size = VK_WHOLE_SIZE;
  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(
      copyCmd,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // VK_PIPELINE_STAGE_HOST_BIT,
      VK_FLAGS_NONE,
      0, nullptr,
      1, &bufferBarrier,
      0, nullptr);

  flushCommandBuffer(device_, commandPool_, commandBuffer_, queue_, true);
  #if 0
  VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

  VkSubmitInfo submitInfo = vks::initializers::submitInfo();
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &copyCmd;
  VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
  VkFence fence;
  VK_CHECK_RESULT(vkCreateFence(device_, &fenceInfo, nullptr, &fence));

  // Submit to the queue
  VK_CHECK_RESULT(vkQueueSubmit(queue_, 1, &submitInfo, fence));
  VK_CHECK_RESULT(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX));
  vkQueueWaitIdle(queue_);

  vkDestroyFence(device_, fence, nullptr);
  vkFreeCommandBuffers(device_, commandPool_, 1, &copyCmd);

  #endif

  // Make device writes visible to the host.
  void *mapped;
  vkMapMemory(device_, hostMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
  VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
  mappedRange.memory = hostMemory;
  mappedRange.offset = 0;
  mappedRange.size = VK_WHOLE_SIZE;
  vkInvalidateMappedMemoryRanges(device_, 1, &mappedRange);

  // Copy to output.
  memcpy(params_.computeOutput.data(), mapped, bufferSize);
#endif

#if 1
  T out[32];
  memcpy(out, mapped, 32 * sizeof(T));
  for (uint32_t y = 0; y < 32; y++)
  {
    printf("%f", (T) * (out + y));
    if (y != 31)
      printf(",");
  }
  printf("\n");
#endif

#if 0
	VkCommandBuffer copyCmd = createCommandBuffer(
		device_, commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);


	  VkCommandBufferBeginInfo cmdBufInfo =
		  vks::initializers::commandBufferBeginInfo();

	
	  VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer_, &cmdBufInfo));
	
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
	
	  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
						   nullptr, 1, &bufferBarrier, 0, nullptr);
	
	
	  // Barrier to ensure that shader writes are finished before buffer is read
	  // back from GPU.
	  bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	  bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	  bufferBarrier.buffer = outputDeviceBuffer;
	  bufferBarrier.size = VK_WHOLE_SIZE;
	  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	
	  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
						   nullptr, 1, &bufferBarrier, 0, nullptr);
	
	  // Read back to host visible buffer.
	  VkBufferCopy copyRegion = {};
	  copyRegion.size = bufferSize;
	  vkCmdCopyBuffer(commandBuffer_, outputDeviceBuffer, outputHostBuffer, 1,
					  &copyRegion);
	
	  // Barrier to ensure that buffer copy is finished before host reading from
	  // it.
	  bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	  bufferBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	  bufferBarrier.buffer = outputHostBuffer;
	  bufferBarrier.size = VK_WHOLE_SIZE;
	  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	
	  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
						   nullptr, 1, &bufferBarrier, 0, nullptr);
	
	  flushCommandBuffer(device_, commandPool_, copyCmd, queue_, true);
	

	  // Make device writes visible to the host.
	  void *mapped;
	  vkMapMemory(device_, outputHostMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
	  VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
	  mappedRange.memory = outputHostMemory;
	  mappedRange.offset = 0;
	  mappedRange.size = VK_WHOLE_SIZE;
	  vkInvalidateMappedMemoryRanges(device_, 1, &mappedRange);
	
	  // Copy to output.
	  memcpy(params_.computeOutput.data(), mapped, bufferSize);
#endif

#if 0
		T out[32];
		memcpy(out, mapped, 32 * sizeof(T));
		for (uint32_t y = 0; y < 32; y++) {
		  printf("%f", (T)*(out + y));
		  if (y != 31)
			printf(",");
		}
	  vkUnmapMemory(device_, outputHostMemory);
#endif
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::prepareComputeCommandBuffer(
    VkBuffer &outputDeviceBuffer, VkBuffer &outputHostBuffer,
    VkDeviceMemory &outputHostMemory, const VkDeviceSize &bufferSize)
{
  VkCommandBufferBeginInfo cmdBufInfo =
      vks::initializers::commandBufferBeginInfo();

  VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer_, &cmdBufInfo));

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

  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
                       nullptr, 1, &bufferBarrier, 0, nullptr);

  vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
  vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipelineLayout_, 0, 1, &descriptorSet_, 0, 0);

  vkCmdDispatch(commandBuffer_, BUFFER_ELEMENTS, 1, 1);

#if 0
  // Barrier to ensure that shader writes are finished before buffer is read
  // back from GPU.
  bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bufferBarrier.buffer = outputDeviceBuffer;
  bufferBarrier.size = VK_WHOLE_SIZE;
  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
                       nullptr, 1, &bufferBarrier, 0, nullptr);

  // Read back to host visible buffer.
  VkBufferCopy copyRegion = {};
  copyRegion.size = bufferSize;
  vkCmdCopyBuffer(commandBuffer_, outputDeviceBuffer, outputHostBuffer, 1,
                  &copyRegion);

  // Barrier to ensure that buffer copy is finished before host reading from
  // it.
  bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bufferBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  bufferBarrier.buffer = outputHostBuffer;
  bufferBarrier.size = VK_WHOLE_SIZE;
  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
                       nullptr, 1, &bufferBarrier, 0, nullptr);
#endif
  VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer_));
  VkFence fence;
  VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
  VK_CHECK_RESULT(vkCreateFence(device_, &fenceInfo, nullptr, &fence));

  // Submit compute work.
  //vkResetFences(device_, 1, &fence);
  const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
  computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
  computeSubmitInfo.commandBufferCount = 1;
  computeSubmitInfo.pCommandBuffers = &commandBuffer_;
  VK_CHECK_RESULT(vkQueueSubmit(queue_, 1, &computeSubmitInfo, fence));
  VK_CHECK_RESULT(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX));
#if 0
  // Make device writes visible to the host.
  void *mapped;
  vkMapMemory(device_, outputHostMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
  VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
  mappedRange.memory = outputHostMemory;
  mappedRange.offset = 0;
  mappedRange.size = VK_WHOLE_SIZE;
  vkInvalidateMappedMemoryRanges(device_, 1, &mappedRange);

  // Copy to output.
  memcpy(params_.computeOutput.data(), mapped, bufferSize);

  T out[32];
  memcpy(out, mapped, 32 * sizeof(T));
  for (uint32_t y = 0; y < 32; y++)
  {
    printf("%f", (T) * (out + y));
    if (y != 31)
      printf(",");
  }
  printf("\n");


  vkUnmapMemory(device_, outputHostMemory);
#endif
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::prepareComputeImageToImageCommandBuffer()
{
  vkQueueWaitIdle(queue_);
  VkCommandBufferBeginInfo cmdBufInfo =
      vks::initializers::commandBufferBeginInfo();

  VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer_, &cmdBufInfo));

  // Image memory barrier to make sure that compute shader writes are finished
  // before sampling from the texture
  VkImageMemoryBarrier imageMemoryBarrier = {};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
#ifdef USE_INPUT

  // We won't be changing the layout of the image
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageMemoryBarrier.image = image_;
  imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
                       nullptr, 0, nullptr, 1, &imageMemoryBarrier);
#endif
#ifdef USE_FILTER
  imageMemoryBarrier.image = filterImage_;
  imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
                       nullptr, 0, nullptr, 1, &imageMemoryBarrier);
#endif

  vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
  vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipelineLayout_, 0, 1, &descriptorSet_, 0, 0);

  vkCmdDispatch(commandBuffer_, width, height, 1);

  imageMemoryBarrier.image = outputImage_;
  imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_FLAGS_NONE, 0,
                       nullptr, 0, nullptr, 1, &imageMemoryBarrier);

  VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer_));
  VkFence fence;
  VkFenceCreateInfo fenceCreateInfo =
      vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VK_CHECK_RESULT(vkCreateFence(device_, &fenceCreateInfo, nullptr, &fence));

  // Submit compute work.
  vkResetFences(device_, 1, &fence);
  const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
  computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
  computeSubmitInfo.commandBufferCount = 1;
  computeSubmitInfo.pCommandBuffers = &commandBuffer_;
  VK_CHECK_RESULT(vkQueueSubmit(queue_, 1, &computeSubmitInfo, fence));
  VK_CHECK_RESULT(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX));
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
template <class T>
uint32_t ComputeOp<T>::getMemoryType(uint32_t typeBits,
                                     VkMemoryPropertyFlags properties,
                                     VkBool32 *memTypeFound)
{
  for (uint32_t i = 0; i < deviceMemoryProperties_.memoryTypeCount; i++)
  {
    if ((typeBits & 1) == 1)
    {
      if ((deviceMemoryProperties_.memoryTypes[i].propertyFlags & properties) ==
          properties)
      {
        if (memTypeFound)
        {
          *memTypeFound = true;
        }
        return i;
      }
    }
    typeBits >>= 1;
  }

  if (memTypeFound)
  {
    *memTypeFound = false;
    return 0;
  }
  else
  {
    throw std::runtime_error("Could not find a matching memory type");
  }
}

template <class T>
VkResult ComputeOp<T>::prepareDebugLayer()
{

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
      "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation",
      "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation",
      "VK_LAYER_LUNARG_swapchain", "VK_LAYER_GOOGLE_unique_objects"};
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
  for (auto layerName : validationLayers)
  {
    bool layerAvailable = false;
    for (auto instanceLayer : instanceLayers)
    {
      if (strcmp(instanceLayer.layerName, layerName) == 0)
      {
        layerAvailable = true;
        break;
      }
    }
    if (!layerAvailable)
    {
      layersAvailable = false;
      break;
    }
  }

  if (layersAvailable)
  {
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
    const char *validationExt = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
    instanceCreateInfo.enabledLayerCount = layerCount;
    instanceCreateInfo.enabledExtensionCount = 1;
    instanceCreateInfo.ppEnabledExtensionNames = &validationExt;
  }
#endif
  VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance_));

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  vks::android::loadVulkanFunctions(instance);
#endif
#if DEBUG
  if (layersAvailable)
  {
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
            vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT"));
    assert(vkCreateDebugReportCallbackEXT);
    VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(
        instance_, &debugReportCreateInfo, nullptr, &debugReportCallback));
  }
#endif
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::prepareDevice()
{
  // Physical device (always use first).
  uint32_t deviceCount = 0;
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr));
  std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance_, &deviceCount,
                                             physicalDevices.data()));
  physicalDevice_ = physicalDevices[0];

  // VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(physicalDevice_, &deviceProperties_);
  LOG("GPU: %s\n", deviceProperties_.deviceName);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_,
                                      &deviceMemoryProperties_);

  // Request a single compute queue.
  const float defaultQueuePriority(0.0f);
  VkDeviceQueueCreateInfo queueCreateInfo = {};
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount,
                                           queueFamilyProperties.data());
  for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size());
       i++)
  {
    if (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
    {
      queueFamilyIndex_ = i;
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
      vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_));

  // Get a compute queue.
  vkGetDeviceQueue(device_, queueFamilyIndex_, 0, &queue_);

  // Compute command pool.
  VkCommandPoolCreateInfo cmdPoolInfo = {};
  cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolInfo.queueFamilyIndex = queueFamilyIndex_;
  cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VK_CHECK_RESULT(
      vkCreateCommandPool(device_, &cmdPoolInfo, nullptr, &commandPool_));
  return VK_SUCCESS;
}

template <class T>
VkResult
ComputeOp<T>::prepareBufferToBufferPipeline(VkBuffer &deviceBuffer,
                                            VkBuffer &filterDeviceBuffer,
                                            VkBuffer &outputDeviceBuffer)
{
  // SIZE.
  std::vector<VkDescriptorPoolSize> poolSizes = {
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            BUFFER_NUMBER),
  };

  VkDescriptorPoolCreateInfo descriptorPoolInfo =
      vks::initializers::descriptorPoolCreateInfo(
          static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 1);
  VK_CHECK_RESULT(vkCreateDescriptorPool(device_, &descriptorPoolInfo, nullptr,
                                         &descriptorPool_));

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
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device_, &descriptorLayout,
                                              nullptr, &descriptorSetLayout_));

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
      vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout_, 1);
  VK_CHECK_RESULT(vkCreatePipelineLayout(device_, &pipelineLayoutCreateInfo,
                                         nullptr, &pipelineLayout_));

  VkDescriptorSetAllocateInfo allocInfo =
      vks::initializers::descriptorSetAllocateInfo(descriptorPool_,
                                                   &descriptorSetLayout_, 1);
  VK_CHECK_RESULT(
      vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_));

  VkDescriptorBufferInfo outputBufferDescriptor = {outputDeviceBuffer, 0,
                                                   VK_WHOLE_SIZE};
  VkDescriptorBufferInfo bufferDescriptor = {deviceBuffer, 1, VK_WHOLE_SIZE};

  VkDescriptorBufferInfo filterBufferDescriptor = {filterDeviceBuffer, 2,
                                                   VK_WHOLE_SIZE};

  std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            0, &outputBufferDescriptor),
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            1, &bufferDescriptor),
#ifdef USE_FILTER
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            2, &filterBufferDescriptor),
#endif
  };
  vkUpdateDescriptorSets(
      device_, static_cast<uint32_t>(computeWriteDescriptorSets.size()),
      computeWriteDescriptorSets.data(), 0, NULL);

  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreatePipelineCache(device_, &pipelineCacheCreateInfo,
                                        nullptr, &pipelineCache_));

  // Create pipeline
  VkComputePipelineCreateInfo computePipelineCreateInfo =
      vks::initializers::computePipelineCreateInfo(pipelineLayout_, 0);

  // Pass SSBO size via specialization constant
  struct SpecializationData
  {
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
      (getAssetPath() + params_.shader_path).c_str(), device_);
#else
  shaderStage.module = vks::tools::loadShader(
      (getAssetPath() + params_.shader_path).c_str(), device_);
#endif
  shaderStage.pName = "main";
  shaderStage.pSpecializationInfo = &specializationInfo;
  shaderModule_ = shaderStage.module;

  assert(shaderStage.module != VK_NULL_HANDLE);
  computePipelineCreateInfo.stage = shaderStage;
  VK_CHECK_RESULT(vkCreateComputePipelines(device_, pipelineCache_, 1,
                                           &computePipelineCreateInfo, nullptr,
                                           &pipeline_));

  // Create a command buffer for compute operations
  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
          commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device_, &cmdBufAllocateInfo, &commandBuffer_));

  // Fence for compute CB sync
  VkFenceCreateInfo fenceCreateInfo =
      vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VK_CHECK_RESULT(vkCreateFence(device_, &fenceCreateInfo, nullptr, &fence_));
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::prepareImageToBufferPipeline(VkBuffer &deviceBuffer,
                                                    VkBuffer &filterDeviceBuffer,
                                                    VkBuffer &outputDeviceBuffer)
{
  // SIZE.
  std::vector<VkDescriptorPoolSize> poolSizes = {
      // Compute UBO
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                            2),
      // Graphics image samplers
      vks::initializers::descriptorPoolSize(
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2),
      // Storage buffer for the scene primitives
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            2),
  };

  VkDescriptorPoolCreateInfo descriptorPoolInfo =
      vks::initializers::descriptorPoolCreateInfo(
          static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 3);
  VK_CHECK_RESULT(vkCreateDescriptorPool(device_, &descriptorPoolInfo, nullptr,
                                         &descriptorPool_));

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
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device_, &descriptorLayout,
                                              nullptr, &descriptorSetLayout_));

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
      vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout_, 1);
  VK_CHECK_RESULT(vkCreatePipelineLayout(device_, &pipelineLayoutCreateInfo,
                                         nullptr, &pipelineLayout_));

  VkDescriptorSetAllocateInfo allocInfo =
      vks::initializers::descriptorSetAllocateInfo(descriptorPool_,
                                                   &descriptorSetLayout_, 1);
  VK_CHECK_RESULT(
      vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_));

  // Setup a descriptor image info for the current texture to be used as a
  // combined image sampler
  VkDescriptorImageInfo imageDescriptor;
  imageDescriptor.imageView = view_;
  imageDescriptor.sampler = sampler_;
  imageDescriptor.imageLayout = imageLayout_;

  VkDescriptorImageInfo filterImageDescriptor;
  filterImageDescriptor.imageView = filterView_;
  filterImageDescriptor.sampler = filterSampler_;
  filterImageDescriptor.imageLayout = filterImageLayout_;

  VkDescriptorBufferInfo outputBufferDescriptor = {outputDeviceBuffer, 0,
                                                   VK_WHOLE_SIZE};

  std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            0, &outputBufferDescriptor),
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                            &imageDescriptor),
#ifdef USE_FILTER
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2,
                                            &filterImageDescriptor),
#endif
  };

  vkUpdateDescriptorSets(
      device_, static_cast<uint32_t>(computeWriteDescriptorSets.size()),
      computeWriteDescriptorSets.data(), 0, NULL);

  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreatePipelineCache(device_, &pipelineCacheCreateInfo,
                                        nullptr, &pipelineCache_));

  // Create pipeline
  VkComputePipelineCreateInfo computePipelineCreateInfo =
      vks::initializers::computePipelineCreateInfo(pipelineLayout_, 0);

  // Pass SSBO size via specialization constant
  struct SpecializationData
  {
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
      (getAssetPath() + params_.shader_path).c_str(), device_);
#else
  shaderStage.module = vks::tools::loadShader(
      (getAssetPath() + params_.shader_path).c_str(), device_);
#endif
  shaderStage.pName = "main";
  shaderStage.pSpecializationInfo = &specializationInfo;
  shaderModule_ = shaderStage.module;

  assert(shaderStage.module != VK_NULL_HANDLE);
  computePipelineCreateInfo.stage = shaderStage;
  VK_CHECK_RESULT(vkCreateComputePipelines(device_, pipelineCache_, 1,
                                           &computePipelineCreateInfo, nullptr,
                                           &pipeline_));

  // Create a command buffer for compute operations
  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
          commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device_, &cmdBufAllocateInfo, &commandBuffer_));

  // Fence for compute CB sync
  VkFenceCreateInfo fenceCreateInfo =
      vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VK_CHECK_RESULT(vkCreateFence(device_, &fenceCreateInfo, nullptr, &fence_));
  return VK_SUCCESS;
}

template <class T>
VkResult ComputeOp<T>::prepareImageToImagePipeline()
{
  // SIZE.
  std::vector<VkDescriptorPoolSize> poolSizes = {
      // Compute UBO
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                            6),
      // Graphics image samplers
      vks::initializers::descriptorPoolSize(
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6),
  };

  VkDescriptorPoolCreateInfo descriptorPoolInfo =
      vks::initializers::descriptorPoolCreateInfo(
          static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 3);
  VK_CHECK_RESULT(vkCreateDescriptorPool(device_, &descriptorPoolInfo, nullptr,
                                         &descriptorPool_));

  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
      // Binding 0: Storage image (raytraced output)
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
#ifdef USE_INPUT
      // Binding 2: Shader storage buffer for the spheres
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
#endif
#ifdef USE_FILTER

      // Binding 2: Shader storage buffer for the spheres
      vks::initializers::descriptorSetLayoutBinding(
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2),
#endif
  };

  VkDescriptorSetLayoutCreateInfo descriptorLayout =
      vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device_, &descriptorLayout,
                                              nullptr, &descriptorSetLayout_));

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
      vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout_, 1);
  VK_CHECK_RESULT(vkCreatePipelineLayout(device_, &pipelineLayoutCreateInfo,
                                         nullptr, &pipelineLayout_));

  VkDescriptorSetAllocateInfo allocInfo =
      vks::initializers::descriptorSetAllocateInfo(descriptorPool_,
                                                   &descriptorSetLayout_, 1);
  VK_CHECK_RESULT(
      vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_));

  // Setup a descriptor image info for the current texture to be used as a
  // combined image sampler
  VkDescriptorImageInfo imageDescriptor;
  imageDescriptor.imageView = view_;
  imageDescriptor.sampler = sampler_;
  imageDescriptor.imageLayout = imageLayout_;

  VkDescriptorImageInfo filterImageDescriptor;
  filterImageDescriptor.imageView = filterView_;
  filterImageDescriptor.sampler = filterSampler_;
  filterImageDescriptor.imageLayout = filterImageLayout_;

  VkDescriptorImageInfo outputImageDescriptor;
  outputImageDescriptor.imageView = outputImageView_;
  outputImageDescriptor.sampler = outputImageSampler_;
  outputImageDescriptor.imageLayout = outputImageLayout_;

  std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0,
                                            &outputImageDescriptor),
#ifdef USE_INPUT
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                            &imageDescriptor),
#endif
#ifdef USE_FILTER
      vks::initializers::writeDescriptorSet(descriptorSet_,
                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2,
                                            &filterImageDescriptor),
#endif
  };
  vkUpdateDescriptorSets(
      device_, static_cast<uint32_t>(computeWriteDescriptorSets.size()),
      computeWriteDescriptorSets.data(), 0, NULL);

  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreatePipelineCache(device_, &pipelineCacheCreateInfo,
                                        nullptr, &pipelineCache_));

  // Create pipeline
  VkComputePipelineCreateInfo computePipelineCreateInfo =
      vks::initializers::computePipelineCreateInfo(pipelineLayout_, 0);

  // Pass SSBO size via specialization constant
  struct SpecializationData
  {
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
      (getAssetPath() + params_.shader_path).c_str(), device_);
#else
  shaderStage.module = vks::tools::loadShader(
      (getAssetPath() + params_.shader_path).c_str(), device_);
#endif
  shaderStage.pName = "main";
  shaderStage.pSpecializationInfo = &specializationInfo;
  shaderModule_ = shaderStage.module;

  assert(shaderStage.module != VK_NULL_HANDLE);
  computePipelineCreateInfo.stage = shaderStage;
  VK_CHECK_RESULT(vkCreateComputePipelines(device_, pipelineCache_, 1,
                                           &computePipelineCreateInfo, nullptr,
                                           &pipeline_));

  // Create a command buffer for compute operations
  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
          commandPool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device_, &cmdBufAllocateInfo, &commandBuffer_));

#if 0
  // Fence for compute CB sync
  VkFenceCreateInfo fenceCreateInfo =
      vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VK_CHECK_RESULT(vkCreateFence(device_, &fenceCreateInfo, nullptr, &fence_));
#endif
  return VK_SUCCESS;
}

template <class T>
ComputeOp<T>::ComputeOp() {}

template <class T>
void ComputeOp<T>::summary()
{
#if 0
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
#endif
  LOG("\nCompute output:\n");
  for (auto v : params_.computeOutput)
  {
    LOG("%d \t", v);
  }
  std::cout << std::endl;
}

template <class T>
ComputeOp<T>::ComputeOp(const InitParams<T> &init_params) : params_(init_params)
{
  prepareDebugLayer();
  // Vulkan device creation.
  prepareDevice();
}

template <class T>
void ComputeOp<T>::execute()
{
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

    createBufferWithData(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &deviceBuffer_,
                         &deviceMemory_, bufferSize);

    copyBufferHostToDevice(deviceBuffer_, hostBuffer_, bufferSize);
  }

  // Copy filter data to VRAM using a staging buffer.
  {
    createBufferWithData(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &filterHostBuffer_,
        &filterHostMemory_, bufferSize, params_.computeFilter.data());

    createBufferWithData(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &filterDeviceBuffer_,
        &filterDeviceMemory_, filterBufferSize);

    // Copy to staging buffer.
    copyBufferHostToDevice(filterDeviceBuffer_, filterHostBuffer_,
                           filterBufferSize);
    // Debug only.
    //copyDeviceImageToHostBuffer(image_);
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
  prepareComputeCommandBuffer(outputDeviceBuffer_, outputHostBuffer_,
                              outputHostMemory_, outputBufferSize);
  copyDeviceBufferToHostBuffer(outputDeviceBuffer_, bufferSize);

  vkQueueWaitIdle(queue_);

  // Output buffer contents.
  // summary();
}

template <class T>
ComputeOp<T>::~ComputeOp()
{
  // Clean up.
  vkDestroyBuffer(device_, deviceBuffer_, nullptr);
  vkFreeMemory(device_, deviceMemory_, nullptr);
  vkDestroyBuffer(device_, hostBuffer_, nullptr);
  vkFreeMemory(device_, hostMemory_, nullptr);

  vkDestroyBuffer(device_, filterDeviceBuffer_, nullptr);
  vkFreeMemory(device_, filterDeviceMemory_, nullptr);
  vkDestroyBuffer(device_, filterHostBuffer_, nullptr);
  vkFreeMemory(device_, filterHostMemory_, nullptr);

  vkDestroyBuffer(device_, outputDeviceBuffer_, nullptr);
  vkFreeMemory(device_, outputDeviceMemory_, nullptr);

  vkDestroyBuffer(device_, outputHostBuffer_, nullptr);
  vkFreeMemory(device_, outputHostMemory_, nullptr);

  vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
  vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
  vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyPipelineCache(device_, pipelineCache_, nullptr);
  // vkDestroyFence(device_, fence_, nullptr);
  vkDestroyCommandPool(device_, commandPool_, nullptr);
  vkDestroyShaderModule(device_, shaderModule_, nullptr);
  vkDestroyDevice(device_, nullptr);
#if DEBUG
  if (debugReportCallback)
  {
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallback =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(instance_,
                                  "vkDestroyDebugReportCallbackEXT"));
    assert(vkDestroyDebugReportCallback);
    vkDestroyDebugReportCallback(instance_, debugReportCallback, nullptr);
  }
#endif
  vkDestroyInstance(instance_, nullptr);
}
