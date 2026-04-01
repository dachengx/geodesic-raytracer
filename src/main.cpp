#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "raytracer.cuh"
#include "renderer.h"

// ---------------------------------------------------------------------------
// Scene / camera / integration parameters (mirrors the notebook)
// ---------------------------------------------------------------------------
static const SceneParams kScene = {
  .r_s             = 1.0f, // r_s is 2GM/c^2
  .accretion_r_min = 3.5f,
  .accretion_r_max = 7.5f,
};

static const CameraParams kCamera = {
  .pixel_size    = 0.005f,
  .width_pixels  = 1280,
  .height_pixels = 720,
  .d_obs_cam     = 5.0f,
  .d_bh_cam      = 10.0f,
  .x_offset_cam  = 0.0f,
  .y_offset_cam  = 0.0f,
  .z_offset_cam  = 1.0f,
  .x_offset_obs  = 0.0f,
  .y_offset_obs  = 0.0f,
  .z_offset_obs  = 0.5f,
};

static constexpr float kTimeScale        = 0.5f; // Scales wall-clock seconds to simulation time
static constexpr float kAccretionMargin  = 0.1f;
static constexpr int   kGaussianRandSeed = 42;

static const RK4Params kRK4 = {
  .dl = 0.01f,
  .N  = 2000,
};

static void error_callback( int error, const char* description ) {
  fprintf( stderr, "GLFW error %i: %s\n", error, description );
}

int main( void ) {
  const int W = kCamera.width_pixels;
  const int H = kCamera.height_pixels;

  // -------------------------------------------------------------------------
  // GLFW + OpenGL
  // -------------------------------------------------------------------------
  glfwSetErrorCallback( error_callback );
  if ( !glfwInit() ) {
    fprintf( stderr, "ERROR: could not start GLFW.\n" );
    return 1;
  }

  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
  glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
  glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
  glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

  // Scale up the window so the small framebuffer is visible.
  GLFWwindow* window = glfwCreateWindow(
    W, H,
    "Geodesic Raytracer",
    NULL, NULL
  );
  if ( !window ) {
    fprintf( stderr, "ERROR: could not open window.\n" );
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent( window );
  glfwSwapInterval( 0 ); // The value of 0 means "swap immediately".
  // glfwSwapInterval( 1 ); // Lock to normal refresh rate for your monitor.

  if ( !gladLoadGL( glfwGetProcAddress ) ) {
    fprintf( stderr, "ERROR: failed to initialize OpenGL.\n" );
    return 1;
  }
  printf( "OpenGL %s | Renderer: %s\n",
    glGetString( GL_VERSION ), glGetString( GL_RENDERER ) );

  // -------------------------------------------------------------------------
  // Generate Gaussian centers randomly in (r, phi) and upload to GPU.
  // -------------------------------------------------------------------------
  {
    srand( kGaussianRandSeed );
    float2 centers[NUM_GAUSSIANS];
    for ( int i = 0; i < NUM_GAUSSIANS; i++ ) {
      float t = (float)rand() / (float)RAND_MAX;
      float r_lo = kScene.accretion_r_min + 2.0f * kAccretionMargin;
      float r_hi = kScene.accretion_r_max - 2.0f * kAccretionMargin;
      centers[i].x = r_lo + t * (r_hi - r_lo);
      centers[i].y = 2.0f * kPI * (float)rand() / (float)RAND_MAX;
    }
    upload_gaussians( centers, kScene.r_s );
  }

  // -------------------------------------------------------------------------
  // CUDA buffers
  // -------------------------------------------------------------------------
  float* d_framebuffer = NULL;
  cudaMalloc( &d_framebuffer, W * H * sizeof( float ) );

  float* d_blurred = NULL;
  cudaMalloc( &d_blurred, W * H * sizeof( float ) );

  float* h_framebuffer = NULL;
  cudaMallocHost( &h_framebuffer, W * H * sizeof( float ) ); // pinned for fast DtoH

  float* d_shift = NULL;
  cudaMalloc( &d_shift, W * H * sizeof( float ) );

  float* h_shift = NULL;
  cudaMallocHost( &h_shift, W * H * sizeof( float ) );

  // -------------------------------------------------------------------------
  // OpenGL renderer
  // -------------------------------------------------------------------------
  Renderer renderer = {};
  if ( !renderer.init( W, H ) ) {
    fprintf( stderr, "ERROR: failed to init renderer.\n" );
    return 1;
  }

  // -------------------------------------------------------------------------
  // Display loop
  // -------------------------------------------------------------------------
  float  t_offset = 0.0f;
  double prev_s = glfwGetTime();
  double title_countdown_s = 0.1;
  while ( !glfwWindowShouldClose( window ) ) {
    double curr_s    = glfwGetTime();
    double elapsed_s = curr_s - prev_s;
    prev_s           = curr_s;

    title_countdown_s -= elapsed_s;
    if ( title_countdown_s <= 0.0 && elapsed_s > 0.0 ) {
      char tmp[256];
      sprintf( tmp, "Geodesic Raytracer | FPS %.2f", 1.0 / elapsed_s );
      glfwSetWindowTitle( window, tmp );
      title_countdown_s = 0.1;
    }

    glfwPollEvents();
    if ( glfwGetKey( window, GLFW_KEY_ESCAPE ) == GLFW_PRESS )
      glfwSetWindowShouldClose( window, 1 );

    t_offset += kTimeScale * (float)elapsed_s;
    launch_raytracer( d_framebuffer, d_shift, W, H, kScene, kCamera, kRK4, t_offset );
    launch_blur( d_framebuffer, d_blurred, W, H );
    cudaDeviceSynchronize();
    cudaMemcpy( h_framebuffer, d_blurred, W * H * sizeof( float ), cudaMemcpyDeviceToHost );
    cudaMemcpy( h_shift, d_shift, W * H * sizeof( float ), cudaMemcpyDeviceToHost );
    renderer.upload( h_framebuffer, h_shift );

    glClear( GL_COLOR_BUFFER_BIT );
    renderer.draw();
    glfwSwapBuffers( window );
  }

  // -------------------------------------------------------------------------
  // Cleanup
  // -------------------------------------------------------------------------
  renderer.destroy();
  cudaFreeHost( h_framebuffer );
  cudaFreeHost( h_shift );
  cudaFree( d_framebuffer );
  cudaFree( d_blurred );
  cudaFree( d_shift );
  glfwTerminate();
  return 0;
}
