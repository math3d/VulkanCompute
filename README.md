# Vulkan C++ 示例

## 代码的获取和编译

示例的[Vulkan源代码](https://github.com/math3d/Vulkan)，是基于SaschaWillems开源的[示例程序](https://github.com/SaschaWillems/Vulkan)修改而来。可以运行在Ubuntu和Windows环境（调整Makefile应该可以运行于Android，但是未经验证）。

Vulkan源码的获得：
```
$git clone https://github.com/math3d/Vulkan.git
$git submodule init
$git submodule update
```

Vulkan源码的编译-Ubuntu 18.04 （编译之前，需要先去https://www.lunarg.com/vulkan-sdk/ 安装Vulkan SDK）：
```
$cmake CMakeLists.txt 
$make
```

Vulkan源码的编译-Windows 10（编译之前，需要先去https://www.lunarg.com/vulkan-sdk/ 安装Vulkan SDK）：
```
$cmake -G "Visual Studio 15 2017 Win64"
```
用Visual Studio 打开项目vulkanExamples.sln，进行编译。


Submodule information
```
git submodule add https://github.com/g-truc/glm external/glm external/glm
cd external/glm
git checkout -b 1ad55c5016339b83b7eec98c31007e0aee57d2bf

git submodule add https://github.com/KhronosGroup/KTX-Software external/ktx
cd  external/ktx
git checkout -b 726d14d02c95bb21ec9e43807751b491d295dd3c
```