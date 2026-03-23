#pragma once
#include <glad/gl.h>

// Renders a float grayscale framebuffer to the window via a fullscreen quad.
struct Renderer {
  GLuint vao;
  GLuint vbo;
  GLuint texture;
  GLuint shader_program;
  int    width;
  int    height;

  // Create GL objects. Must be called after a valid GL context is current.
  bool init( int width, int height );

  // Upload width*height floats (row-major, origin bottom-left) to the texture.
  void upload( const float* data );

  // Draw the fullscreen quad.
  void draw();

  void destroy();
};
