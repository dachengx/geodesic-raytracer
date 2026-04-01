#include "raytracer.cuh"
#include <cuda_runtime.h>
#include <math.h>

static constexpr int   kBlockDimX      = 32;
static constexpr int   kBlockDimY      = 32;
static constexpr int   kBlurRadius     = 1;
static constexpr float kSpeedScale     = 1.0f; // manual scale factor for orbital speed
static constexpr float kSigmaR         = 0.05f;
static constexpr float kSigmaPhiLeft   = 0.5f; // trailing edge (dphi < 0)
static constexpr float kSigmaPhiRight  = 0.1f; // leading edge  (dphi >= 0)
static constexpr float kSigmaREnvelope = 2.0f;
// Precomputed reciprocals — avoids divisions inside the Gaussian loop.
static constexpr float kInvSigmaR2         = 1.0f / (kSigmaR         * kSigmaR        );
static constexpr float kInvSigmaPhiLeft2   = 1.0f / (kSigmaPhiLeft   * kSigmaPhiLeft  );
static constexpr float kInvSigmaPhiRight2  = 1.0f / (kSigmaPhiRight  * kSigmaPhiRight );
static constexpr float kInvSigmaREnv2      = 1.0f / (kSigmaREnvelope * kSigmaREnvelope);

// Gaussian centers in (r, phi) space, uploaded once before rendering.
__constant__ float2 g_gaussian_centers[NUM_GAUSSIANS];
__constant__ float  g_gaussian_speeds[NUM_GAUSSIANS];
__constant__ float  g_gaussian_betas[NUM_GAUSSIANS];
__constant__ float  g_gaussian_gammas[NUM_GAUSSIANS];

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

// ---------------------------------------------------------------------------
// Project 3D pixel position into the 2D (r, phi) initial conditions.
// Each pixel defines a unique orbital plane through the BH.
// Matches get_init_rphi in the notebook exactly.
// ---------------------------------------------------------------------------
__device__ void get_init_rphi(
  int wi, int hi,
  const CameraParams& cam,
  float& r0, float& u0, float& phi0, float& L, float& projection,
  float3& accretion_out
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

  // Find the in-plane basis vector x: trace the ray to z=0, normalize.
  float ex = -xyz.z / dxyzdl.z;
  float3 accretion = normalize3( xyz + ex * dxyzdl );
  if ( accretion.y < 0.0f )
    accretion = (-1.0f) * accretion;
  accretion_out = accretion;

  // Find the in-place basis vector y: perpendicular to basis vector x, normalize.
  float ey = -( xyz.x * accretion.x + xyz.y * accretion.y ) / ( dxyzdl.x * accretion.x + dxyzdl.y * accretion.y );
  float3 vertical = normalize3( xyz + ey * dxyzdl );
  if ( vertical.z < 0.0f )
    vertical = (-1.0f) * vertical;

  // Find the orbital velocity direction of accretion
  float3 orbital = { -accretion.y, accretion.x, 0.0f };
  projection = dot3( vertical, orbital );

  // Project 3D position and direction into 2D (x, y) in this plane.
  float x0_2d  = dot3( xyz, accretion );
  float y0_2d  = sqrtf( fmaxf( 0.0f, dot3( xyz, xyz ) - x0_2d * x0_2d ) );
  if ( xyz.z < 0.0f ) y0_2d = -y0_2d;

  float dxdl0  = dot3( dxyzdl, accretion );
  float dydl0  = sqrtf( fmaxf( 0.0f, 1.0f - dxdl0 * dxdl0 ) );
  if ( dxyzdl.z < 0.0f ) dydl0 = -dydl0;

  // Convert Cartesian 2D -> polar.
  r0   = hypotf( x0_2d, y0_2d );
  phi0 = atan2f( y0_2d, x0_2d ); // in (-pi, pi], may be negative on first step
  float drdl0   = (x0_2d * dxdl0 + y0_2d * dydl0) / r0;
  float dphidl0 = (x0_2d * dydl0 - y0_2d * dxdl0) / (r0 * r0);
  u0 = drdl0;
  L  = r0 * r0 * dphidl0;
}

// ---------------------------------------------------------------------------
// Keplerian orbital speed at radius r around a black hole with Schwarzschild
// radius r_s. Returns v = sqrt(r_s / (2r - 3r_s)), the azimuthal speed in natural units.
// ---------------------------------------------------------------------------
__device__ __host__ inline float orbital_speed( float r, float r_s ) {
  return sqrtf( r_s / (2.0f * r - 3.0f * r_s) );
}

// ---------------------------------------------------------------------------
// Main kernel: one thread per pixel.
// Outputs a float per pixel: 0 = fell into BH, 1 = hit accretion, 0.5 = escaped.
// ---------------------------------------------------------------------------
__launch_bounds__( kBlockDimX * kBlockDimY )
__global__ void raytracer_kernel(
  float* __restrict__ intensity_buffer,
  float* __restrict__ shift_buffer,
  int          width,
  int          height,
  SceneParams  scene,
  CameraParams cam,
  RK4Params    rk4,
  float        t_offset
) {
  int wi = blockIdx.x * blockDim.x + threadIdx.x;
  int hi = blockIdx.y * blockDim.y + threadIdx.y;
  if ( wi >= width || hi >= height ) return;

  float r0, u0, phi0, L, projection;
  float3 accretion;
  get_init_rphi( wi, hi, cam, r0, u0, phi0, L, projection, accretion );

  State y = { r0, u0, phi0 };
  float strength   = 0.0f;
  float intensity  = 0.0f;
  float shift      = 0.0f;
  const float r_mid = 0.5f * ( scene.accretion_r_min + scene.accretion_r_max );

  for ( int i = 0; i < rk4.N; i++ ) {
    State yn = rk4_step( y, rk4.dl, scene.r_s, L );

    float phi0s = y.phi, phi1s = yn.phi;
    bool in_disk = ( y.r  > scene.accretion_r_min && y.r  < scene.accretion_r_max &&
                     yn.r > scene.accretion_r_min && yn.r < scene.accretion_r_max );
    // Phi=0 crossing: first-step case OR wrap 2π→0 (L>0) OR wrap 0→2π (L<0).
    bool cross_zero = fabsf(phi0s - phi1s) > kPI;
    bool cross_pi   = ( (phi0s - kPI) * (phi1s - kPI) < 0.0f );

    if ( in_disk && ( cross_zero || cross_pi ) ) {
      // Midpoint radius at crossing
      float r_hit = 0.5f * ( y.r + yn.r );

      // y-component of the yn→y direction in the 2D disk plane (along orbital axis).
      // Used as cos_theta: angle between photon travel direction and orbital velocity.
      float2 pos_y  = { y.r  * cosf(y.phi),  y.r  * sinf(y.phi)  };
      float2 pos_yn = { yn.r * cosf(yn.phi), yn.r * sinf(yn.phi) };
      float2 d      = { pos_y.x - pos_yn.x,  pos_y.y - pos_yn.y };
      float  cos_theta = d.y / sqrtf( d.x*d.x + d.y*d.y );

      // Global azimuthal angle of the intersection in the accretion disk plane.
      // cross_zero => intersection along +accretion; cross_pi => along -accretion.
      float disk_phi;
      if ( cross_zero ) {
        disk_phi = atan2f(  accretion.y,  accretion.x );
      } else {
        disk_phi = atan2f( -accretion.y, -accretion.x );
        cos_theta *= -1.0f; // The orbital direction should be reversed.
      }
      if ( disk_phi < 0.0f ) disk_phi += 2.0f * kPI;

      // Sum all Gaussian contributions at (r_hit, disk_phi).
      // Gravitational redshift depends only on emission radius, not on which Gaussian.
      float grav_redshift = sqrtf( 1.0f - scene.r_s / r_hit );
      float intensity_cross  = 0.0f;
      float shift_cross      = 0.0f;
      for ( int g = 0; g < NUM_GAUSSIANS; g++ ) {
        float sample_phi = disk_phi - t_offset * g_gaussian_speeds[g];
        float dr   = r_hit - g_gaussian_centers[g].x;
        float dphi = remainderf( sample_phi - g_gaussian_centers[g].y, 2.0f * kPI );
        float inv_sphi2 = dphi < 0.0f ? kInvSigmaPhiLeft2 : kInvSigmaPhiRight2;
        float exponent = -0.5f * ( dr * dr * kInvSigmaR2 + dphi * dphi * inv_sphi2 );

        // Relativistic intensity boost: kinematic Doppler × gravitational redshift
        float delta = grav_redshift / ( g_gaussian_gammas[g] * ( 1 - g_gaussian_betas[g] * projection * cos_theta ));
        float contrib = __expf( exponent ) * delta * delta * delta;

        intensity_cross  += contrib;
        // Relativistic Doppler effect
        shift_cross += contrib * delta;
      }

      float dr_mid = r_hit - r_mid;
      float radial_envelope = __expf( -0.5f * dr_mid * dr_mid * kInvSigmaREnv2 );
      strength   += intensity_cross;
      intensity  += intensity_cross  * radial_envelope;
      shift += shift_cross;
    }
    if ( yn.r <= scene.r_s ) {
      break;
    }
    y = yn;
  }

  int idx = hi * width + wi;
  intensity_buffer [ idx ] = intensity;
  // 1.0 = no shift, <1 = blueshift, >1 = redshift
  shift_buffer[ idx ] = strength > 0.0f ? shift / strength : 1.0f;
}

// ---------------------------------------------------------------------------
// Upload Gaussian centers to constant memory (call once before render loop).
// ---------------------------------------------------------------------------
void upload_gaussians( const float2* centers, float r_s ) {
  float speeds[NUM_GAUSSIANS];
  float betas[NUM_GAUSSIANS];
  float gammas[NUM_GAUSSIANS];
  for ( int i = 0; i < NUM_GAUSSIANS; i++ ) {
    float speed = orbital_speed( centers[i].x, r_s ) * kSpeedScale;
    speeds[i]   = speed / r_s; // angular speed
    betas[i]    = speed;
    gammas[i]   = 1.0f / sqrtf( 1.0f - speed * speed );
  }
  cudaMemcpyToSymbol( g_gaussian_centers, centers, NUM_GAUSSIANS * sizeof( float2 ) );
  cudaMemcpyToSymbol( g_gaussian_speeds,  speeds,  NUM_GAUSSIANS * sizeof( float  ) );
  cudaMemcpyToSymbol( g_gaussian_betas,   betas,   NUM_GAUSSIANS * sizeof( float  ) );
  cudaMemcpyToSymbol( g_gaussian_gammas,  gammas,  NUM_GAUSSIANS * sizeof( float  ) );
}

// ---------------------------------------------------------------------------
// Host-side launch wrapper
// ---------------------------------------------------------------------------
void launch_raytracer(
  float*       d_intensity,
  float*       d_shift,
  int          width,
  int          height,
  SceneParams  scene,
  CameraParams cam,
  RK4Params    rk4,
  float        t_offset
) {
  dim3 block( kBlockDimX, kBlockDimY );
  dim3 grid(
    (width  + kBlockDimX - 1) / kBlockDimX,
    (height + kBlockDimY - 1) / kBlockDimY
  );
  raytracer_kernel<<<grid, block>>>( d_intensity, d_shift, width, height, scene, cam, rk4, t_offset );
}

// ---------------------------------------------------------------------------
// Box blur kernel — shared memory tile avoids redundant global memory reads.
// ---------------------------------------------------------------------------
__launch_bounds__( kBlockDimX * kBlockDimY )
__global__ void blur_kernel(
  const float* __restrict__ input,
  float*       __restrict__ output,
  int width, int height
) {
  constexpr int R      = kBlurRadius;
  constexpr int TILE_W = kBlockDimX + 2 * R;
  constexpr int TILE_H = kBlockDimY + 2 * R;
  __shared__ float tile[ TILE_H ][ TILE_W ];

  const int base_x = blockIdx.x * kBlockDimX - R;
  const int base_y = blockIdx.y * kBlockDimY - R;
  const int flat   = threadIdx.y * kBlockDimX + threadIdx.x;
  const int total  = TILE_W * TILE_H;

  // Cooperatively load the padded tile (handles halo with clamped addressing).
  for ( int i = flat; i < total; i += kBlockDimX * kBlockDimY ) {
    int lx = i % TILE_W;
    int ly = i / TILE_W;
    int gx = max( 0, min( base_x + lx, width  - 1 ) );
    int gy = max( 0, min( base_y + ly, height - 1 ) );
    tile[ ly ][ lx ] = input[ gy * width + gx ];
  }
  __syncthreads();

  int wi = blockIdx.x * kBlockDimX + threadIdx.x;
  int hi = blockIdx.y * kBlockDimY + threadIdx.y;
  if ( wi >= width || hi >= height ) return;

  float sum = 0.0f;
  for ( int dy = -R; dy <= R; dy++ )
    for ( int dx = -R; dx <= R; dx++ )
      sum += tile[ threadIdx.y + R + dy ][ threadIdx.x + R + dx ];

  constexpr float kInvCount = 1.0f / ( (2*R+1) * (2*R+1) );
  output[ hi * width + wi ] = sum * kInvCount;
}

void launch_blur(
  const float* d_input,
  float*       d_output,
  int          width,
  int          height
) {
  dim3 block( kBlockDimX, kBlockDimY );
  dim3 grid(
    ( width  + kBlockDimX - 1 ) / kBlockDimX,
    ( height + kBlockDimY - 1 ) / kBlockDimY
  );
  blur_kernel<<<grid, block>>>( d_input, d_output, width, height );
}
