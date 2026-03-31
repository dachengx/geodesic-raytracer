#pragma once
#include <cuda_runtime.h>

#define NUM_GAUSSIANS 100

// Upload Gaussian centers (r, phi) to device constant memory before rendering.
void upload_gaussians( const float2* centers );

struct SceneParams {
  float r_s;
  float accretion_r_min;
  float accretion_r_max;
};

struct CameraParams {
  float pixel_size;
  int   width_pixels;
  int   height_pixels;
  float d_obs_cam;
  float d_bh_cam;
  float x_offset_cam;
  float y_offset_cam;
  float z_offset_cam;
  float x_offset_obs;
  float y_offset_obs;
  float z_offset_obs;
};

struct RK4Params {
  float dl;
  int   N;
};

void launch_raytracer(
  float*       d_framebuffer,
  int          width,
  int          height,
  SceneParams  scene,
  CameraParams cam,
  RK4Params    rk4,
  float        phi_offset
);
