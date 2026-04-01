#include "renderer.h"
#include <stdio.h>

static const char* kVertSrc = R"(
#version 410 core
layout(location = 0) in vec2 pos;
out vec2 uv;
void main() {
    uv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

static const char* kFragSrc = R"(
#version 410 core
in vec2 uv;
out vec4 frag_color;
uniform sampler2D tex_intensity;
uniform sampler2D tex_wavelength;

// Gaussian fit to CIE 1931 color matching functions.
// lambda in nanometers, visible range ~380-780 nm.
vec3 wavelength_to_rgb(float lambda) {
    float r = exp(-pow((lambda - 602.0) / 75.0, 2.0));
    float g = exp(-pow((lambda - 537.0) / 75.0, 2.0));
    float b = exp(-pow((lambda - 447.0) / 40.0, 2.0));
    return clamp(vec3(r, g, b), 0.0, 1.0);
}

void main() {
    float v      = texture(tex_intensity,  uv).r;
    float lambda = texture(tex_wavelength, uv).r;
    frag_color = vec4(v * wavelength_to_rgb(lambda), 1.0);
}
)";

static GLuint compile_shader( GLenum type, const char* src ) {
  GLuint s = glCreateShader( type );
  glShaderSource( s, 1, &src, NULL );
  glCompileShader( s );
  int ok = 0;
  glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
  if ( !ok ) {
    char log[512];
    glGetShaderInfoLog( s, 512, NULL, log );
    fprintf( stderr, "Shader compile error: %s\n", log );
    return 0;
  }
  return s;
}

bool Renderer::init( int w, int h ) {
  width  = w;
  height = h;

  // Shader program
  GLuint vs = compile_shader( GL_VERTEX_SHADER,   kVertSrc );
  GLuint fs = compile_shader( GL_FRAGMENT_SHADER, kFragSrc );
  if ( !vs || !fs ) return false;

  shader_program = glCreateProgram();
  glAttachShader( shader_program, vs );
  glAttachShader( shader_program, fs );
  glLinkProgram( shader_program );
  glDeleteShader( vs );
  glDeleteShader( fs );

  int ok = 0;
  glGetProgramiv( shader_program, GL_LINK_STATUS, &ok );
  if ( !ok ) {
    char log[512];
    glGetProgramInfoLog( shader_program, 512, NULL, log );
    fprintf( stderr, "Program link error: %s\n", log );
    return false;
  }

  // Fullscreen quad: two triangles covering [-1,1]x[-1,1]
  float verts[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f,
    -1.0f, -1.0f,
     1.0f,  1.0f,
    -1.0f,  1.0f,
  };

  glGenVertexArrays( 1, &vao );
  glGenBuffers( 1, &vbo );
  glBindVertexArray( vao );
  glBindBuffer( GL_ARRAY_BUFFER, vbo );
  glBufferData( GL_ARRAY_BUFFER, sizeof( verts ), verts, GL_STATIC_DRAW );
  glEnableVertexAttribArray( 0 );
  glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, NULL );
  glBindVertexArray( 0 );

  auto make_tex = [&]( GLuint& tex ) {
    glGenTextures( 1, &tex );
    glBindTexture( GL_TEXTURE_2D, tex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, NULL );
    glBindTexture( GL_TEXTURE_2D, 0 );
  };
  make_tex( texture_intensity );
  make_tex( texture_wavelength );

  return true;
}

void Renderer::upload( const float* intensity, const float* wavelength ) {
  auto upload_tex = []( GLuint tex, int w, int h, const float* data ) {
    glBindTexture( GL_TEXTURE_2D, tex );
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_FLOAT, data );
    glBindTexture( GL_TEXTURE_2D, 0 );
  };
  upload_tex( texture_intensity,  width, height, intensity  );
  upload_tex( texture_wavelength, width, height, wavelength );
}

void Renderer::draw() {
  glUseProgram( shader_program );
  glActiveTexture( GL_TEXTURE0 );
  glBindTexture( GL_TEXTURE_2D, texture_intensity );
  glUniform1i( glGetUniformLocation( shader_program, "tex_intensity" ), 0 );
  glActiveTexture( GL_TEXTURE1 );
  glBindTexture( GL_TEXTURE_2D, texture_wavelength );
  glUniform1i( glGetUniformLocation( shader_program, "tex_wavelength" ), 1 );
  glBindVertexArray( vao );
  glDrawArrays( GL_TRIANGLES, 0, 6 );
  glBindVertexArray( 0 );
}

void Renderer::destroy() {
  glDeleteProgram( shader_program );
  glDeleteTextures( 1, &texture_intensity );
  glDeleteTextures( 1, &texture_wavelength );
  glDeleteBuffers( 1, &vbo );
  glDeleteVertexArrays( 1, &vao );
}
