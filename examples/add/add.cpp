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

#include "ComputeOp.h"

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
  std::generate(computeInput.begin(), computeInput.end(), [&n] { return (DATA_TYPE)n++; });

  uint32_t m = 0;
  std::generate(computeFilter.begin(), computeFilter.end(),
                [&m] { return m++; });
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

  params.shader_path = "shaders/add/add_float.comp.spv";

  ComputeOp *computeOp = new ComputeOp(params);
  computeOp->execute();
  computeOp->summary();
  delete (computeOp);
  return 0;
}
#endif
