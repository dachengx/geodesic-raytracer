#include "raytracer.cuh"
#include <cuda_runtime.h>
#include <math.h>

static constexpr float kPI = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// float3 operators (not provided by cuda_runtime.h by default)
// ---------------------------------------------------------------------------
__device__ inline float3 operator+( float3 a, float3 b ) { return { a.x+b.x, a.y+b.y, a.z+b.z }; }
__device__ inline float3 operator-( float3 a, float3 b ) { return { a.x-b.x, a.y-b.y, a.z-b.z }; }
__device__ inline float3 operator*( float s, float3 a )  { return { s*a.x, s*a.y, s*a.z }; }
__device__ inline float  dot3( float3 a, float3 b )      { return a.x*b.x + a.y*b.y + a.z*b.z; }
__device__ inline float3 normalize3( float3 a ) {
  float n = rsqrtf( dot3( a, a ) );
  return n * a;
}

// ---------------------------------------------------------------------------
// Geodesic state: y = { r, u = dr/dl, phi }
// Equations of motion in Schwarzschild equatorial plane (from notebook):
//   dr/dl   = u
//   du/dl   = (r - 1.5*r_s) * L^2 / r^4
//   dphi/dl = L / r^2
// ---------------------------------------------------------------------------
struct State { float r, u, phi; };

__device__ inline State operator+( State a, State b ) { return { a.r+b.r, a.u+b.u, a.phi+b.phi }; }
__device__ inline State operator*( float s, State a )  { return { s*a.r, s*a.u, s*a.phi }; }

__device__ State geodesic_f( State y, float r_s, float L ) {
  float r2 = y.r * y.r;
  float r4 = r2 * r2;
  return {
    y.u,
    (y.r - 1.5f * r_s) * (L * L / r4),
    L / r2
  };
}

__device__ State rk4_step( State y, float dl, float r_s, float L ) {
  State k1 = geodesic_f( y, r_s, L );
  State k2 = geodesic_f( y + (0.5f*dl) * k1, r_s, L );
  State k3 = geodesic_f( y + (0.5f*dl) * k2, r_s, L );
  State k4 = geodesic_f( y +       dl  * k3, r_s, L );
  State yp = y + (dl/6.0f) * (k1 + 2.0f*k2 + 2.0f*k3 + k4);
  // wrap phi to [0, 2pi) matching notebook behavior
  yp.phi = fmodf( yp.phi, 2.0f * kPI );
  if ( yp.phi < 0.0f ) yp.phi += 2.0f * kPI;
  return yp;
}

// Returns true when the ray crosses the accretion disk plane (phi = 0 or pi)
// while inside the accretion radii.
__device__ bool across_accretion(
  float r0, float phi0,
  float r1, float phi1,
  float r_min, float r_max
) {
  if ( r0 > r_min && r0 < r_max && r1 > r_min && r1 < r_max ) {
    if ( (phi0 * phi1 < 0.0f) || ((phi0 - kPI) * (phi1 - kPI) < 0.0f) )
      return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Project 3D pixel position into the 2D (r, phi) initial conditions.
// Each pixel defines a unique orbital plane through the BH.
// Matches get_init_rphi in the notebook exactly.
// ---------------------------------------------------------------------------
__device__ void get_init_rphi(
  int wi, int hi,
  const CameraParams& cam,
  float& r0, float& u0, float& phi0, float& L
) {
  // 3D position of pixel on the camera plane
  float3 xyz = {
    -cam.width_pixels  * cam.pixel_size / 2.0f + cam.pixel_size / 2.0f + wi * cam.pixel_size + cam.x_offset_cam,
    -cam.d_bh_cam + cam.y_offset_cam,
    -cam.height_pixels * cam.pixel_size / 2.0f + cam.pixel_size / 2.0f + hi * cam.pixel_size + cam.z_offset_cam
  };
  float3 observer = {
    cam.x_offset_cam + cam.x_offset_obs,
    -(cam.d_obs_cam + cam.d_bh_cam) + cam.y_offset_cam + cam.y_offset_obs,
    cam.z_offset_cam + cam.z_offset_obs
  };
  float3 dxyzdl = normalize3( xyz - observer );

  // Find the in-plane basis vector: trace the ray to z=0, normalize.
  float e = -xyz.z / dxyzdl.z;
  float3 accretion_dir = normalize3( xyz + e * dxyzdl );
  if ( accretion_dir.y < 0.0f )
    accretion_dir = (-1.0f) * accretion_dir;

  // Project 3D position and direction into 2D (x, y) in this plane.
  float x0_2d  = dot3( xyz,    accretion_dir );
  float y0_2d  = sqrtf( fmaxf( 0.0f, dot3( xyz, xyz ) - x0_2d * x0_2d ) );
  if ( xyz.z < 0.0f ) y0_2d = -y0_2d;

  float dxdl0  = dot3( dxyzdl, accretion_dir );
  float dydl0  = sqrtf( fmaxf( 0.0f, 1.0f - dxdl0 * dxdl0 ) );
  if ( dxyzdl.z < 0.0f ) dydl0 = -dydl0;

  // Convert Cartesian 2D -> polar.
  r0   = hypotf( x0_2d, y0_2d );
  phi0 = atan2f( y0_2d, x0_2d );   // in (-pi, pi], may be negative on first step
  float drdl0   = (x0_2d * dxdl0 + y0_2d * dydl0) / r0;
  float dphidl0 = (x0_2d * dydl0 - y0_2d * dxdl0) / (r0 * r0);
  u0 = drdl0;
  L  = r0 * r0 * dphidl0;
}

// ---------------------------------------------------------------------------
// Main kernel: one thread per pixel.
// Outputs a float per pixel: 0 = fell into BH, 1 = hit accretion, 0.5 = escaped.
// ---------------------------------------------------------------------------
__global__ void raytracer_kernel(
  float*       framebuffer,
  int          width,
  int          height,
  SceneParams  scene,
  CameraParams cam,
  RK4Params    rk4
) {
  int wi = blockIdx.x * blockDim.x + threadIdx.x;
  int hi = blockIdx.y * blockDim.y + threadIdx.y;
  if ( wi >= width || hi >= height ) return;

  float r0, u0, phi0, L;
  get_init_rphi( wi, hi, cam, r0, u0, phi0, L );

  State y = { r0, u0, phi0 };
  float value = 0.5f;  // default: escaped to infinity

  for ( int i = 0; i < rk4.N; i++ ) {
    State yn = rk4_step( y, rk4.dl, scene.r_s, L );

    if ( across_accretion( y.r, y.phi, yn.r, yn.phi,
                           scene.accretion_r_min, scene.accretion_r_max ) ) {
      value = 1.0f;
      break;
    }
    if ( yn.r <= scene.r_s ) {
      value = 0.0f;
      break;
    }
    y = yn;
  }

  framebuffer[ hi * width + wi ] = value;
}

// ---------------------------------------------------------------------------
// Host-side launch wrapper
// ---------------------------------------------------------------------------
void launch_raytracer(
  float*       d_framebuffer,
  int          width,
  int          height,
  SceneParams  scene,
  CameraParams cam,
  RK4Params    rk4
) {
  dim3 block( 32, 32 );  // 512 threads/block — better occupancy on Ada Lovelace
  dim3 grid(
    (width  + block.x - 1) / block.x,
    (height + block.y - 1) / block.y
  );
  raytracer_kernel<<<grid, block>>>( d_framebuffer, width, height, scene, cam, rk4 );
}
