# Raytracer in Curved Spacetime

Real-time rendering of light paths near a Schwarzschild black hole, implemented in CUDA and OpenGL (C++). Each pixel traces a null geodesic through the Schwarzschild metric using a 4th-order Runge-Kutta integrator, producing an accretion disk with animated texture.

## Geodesic equation

The equations of motion are derived from the Schwarzschild metric restricted to the equatorial plane. The derivation is documented in this [Overleaf project](https://www.overleaf.com/read/yvtnzvtqjppk#843385).

Each ray is described by three variables — radial position `r`, radial velocity `u = dr/dλ`, and azimuthal angle `φ` — integrated forward in affine parameter `λ` using RK4.

## Demo: 2D and 3D raytracing with Numba

The `demo/` folder contains Python notebooks that build up the raytracer step by step:

- **2D**: traces geodesics in the orbital plane directly.
- **3D**: lifts the 2D result to three dimensions via a coordinate projection. Each pixel defines a unique orbital plane through the black hole, and the 2D integrator is reused within that plane.

Numba JIT acceleration brings each frame down to roughly one minute on CPU.

## Accretion disk texture

The accretion disk occupies the equatorial plane between `r_min` and `r_max` (in units of the Schwarzschild radius). When a geodesic crosses this plane the intersection point is located in global `(r, φ)` coordinates and sampled against a procedural texture.

The texture is a sum of many Gaussians placed randomly in `(r, φ)` space:

- Azimuthal width: σ_φ rad — produces narrow bright strips
- Radial width: σ_r — strips are tightly localised in radius
- A broad radial envelope (σ, centred on the disk midpoint) smoothly fades brightness toward the inner and outer edges

Gaussian centers are generated once at startup with a fixed seed and uploaded to GPU constant memory, so they are free to read from all threads with no latency penalty. The physical time is scaled from wall-clock time so the speed is frame-rate independent.

## 3D raytracing with CUDA

### Prerequisites

Install in this order:

1. [Visual Studio Build Tools](https://visualstudio.microsoft.com/insiders/)
2. [CUDA Toolkit](https://developer.nvidia.com/cuda-downloads?target_os=Windows&target_arch=x86_64&target_version=11&target_type=exe_local)

After installation, add the CMake bundled with Visual Studio to `PATH`:

```
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
```

CUDA paths are added to `PATH` automatically by the installer.

### Build

```powershell
Remove-Item -Recurse -Force build
cmake -B build
cmake --build build --config Release
```

The executable is written to `build\Release\raytracer.exe`. Press `Esc` to quit.
