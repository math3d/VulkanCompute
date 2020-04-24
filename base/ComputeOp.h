/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#ifndef COMPUTE_OP_H_
#define COMPUTE_OP_H_

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

#include "VulkanTools.h"
#include <vulkan/vulkan.h>

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
android_app *androidapp;
#endif

#define DEBUG (!NDEBUG)

#define BUFFER_ELEMENTS 32

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#define LOG(...)                                                               \
  ((void)__android_log_print(ANDROID_LOG_INFO, "ComputeOp", __VA_ARGS__))
#else
#define LOG(...) printf(__VA_ARGS__)
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageCallback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
    uint64_t object, size_t location, int32_t messageCode,
    const char *pLayerPrefix, const char *pMessage, void *pUserData) {
  LOG("[VALIDATION]: %s - %s\n", pLayerPrefix, pMessage);
  return VK_FALSE;
}

class ComputeOp {
public:
  struct InitParams {
    InitParams();
    InitParams(const InitParams &other);
    std::vector<uint32_t> computeInput;
    std::vector<uint32_t> computeFilter;
    std::vector<uint32_t> computeOutput;
    std::string shader_path;
  };
  VkInstance instance;
  VkPhysicalDevice physicalDevice;
  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  VkDevice device;
  uint32_t queueFamilyIndex;
  VkPipelineCache pipelineCache;
  VkQueue queue;
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;
  VkFence fence;
  VkDescriptorPool descriptorPool;
  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;
  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;
  VkShaderModule shaderModule;
  InitParams params_;
  VkBuffer deviceBuffer, hostBuffer;
  VkDeviceMemory deviceMemory, hostMemory;

  VkImage image_;
  VkImage filterImage_;
  VkSampler sampler_;
  VkSampler filterSampler_;
  VkImageView view_;
  VkImageView filterView_;
  VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  VkImageLayout filterImageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkBuffer filterDeviceBuffer, filterHostBuffer;
  VkDeviceMemory filterDeviceMemory, filterHostMemory;

  VkBuffer outputDeviceBuffer, outputHostBuffer;
  VkDeviceMemory outputDeviceMemory, outputHostMemory;

  VkDebugReportCallbackEXT debugReportCallback{};

  VkResult createBufferWithData(VkBufferUsageFlags usageFlags,
                                VkMemoryPropertyFlags memoryPropertyFlags,
                                VkBuffer *buffer, VkDeviceMemory *memory,
                                VkDeviceSize size, void *data = nullptr);
  VkResult createImage(VkImage &image);
  VkResult createSampler(VkImage &image, VkSampler &sampler, VkImageView &view);
  VkResult copyBufferHostToDevice(VkBuffer &deviceBuffer, VkBuffer &hostBuffer,
                                  const VkDeviceSize &bufferSize);

  VkResult copyHostBufferToDeviceImage(VkImage &image,
                                       VkBuffer &stagingBuffer);
  VkResult copyDeviceImageToHostBuffer(VkBuffer &stagingBuffer, VkImage &image);
  VkResult copyDeviceBufferToHostBuffer(VkBuffer &hostBuffer,
                                                VkBuffer &deviceBuffer) 
  VkResult prepareComputeCommandBuffer(VkBuffer &outputDeviceBuffer,
                                       VkBuffer &outputHostBuffer,
                                       VkDeviceMemory &outputHostMemory,
                                       const VkDeviceSize &bufferSize);

  VkResult preparePipeline(VkBuffer &deviceBuffer, VkBuffer &filterDeviceBuffer,
                           VkBuffer &outputDeviceBuffer);
  VkResult prepareImagePipeline(VkBuffer &deviceBuffer,
                                    VkBuffer &filterDeviceBuffer,
                                    VkBuffer &outputDeviceBuffer);
  VkResult prepareDebugLayer();
  VkResult prepareDevice();
  uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties,
                         VkBool32 *memTypeFound = nullptr);
  void summary();
  virtual void execute();
  ComputeOp();
  ComputeOp(const InitParams &init_params);

  virtual ~ComputeOp();
};

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
android_app *androidapp;
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void handleAppCommand(android_app *app, int32_t cmd) {
  if (cmd == APP_CMD_INIT_WINDOW) {
    ComputeOp *computeOp = new ComputeOp();
    delete (computeOp);
    ANativeActivity_finish(app->activity);
  }
}
void android_realmain(android_app *state) {
  androidapp = state;
  androidapp->onAppCmd = handleAppCommand;
  int ident, events;
  struct android_poll_source *source;
  while ((ident = ALooper_pollAll(-1, NULL, &events, (void **)&source)) >= 0) {
    if (source != NULL) {
      source->process(androidapp, source);
    }
    if (androidapp->destroyRequested != 0) {
      break;
    }
  }
}
#endif

#endif
