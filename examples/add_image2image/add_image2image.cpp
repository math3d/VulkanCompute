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

#include "ComputeImageToImageOp.h"

#define DEBUG (!NDEBUG)

#define BUFFER_ELEMENTS 32

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void android_main(android_app *state) { android_realmain(state); }
#else
int main() {

  ComputeOp::InitParams params;
  std::vector<uint32_t> computeInput(BUFFER_ELEMENTS);
  std::vector<uint32_t> computeFilter(BUFFER_ELEMENTS);
  std::vector<uint32_t> computeOutput(BUFFER_ELEMENTS);
  // Fill input data
  uint32_t n = 0;
  uint32_t start = 0x3f800000;
  std::generate(computeInput.begin(), computeInput.end(), [&n] { return 0x3f800000+n++; });

  uint32_t m = 0;
  std::generate(computeFilter.begin(), computeFilter.end(),
                [&m] { return 0x3f800000 +m++; });
  params.computeInput = computeInput;
  params.computeFilter = computeFilter;
  params.computeOutput = computeOutput;
  params.shader_path = "shaders/add_image2image/add_image2image.comp.spv";

  ComputeOp *computeOp = new ComputeImageToImageOp(params);
  computeOp->execute();
  // computeOp->summary();
  delete (computeOp);
  return 0;
}
#endif
