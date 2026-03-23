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
  .pixel_size    = 0.025f,
  .width_pixels  = 160,
  .height_pixels = 120,
  .d_obs_cam     = 4.0f,
  .d_bh_cam      = 10.0f,
  .z_offset_cam  = 0.5f,
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
  const int scale  = 6;
  GLFWwindow* window = glfwCreateWindow(
    W * scale, H * scale,
    "Geodesic Raytracer",
    NULL, NULL
  );
  if ( !window ) {
    fprintf( stderr, "ERROR: could not open window.\n" );
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent( window );
  glfwSwapInterval( 1 );

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
  // Render once (static scene — CUDA finishes in milliseconds)
  // -------------------------------------------------------------------------
  launch_raytracer( d_framebuffer, W, H, kScene, kCamera, kRK4 );
  cudaDeviceSynchronize();
  cudaMemcpy( h_framebuffer, d_framebuffer, W * H * sizeof( float ), cudaMemcpyDeviceToHost );
  renderer.upload( h_framebuffer );

  // -------------------------------------------------------------------------
  // Display loop
  // -------------------------------------------------------------------------
  while ( !glfwWindowShouldClose( window ) ) {
    glfwPollEvents();
    if ( glfwGetKey( window, GLFW_KEY_ESCAPE ) == GLFW_PRESS )
      glfwSetWindowShouldClose( window, 1 );

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
