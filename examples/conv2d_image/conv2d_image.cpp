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

#include "ComputeImageOp.h"


#define DEBUG (!NDEBUG)


#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void android_main(android_app *state) { android_realmain(state); }
#else

int main() {
  ComputeOp::InitParams params;
  params.inputWidth = 4;
  params.inputHeight = 8;
  params.filterWidth = 3;
  params.filterHeight = 3;
  params.outputWidth = params.inputWidth - params.filterWidth + 1;
  params.outputHeight = params.inputHeight - params.filterHeight + 1;
  params.DISPATCH_X = params.inputWidth - params.filterWidth + 1;
  params.DISPATCH_Y = params.inputHeight - params.filterHeight + 1;
  params.DISPATCH_Z = 1;
  int BUFFER_ELEMENTS = params.inputWidth * params.inputHeight;
  // TODO: fix me. If not *4,will crash.
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

  params.shader_path = "shaders/conv2d_image.comp.spv";
  params.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  // params.format = VK_FORMAT_R32_SFLOAT;

  ComputeOp *computeOp = new ComputeImageOp(params);
  computeOp->execute();
  computeOp->summary();
  delete (computeOp);
  return 0;
}
#endif
