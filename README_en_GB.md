[English](https://github.com/math3d/VulkanCompute/blob/master/README_en_GB.md) | [简体中文](https://github.com/math3d/VulkanCompute/blob/master/README.md)

# Vulkan Compute C++ Example

The [VulkanCompute](https://github.com/math3d/VulkanCompute) can run in Ubuntu and Windows (After modifying MakeFile, this may also run in Android, but not tested).

## Getting Code and Building

Getting VulkanCompute code：
```
git clone https://github.com/math3d/VulkanCompute.git
git submodule init
git submodule update
```

Compile VulkanCompute on Ubuntu 18.04 (Need https://www.lunarg.com/vulkan-sdk/)：
```
cmake CMakeLists.txt
make
```

Compile VulkanCompute on Windows 10 (Need https://www.lunarg.com/vulkan-sdk/)：
```
cmake -G "Visual Studio 15 2017 Win64"
```
Open VulkanComputeExamples.sln with Visual Studio.


Submodule information
```
git submodule add https://github.com/g-truc/glm external/glm external/glm
cd external/glm
git checkout -b 1ad55c5016339b83b7eec98c31007e0aee57d2bf

git submodule add https://github.com/KhronosGroup/KTX-Software external/ktx
cd  external/ktx
git checkout -b 726d14d02c95bb21ec9e43807751b491d295dd3c
```

## Run
Add (Default input size w = 4, h = 8; Default work group size: wx = 1, wy = 1, wz =1):
```
add -w 128 -h 128 -wx 16 -wy 16
```

## Others
Makefile is based on SaschaWillems [Example](https://github.com/SaschaWillems/Vulkan).