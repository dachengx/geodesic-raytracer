# Raytracer in Curved Spacetime

This repo real-time-renders lights near Schwarzschild black hole using CUDA and OpenGL in C++.

# Derivation of geodesic equation

The derivation of geodesic equation in two dimensional space is in [overleaf project](https://www.overleaf.com/read/yvtnzvtqjppk#843385).

# Demo of 2D and 3D raytracing using numba

In the folder `demo`, raytracing is demonstrated first in two-dimensional space. Then by simple coordination transformation, three-dimensional raytracing is also achieved. By numba acceleration, each frame needs about one minute.

# 3D raytracing using CUDA

## Prerequisite installation:

* [Visual Studio Build Tools](https://visualstudio.microsoft.com/insiders/)
* [CUDA Toolkit](https://developer.nvidia.com/cuda-downloads?target_os=Windows&target_arch=x86_64&target_version=11&target_type=exe_local)

Make sure Visual Studio is installed first then CUDA. Add `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` to `PATH`. The CUDA related path are automatically added to `PATH` when installation.

## Build executable file

```
cmake -B build
```

then

```
cmake --build build --config Release
```

The executable file is at `build\Release\raytracer.exe`.
