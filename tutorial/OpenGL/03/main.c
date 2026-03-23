#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <assert.h>

void error_callback_glfw(int error, const char* description) {
  fprintf( stderr, "GLFW ERROR: code %i msg: %s.\n", error, description );
}

int main( void ) {
  printf( "Starting GLFW %s.\n", glfwGetVersionString() );

  // Register the error callback function that we wrote earlier.
  glfwSetErrorCallback( error_callback_glfw );

	// Start GLFW.
  if( !glfwInit() ) {
    fprintf( stderr, "ERROR: could not start GLFW3.\n" );
    return 1;
  }

  // Request an OpenGL 4.1, core, context from GLFW.
  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
  glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
  glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

  glfwWindowHint( GLFW_SAMPLES, 8 );

  // Set this to false to go back to windowed mode.
  bool full_screen = false; // NB. include stdbool.h for bool in C.

  GLFWmonitor *mon = NULL;
  int win_w = 800, win_h = 600; // Our window dimensions, in pixels.

  if ( full_screen ) {
    mon = glfwGetPrimaryMonitor();

    const GLFWvidmode* mode = glfwGetVideoMode( mon );

    // Hinting these properties lets us use "borderless full screen" mode.
    glfwWindowHint( GLFW_RED_BITS, mode->redBits );
    glfwWindowHint( GLFW_GREEN_BITS, mode->greenBits );
    glfwWindowHint( GLFW_BLUE_BITS, mode->blueBits );
    glfwWindowHint( GLFW_REFRESH_RATE, mode->refreshRate );

    win_w = mode->width;  // Use our 'desktop' resolution for window size
    win_h = mode->height; // to get a 'full screen borderless' window.
  }

  GLFWwindow *window = glfwCreateWindow(
    win_w,
    win_h,
    "Extended OpenGL Init",
    mon,
    NULL
  );
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

  "in vec3 vertex_position;"
  "uniform float time;"      // uniform is a keyword in GLSL
  "out vec3 pos;"            // <-- pos now declared here

  "void main() {"
  // "  vec3 pos = vertex_position;"
  "  pos = vertex_position;" // <-- removed declaration
  "  pos.y += sin( time );"

  "  gl_Position = vec4( pos, 1.0 );"
  "}";

  const char* fragment_shader =
  "#version 410 core\n"

  "in vec3 pos;"         // <-- match name from vertex shader.
  "uniform float time;"
  "out vec4 frag_colour;"

  "void main() {"
  // "  float wild = sin( time ) * 0.5 + 0.5;"
  // "  frag_colour.rba = vec3( 1.0 );"
  // "  frag_colour.g = wild;"
  "  frag_colour = vec4( pos, 1.0 );"
  "}";

  int params = -1;
  // This is our existing vertex shader compilation code.
  GLuint vs = glCreateShader( GL_VERTEX_SHADER );
  glShaderSource( vs, 1, &vertex_shader, NULL );
  glCompileShader( vs );

  // After glCompileShader check for errors.
  glGetShaderiv( vs, GL_COMPILE_STATUS, &params );

  // On error, capture the log and print it.
  if ( GL_TRUE != params ) {
    int max_length    = 2048, actual_length = 0;
    char slog[2048];
    glGetShaderInfoLog( vs, max_length, &actual_length, slog );
    fprintf( stderr, "ERROR: Shader index %u did not compile.\n%s\n", vs, slog );
    return 1;
  }

  // This is our existing vertex shader compilation code.
  GLuint fs = glCreateShader( GL_FRAGMENT_SHADER );
  glShaderSource( fs, 1, &fragment_shader, NULL );
  glCompileShader( fs );

  // After glCompileShader check for errors.
  glGetShaderiv( fs, GL_COMPILE_STATUS, &params );

  // On error, capture the log and print it.
  if ( GL_TRUE != params ) {
    int max_length    = 2048, actual_length = 0;
    char slog[2048];
    glGetShaderInfoLog( fs, max_length, &actual_length, slog );
    fprintf( stderr, "ERROR: Shader index %u did not compile.\n%s\n", fs, slog );
    return 1;
  }

  GLuint shader_program = glCreateProgram();
  glAttachShader( shader_program, fs );
  
  glAttachShader( shader_program, vs );
  glLinkProgram( shader_program );

  // Delete delete a shader program to recycle resources
  glDeleteShader( fs );
  glDeleteShader( vs );

  // Check for linking errors:
  glGetProgramiv( shader_program, GL_LINK_STATUS, &params );

  // Print the linking log:
  if ( GL_TRUE != params ) {    
    int max_length    = 2048, actual_length = 0;
    char plog[2048];
    glGetProgramInfoLog( shader_program, max_length, &actual_length, plog );
    fprintf( stderr, "ERROR: Could not link shader program GL index %u.\n%s\n", shader_program, plog );
    return 1;
  }

  // Get the location of the "time" uniform in the shader program.
  int time_loc = glGetUniformLocation( shader_program, "time" );
  assert( time_loc > -1 );

  double prev_s = glfwGetTime();  // Set the initial 'previous time'.
  double title_countdown_s = 0.1;
  while ( !glfwWindowShouldClose( window ) ) {
    double curr_s     = glfwGetTime();   // Get the current time.
    double elapsed_s  = curr_s - prev_s; // Work out the time elapsed over the last frame.
    prev_s            = curr_s;          // Set the 'previous time' for the next frame to use.

    // Print the FPS, but not every frame, so it doesn't flicker too much.
    title_countdown_s -= elapsed_s;
    if ( title_countdown_s <= 0.0 && elapsed_s > 0.0 ) {
      double fps        = 1.0 / elapsed_s;

      // Create a string and put the FPS as the window title.
      char tmp[256];
      sprintf( tmp, "FPS %.2lf", fps );
      glfwSetWindowTitle(window, tmp );
      title_countdown_s = 0.1;
    }

    glfwPollEvents();
    if ( GLFW_PRESS == glfwGetKey( window, GLFW_KEY_ESCAPE ) ) {
			glfwSetWindowShouldClose( window, 1 );
		}

		// Check if the window resized.
    glfwGetWindowSize( window, &win_w, &win_h );
    // Update the viewport (drawing area) to fill the window dimensions.
		glViewport( 0, 0, win_w, win_h );

    // Wipe the drawing surface clear.
    glClearColor( 0.6f, 0.6f, 0.8f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glUseProgram( shader_program );
    // Update the "time" uniform with the current time each frame.
    glUniform1f( time_loc, (float)curr_s );
    glBindVertexArray( vao );

    glDrawArrays( GL_TRIANGLES, 0, 3 );

    // Put the stuff we've been drawing onto the visible area.
    glfwSwapBuffers( window );

    // glfwSwapInterval( 0 ); // The value of 0 means "swap immediately".
    glfwSwapInterval( 1 ); // Lock to normal refresh rate for your monitor.
  }
  
  // Close OpenGL window, context, and any other GLFW resources.
  glfwTerminate();
  return 0;
}
