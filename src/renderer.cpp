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
uniform sampler2D tex;
void main() {
    float v = texture(tex, uv).r;
    frag_color = vec4(v, v, v, 1.0);
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

  // Grayscale texture (single float channel)
  glGenTextures( 1, &texture );
  glBindTexture( GL_TEXTURE_2D, texture );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, NULL );
  glBindTexture( GL_TEXTURE_2D, 0 );

  return true;
}

void Renderer::upload( const float* data ) {
  glBindTexture( GL_TEXTURE_2D, texture );
  glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, data );
  glBindTexture( GL_TEXTURE_2D, 0 );
}

void Renderer::draw() {
  glUseProgram( shader_program );
  glActiveTexture( GL_TEXTURE0 );
  glBindTexture( GL_TEXTURE_2D, texture );
  glUniform1i( glGetUniformLocation( shader_program, "tex" ), 0 );
  glBindVertexArray( vao );
  glDrawArrays( GL_TRIANGLES, 0, 6 );
  glBindVertexArray( 0 );
}

void Renderer::destroy() {
  glDeleteProgram( shader_program );
  glDeleteTextures( 1, &texture );
  glDeleteBuffers( 1, &vbo );
  glDeleteVertexArrays( 1, &vao );
}
