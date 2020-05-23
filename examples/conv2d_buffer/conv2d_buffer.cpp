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
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "CommandLineParser.h"
#include "ComputeBufferOp.h"
#define USE_TIME

#ifdef USE_TIME
#include "Utils.h"
#include <time.h>
#else
#define TIME
#endif

#define DEBUG (!NDEBUG)

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void android_main(android_app *state) { android_realmain(state); }
#else
int main(int argc, char **argv) {
  CommandLineParser cmdLine(argc, argv);
  ComputeOp::InitParams params;

  const int width = cmdLine.getWidth();
  const int height = cmdLine.getHeight();
  const int WORKGROUPSIZE_X = cmdLine.getWorkgroupSizeX();
  const int WORKGROUPSIZE_Y = cmdLine.getWorkgroupSizeY();
  const int WORKGROUPSIZE_Z = cmdLine.getWorkgroupSizeZ();

  params.inputWidth = width;
  params.inputHeight = height;
  params.filterWidth = 3;
  params.filterHeight = 3;

/*
  ComputeOp::InitParams params;
  params.inputWidth = width;
  params.inputHeight = height;
  params.filterWidth = width;
  params.filterHeight = height;
  params.outputWidth = width;
  params.outputHeight = height;
  params.DISPATCH_X = ceil((float)width / WORKGROUPSIZE_X);
  params.DISPATCH_Y = ceil((float)height / WORKGROUPSIZE_Y);
  params.DISPATCH_Z = 1;
  params.WORKGROUPSIZE_X = WORKGROUPSIZE_X;
  params.WORKGROUPSIZE_Y = WORKGROUPSIZE_Y;
  params.WORKGROUPSIZE_Z = WORKGROUPSIZE_Z;
*/
#if 1
  // Core segmentation fault.
  params.outputWidth = params.inputWidth - params.filterWidth + 1;
  params.outputHeight = params.inputHeight - params.filterHeight + 1;
  params.DISPATCH_X = params.inputWidth - params.filterWidth + 1;
  params.DISPATCH_Y = params.inputHeight - params.filterHeight + 1;
#endif
#if 0
  params.outputWidth = params.inputWidth;
  params.outputHeight = params.inputHeight;
  params.DISPATCH_X = params.inputWidth;
  params.DISPATCH_Y = params.inputHeight;
#endif
  params.DISPATCH_Z = 1;
  int BUFFER_ELEMENTS = params.inputWidth * params.inputHeight;
  std::vector<DATA_TYPE> computeInput(BUFFER_ELEMENTS);
  BUFFER_ELEMENTS = params.filterWidth * params.filterHeight;
  std::vector<DATA_TYPE> computeFilter(BUFFER_ELEMENTS);
  BUFFER_ELEMENTS = params.outputWidth * params.outputHeight;
  std::vector<DATA_TYPE> computeOutput(BUFFER_ELEMENTS);
  // Fill input data
  uint32_t n = 0;
  std::generate(computeInput.begin(), computeInput.end(),
                [&n] { return (DATA_TYPE)n++; });

  uint32_t m = 0;
  std::generate(computeFilter.begin(), computeFilter.end(),
                [&m] { return (DATA_TYPE)m++; });
  params.computeInput = computeInput;
  params.computeFilter = computeFilter;
  params.computeOutput = computeOutput;

  params.shader_path = "shaders/conv2d_buffer.comp.spv";

  ComputeOp *computeOp = new ComputeBufferOp(params);
  computeOp->execute();
  computeOp->summary();
  delete (computeOp);
  return 0;
}
#endif
