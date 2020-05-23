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
#include "ComputeImageOp.h"

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
  // NV: works: 32x1; 4x8.
  // HD: works: 32x1; not work: 4x8.
  const int width = cmdLine.getWidth();
  const int height = cmdLine.getHeight();
  const int WORKGROUPSIZE_X = cmdLine.getWorkgroupSizeX();
  const int WORKGROUPSIZE_Y = cmdLine.getWorkgroupSizeY();
  const int WORKGROUPSIZE_Z = cmdLine.getWorkgroupSizeZ();

  ComputeOp::InitParams params;
  // works: 32x1.
  // not wor: 4x8.
  // TODO:not work.
  params.inputWidth = width;
  params.inputHeight = height;
  params.filterWidth = width;
  params.filterHeight = height;
  // TODO: why need to read 2* to make sure all data is read out.
  params.outputWidth = width;
  params.outputHeight = height;
  params.DISPATCH_X = width;
  params.DISPATCH_Y = height;
  params.DISPATCH_Z = 1;
  int BUFFER_ELEMENTS = params.inputWidth * params.inputHeight;
  std::vector<DATA_TYPE> computeInput(BUFFER_ELEMENTS);
  BUFFER_ELEMENTS = params.filterWidth * params.filterHeight;
  std::vector<DATA_TYPE> computeFilter(BUFFER_ELEMENTS);
  BUFFER_ELEMENTS = params.outputWidth * params.outputHeight;
  std::vector<DATA_TYPE> computeOutput(BUFFER_ELEMENTS);
  // Fill input data
  uint32_t n = 0;
  std::generate(computeInput.begin(), computeInput.end(), [&n] { return (DATA_TYPE)n++; });

  uint32_t m = 0;
  std::generate(computeFilter.begin(), computeFilter.end(),
                [&m] { return (DATA_TYPE) m++; });
  params.computeInput = computeInput;
  params.computeFilter = computeFilter;
  params.computeOutput = computeOutput;

  params.shader_path = "shaders/add_imager32f/add_imager32f.comp.spv";
  params.format = VK_FORMAT_R32_SFLOAT;

  ComputeOp *computeOp = new ComputeImageOp(params);
  computeOp->summaryOfInput();
  computeOp->execute();
  computeOp->summary();
  delete (computeOp);
  return 0;
}
#endif
