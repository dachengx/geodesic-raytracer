#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stdio.h>

int main( void ) {
  // Start OpenGL context and OS window using the GLFW helper library.
  if ( !glfwInit() ) {
    fprintf( stderr, "ERROR: could not start GLFW3.\n" );
    return 1;
  }

  // Request an OpenGL 4.1, core, context from GLFW.
  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
  glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
  glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

  // Create a window on the operating system, then tie the OpenGL context to it.
  GLFWwindow* window = glfwCreateWindow( 800, 600, "Hello Triangle", NULL, NULL );
  if ( !window ) {
    fprintf( stderr, "ERROR: Could not open window with GLFW3.\n" );
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent( window );
                                  
  // Start Glad, so we can call OpenGL functions.
  int version_glad = gladLoadGL( glfwGetProcAddress );
  if ( version_glad == 0 ) {
    fprintf( stderr, "ERROR: Failed to initialize OpenGL context.\n" );
    return 1;
  }
  printf( "Loaded OpenGL %i.%i\n", GLAD_VERSION_MAJOR( version_glad ), GLAD_VERSION_MINOR( version_glad ) );

  // Try to call some OpenGL functions, and print some more version info.
  printf( "Renderer: %s.\n", glGetString( GL_RENDERER ) );
  printf( "OpenGL version supported %s.\n", glGetString( GL_VERSION ) );

  /* OTHER STUFF GOES HERE NEXT */

  float points[] = {
    0.0f,  0.5f,  0.0f, // x,y,z of first point.
    0.5f, -0.5f,  0.0f, // x,y,z of second point.
    -0.5f, -0.5f,  0.0f  // x,y,z of third point.
  };

  GLuint vbo = 0;
  glGenBuffers( 1, &vbo );
  glBindBuffer( GL_ARRAY_BUFFER, vbo );
  glBufferData( GL_ARRAY_BUFFER, 9 * sizeof( float ), points, GL_STATIC_DRAW );

  GLuint vao = 0;
  glGenVertexArrays( 1, &vao );
  glBindVertexArray( vao );
  glEnableVertexAttribArray( 0 );
  glBindBuffer( GL_ARRAY_BUFFER, vbo );
  glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, NULL );

  const char* vertex_shader =
  "#version 410 core\n"
  "in vec3 vp;"
  "void main() {"
  "  gl_Position = vec4( vp, 1.0 );"
  "}";

  const char* fragment_shader =
  "#version 410 core\n"
  "out vec4 frag_colour;"
  "void main() {"
  "  frag_colour = vec4( 0.5, 0.0, 0.5, 1.0 );"
  "}";

  GLuint vs = glCreateShader( GL_VERTEX_SHADER );
  glShaderSource( vs, 1, &vertex_shader, NULL );
  glCompileShader( vs );
  GLuint fs = glCreateShader( GL_FRAGMENT_SHADER );
  glShaderSource( fs, 1, &fragment_shader, NULL );
  glCompileShader( fs );

  GLuint shader_program = glCreateProgram();
  glAttachShader( shader_program, fs );
  glAttachShader( shader_program, vs );
  glLinkProgram( shader_program );

  while ( !glfwWindowShouldClose( window ) ) {
    // Update window events.
    glfwPollEvents();
    
    // Wipe the drawing surface clear.
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    // Put the shader program, and the VAO, in focus in OpenGL's state machine.
    glUseProgram( shader_program );
    glBindVertexArray( vao );

    // Draw points 0-3 from the currently bound VAO with current in-use shader.
    glDrawArrays( GL_TRIANGLES, 0, 3 );
    
    // Put the stuff we've been drawing onto the visible area.
    glfwSwapBuffers( window );
  }
  
  // Close OpenGL window, context, and any other GLFW resources.
  glfwTerminate();
  return 0;
}
