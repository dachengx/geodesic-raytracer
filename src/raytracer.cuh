#pragma once

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
  float z_offset_cam;
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
  RK4Params    rk4
);
