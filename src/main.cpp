#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cuda_runtime.h>
#include <stdio.h>

#include "raytracer.cuh"
#include "renderer.h"

// ---------------------------------------------------------------------------
// Scene / camera / integration parameters (mirrors the notebook)
// ---------------------------------------------------------------------------
static const SceneParams kScene = {
  .r_s             = 1.0f,
  .accretion_r_min = 2.0f,
  .accretion_r_max = 6.0f,
};

static const CameraParams kCamera = {
  .pixel_size    = 0.004f,
  .width_pixels  = 1280,
  .height_pixels = 720,
  .d_obs_cam     = 4.0f,
  .d_bh_cam      = 10.0f,
  .x_offset_cam  = 0.0f,
  .y_offset_cam  = 0.0f,
  .z_offset_cam  = 0.5f,
  .x_offset_obs  = 0.0f,
  .y_offset_obs  = 0.0f,
  .z_offset_obs  = 0.0f,
};

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
  // CUDA buffers
  // -------------------------------------------------------------------------
  float* d_framebuffer = NULL;
  cudaMalloc( &d_framebuffer, W * H * sizeof( float ) );

  float* h_framebuffer = NULL;
  cudaMallocHost( &h_framebuffer, W * H * sizeof( float ) );  // pinned for fast DtoH

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

    launch_raytracer( d_framebuffer, W, H, kScene, kCamera, kRK4 );
    cudaDeviceSynchronize();
    cudaMemcpy( h_framebuffer, d_framebuffer, W * H * sizeof( float ), cudaMemcpyDeviceToHost );
    renderer.upload( h_framebuffer );

    glClear( GL_COLOR_BUFFER_BIT );
    renderer.draw();
    glfwSwapBuffers( window );
  }

  // -------------------------------------------------------------------------
  // Cleanup
  // -------------------------------------------------------------------------
  renderer.destroy();
  cudaFreeHost( h_framebuffer );
  cudaFree( d_framebuffer );
  glfwTerminate();
  return 0;
}
