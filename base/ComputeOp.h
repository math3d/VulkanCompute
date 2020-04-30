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

const int mipLevels = 1;

// TODO:
// 1. Move InitParams into ComputeOp.
// 2. Template the input and output, not all the functions.
typedef float DATA_TYPE;
const int DATA_TYPE_ID = 0;

class ComputeOp {
public:
  struct InitParams {
    InitParams();
    InitParams(const InitParams &other);
    std::vector<DATA_TYPE> computeInput;
    std::vector<DATA_TYPE> computeFilter;
    std::vector<DATA_TYPE> computeOutput;
    int inputWidth = 32;
    int inputHeight = 1;
    int filterWidth = 32;
    int filterHeight = 1;
    int outputWidth = 32;
    int outputHeight = 1;
    int DISPATCH_X = 32;
    int DISPATCH_Y = 1;
    int DISPATCH_Z = 1;
    std::string shader_path;
    VkFormat format = VK_FORMAT_R32_SFLOAT;
  };

  VkInstance instance_;
  VkPhysicalDevice physicalDevice_;
  VkPhysicalDeviceProperties deviceProperties_;
  VkPhysicalDeviceMemoryProperties deviceMemoryProperties_;
  VkDevice device_;
  uint32_t queueFamilyIndex_;
  VkPipelineCache pipelineCache_;
  VkQueue queue_;
  VkCommandPool commandPool_;
  VkCommandBuffer commandBuffer_;
  VkFence fence_;
  VkDescriptorPool descriptorPool_;
  VkDescriptorSetLayout descriptorSetLayout_;
  VkDescriptorSet descriptorSet_;
  VkPipelineLayout pipelineLayout_;
  VkPipeline pipeline_;
  VkShaderModule shaderModule_;
  InitParams params_;
  VkBuffer deviceBuffer_, hostBuffer_;
  VkDeviceMemory deviceMemory_, hostMemory_;
  VkFormat imageFormat_; // VK_FORMAT_R32_SFLOAT;
  // VK_FORMAT_R32G32B32A32_SFLOAT;
  // VK_FORMAT_R32G32B32A32_SFLOAT; //
  // VK_FORMAT_R8G8B8A8_UINT;//VK_FORMAT_R8G8B8_UINT;//VK_FORMAT_R8G8B8A8_UNORM;

  VkImage image_;
  VkImage filterImage_;
  VkSampler sampler_;
  VkSampler filterSampler_;
  VkImageView view_;
  VkImageView filterView_;
  VkImageLayout imageLayout_ = VK_IMAGE_LAYOUT_GENERAL;
  VkImageLayout filterImageLayout_ = VK_IMAGE_LAYOUT_GENERAL;

  VkBuffer filterDeviceBuffer_, filterHostBuffer_;
  VkDeviceMemory filterDeviceMemory_, filterHostMemory_;

  VkBuffer outputDeviceBuffer_, outputHostBuffer_;
  VkDeviceMemory outputDeviceMemory_, outputHostMemory_;

  VkImage outputImage_;
  VkSampler outputImageSampler_;
  VkImageView outputImageView_;
  VkImageLayout outputImageLayout_ = VK_IMAGE_LAYOUT_GENERAL;
  VkDeviceMemory outputImageDeviceMemory_;

  VkQueryPool queryPool_;

  VkDebugReportCallbackEXT debugReportCallback{};

  VkResult createBufferWithData(VkBufferUsageFlags usageFlags,
                                VkMemoryPropertyFlags memoryPropertyFlags,
                                VkBuffer *buffer, VkDeviceMemory *memory,
                                VkDeviceSize size, void *data = nullptr);
  VkResult createDeviceImage(VkImage &image, const int width, const int height);
  VkResult createSampler(VkImage &image, VkSampler &sampler, VkImageView &view);
  VkResult copyBufferHostToDevice(VkBuffer &deviceBuffer, VkBuffer &hostBuffer,
                                  const VkDeviceSize &bufferSize);

  VkResult copyHostBufferToDeviceImage(VkImage &image, VkBuffer &hostBuffer,
                                       const uint32_t width,
                                       const uint32_t height);
  VkResult copyDeviceImageToHostBuffer(VkImage &image,
                                       const VkDeviceSize &bufferSize,
                                       const uint32_t width,
                                       const uint32_t height);
  VkResult copyDeviceBufferToHostBuffer(VkBuffer &deviceBuffer,
                                        const VkDeviceSize &bufferSize,
                                        const uint32_t width,
                                        const uint32_t height);
  VkResult prepareComputeCommandBuffer(VkBuffer &outputDeviceBuffer,
                                       VkBuffer &outputHostBuffer,
                                       VkDeviceMemory &outputHostMemory,
                                       const VkDeviceSize &bufferSize);
  VkResult prepareComputeImageToImageCommandBuffer();

  VkResult prepareBufferToBufferPipeline(VkBuffer &deviceBuffer,
                                         VkBuffer &filterDeviceBuffer,
                                         VkBuffer &outputDeviceBuffer);
  VkResult prepareImageToBufferPipeline(VkBuffer &deviceBuffer,
                                        VkBuffer &filterDeviceBuffer,
                                        VkBuffer &outputDeviceBuffer);
  VkResult prepareImageToImagePipeline();

  VkResult prepareTextureTarget(uint32_t width, uint32_t height,
                                VkFormat format);
  VkResult prepareDebugLayer();
  VkResult prepareDevice();
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
