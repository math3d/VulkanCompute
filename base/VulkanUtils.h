/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#ifndef VULKAN_UTILS_H_
#define VULKAN_UTILS_H_

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

const int BUFFER_NUMBER = 3;

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

inline VkSamplerCreateInfo samplerCreateInfo() {
  VkSamplerCreateInfo samplerCreateInfo{};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.maxAnisotropy = 1.0f;
  return samplerCreateInfo;
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
  VkFenceCreateInfo fenceInfo =
      vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
  VkFence fence;
  VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));
  // Submit to the queue
  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
  VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

  vkDestroyFence(device, fence, nullptr);

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

uint32_t
getMemoryType(const VkPhysicalDeviceMemoryProperties &deviceMemoryProperties,
              uint32_t typeBits, VkMemoryPropertyFlags properties,
              VkBool32 *memTypeFound = nullptr) {
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

void timeOfDispatch(const VkDevice device, const VkQueryPool &queryPool,
                    float timestampPeriod, uint32_t timestampValidBits) {
  uint64_t rawDeviceTimestamps[2] = {0, 0};
  VK_CHECK_RESULT(vkGetQueryPoolResults(
      device, queryPool, 0, 2, sizeof(rawDeviceTimestamps), rawDeviceTimestamps,
      sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
  // https://gist.github.com/cdwfs/4222ca09cb259f8dd50f7f2cf7d09179
  const uint64_t timestampMask = (timestampValidBits == 64)
                                     ? UINT64_MAX
                                     : ((1ULL << timestampValidBits) - 1);
  const double secondsPerTick = static_cast<double>(timestampPeriod) / 1e6;
  double deviceMs[2] = {0.0, 0.0};
  deviceMs[0] = static_cast<double>(rawDeviceTimestamps[0]) * secondsPerTick;
  deviceMs[1] = static_cast<double>(rawDeviceTimestamps[1]) * secondsPerTick;
  double deviceDiffMs = deviceMs[1] - deviceMs[0];
  LOG("Time for dispatch = %fms\n", deviceDiffMs);
}
#endif