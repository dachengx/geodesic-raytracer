#pragma once
#include <glad/gl.h>

// Renders a float grayscale framebuffer to the window via a fullscreen quad.
struct Renderer {
  GLuint vao;
  GLuint vbo;
  GLuint texture_intensity;
  GLuint texture_shift;
  GLuint shader_program;
  int    width;
  int    height;

  // Create GL objects. Must be called after a valid GL context is current.
  bool init( int width, int height );

  // Upload intensity and wavelength buffers (width*height floats each, row-major).
  void upload( const float* intensity, const float* shift );

  // Draw the fullscreen quad.
  void draw();

  void destroy();
};
