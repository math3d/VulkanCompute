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
#include "ComputeCopyImageOp.h"

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
#if 0
#define BUFFER_ELEMENTS 32
  ComputeOp::InitParams params;
  std::vector<DATA_TYPE> computeInput(BUFFER_ELEMENTS);
  std::vector<DATA_TYPE> computeFilter(BUFFER_ELEMENTS);
  std::vector<DATA_TYPE> computeOutput(BUFFER_ELEMENTS);
#endif
#if 1
  // TODO: understand 4x8. When height is 1, ceil when height/2.
  CommandLineParser cmdLine(argc, argv);
  const int width = cmdLine.getWidth();
  const int height = cmdLine.getHeight();
  const int WORKGROUPSIZE_X = 1;
  const int WORKGROUPSIZE_Y = 1;
  const int WORKGROUPSIZE_Z = 1;

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
  int BUFFER_ELEMENTS = params.inputWidth * params.inputHeight;
  std::vector<DATA_TYPE> computeInput(BUFFER_ELEMENTS);
  BUFFER_ELEMENTS = params.filterWidth * params.filterHeight;
  std::vector<DATA_TYPE> computeFilter(BUFFER_ELEMENTS);
  BUFFER_ELEMENTS = params.outputWidth * params.outputHeight;
  std::vector<DATA_TYPE> computeOutput(BUFFER_ELEMENTS);
#endif
  // Fill input data
  uint32_t n = 0;
  std::generate(computeInput.begin(), computeInput.end(),
                [&n] { return n++; });

  uint32_t m = 0;
  std::generate(computeFilter.begin(), computeFilter.end(),
                [&m] { return m++; });
  params.computeInput = computeInput;
  params.computeFilter = computeFilter;
  params.computeOutput = computeOutput;
  params.shader_path = "shaders/add_image/add_image.comp.spv";
  params.format = VK_FORMAT_R32G32B32A32_SFLOAT;

  ComputeOp *computeOp = new ComputeCopyImageOp(params);
  computeOp->summaryOfInput();

  TIME("execute", computeOp->execute());
  if (width*height < 2000)
    computeOp->summary();
  delete (computeOp);
  return 0;
}
#endif
