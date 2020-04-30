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

  std::vector<DATA_TYPE> computeInput(BUFFER_ELEMENTS);
  std::vector<DATA_TYPE> computeFilter(BUFFER_ELEMENTS);
  std::vector<DATA_TYPE> computeOutput(BUFFER_ELEMENTS);
  // Fill input data
  uint32_t n = 0;
  uint32_t start = 0x3f800000;
  std::generate(computeInput.begin(), computeInput.end(), [&n] { return 5.0+(DATA_TYPE)n++; });

  uint32_t m = 0;
  std::generate(computeFilter.begin(), computeFilter.end(),
                [&m] {return 2.0+(float) m++;});
  params.computeInput = computeInput;
  params.computeFilter = computeFilter;
  params.computeOutput = computeOutput;
  params.inputWidth = 32;
  params.inputHeight = 1;
  params.filterWidth = 32;
  params.filterHeight = 1;
  params.outputWidth = 32;
  params.outputHeight = 1;
  params.DISPATCH_X = 32;
  params.DISPATCH_Y = 1;
  params.DISPATCH_Z = 1;

  params.shader_path = "shaders/add_image/add_image.comp.spv";
  params.format = VK_FORMAT_R32G32B32A32_SFLOAT;

  ComputeOp *computeOp = new ComputeImageOp(params);
  computeOp->execute();
  computeOp->summary();
  delete (computeOp);
  return 0;
}
#endif
