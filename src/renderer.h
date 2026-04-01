#pragma once
#include <glad/gl.h>

// Renders an RGB framebuffer to the window via a fullscreen quad.
struct Renderer {
  GLuint vao;
  GLuint vbo;
  GLuint texture_color;
  GLuint shader_program;
  int    width;
  int    height;

  // Create GL objects. Must be called after a valid GL context is current.
  bool init( int width, int height );

  // Upload RGB buffer (width*height*3 floats, row-major, interleaved RGB).
  void upload( const float* rgb );

  // Draw the fullscreen quad.
  void draw();

  void destroy();
};
