#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <assert.h>

void error_callback_glfw(int error, const char* description) {
  fprintf( stderr, "GLFW ERROR: code %i msg: %s.\n", error, description );
}

// Compile and link a shader program from two source strings.
// Returns the program handle, or 0 on failure.
GLuint create_shader_program_from_strings(
  const char* vertex_shader_str,
  const char* fragment_shader_str ) {

  int params = -1;

  GLuint vs = glCreateShader( GL_VERTEX_SHADER );
  glShaderSource( vs, 1, &vertex_shader_str, NULL );
  glCompileShader( vs );
  glGetShaderiv( vs, GL_COMPILE_STATUS, &params );
  if ( GL_TRUE != params ) {
    int max_length = 2048, actual_length = 0;
    char slog[2048];
    glGetShaderInfoLog( vs, max_length, &actual_length, slog );
    fprintf( stderr, "ERROR: Vertex shader did not compile.\n%s\n", slog );
    return 0;
  }

  GLuint fs = glCreateShader( GL_FRAGMENT_SHADER );
  glShaderSource( fs, 1, &fragment_shader_str, NULL );
  glCompileShader( fs );
  glGetShaderiv( fs, GL_COMPILE_STATUS, &params );
  if ( GL_TRUE != params ) {
    int max_length = 2048, actual_length = 0;
    char slog[2048];
    glGetShaderInfoLog( fs, max_length, &actual_length, slog );
    fprintf( stderr, "ERROR: Fragment shader did not compile.\n%s\n", slog );
    return 0;
  }

  GLuint program = glCreateProgram();
  glAttachShader( program, vs );
  glAttachShader( program, fs );
  glLinkProgram( program );
  glDeleteShader( vs );
  glDeleteShader( fs );
  glGetProgramiv( program, GL_LINK_STATUS, &params );
  if ( GL_TRUE != params ) {
    int max_length = 2048, actual_length = 0;
    char plog[2048];
    glGetProgramInfoLog( program, max_length, &actual_length, plog );
    fprintf( stderr, "ERROR: Could not link shader program.\n%s\n", plog );
    return 0;
  }

  return program;
}

#define MAX_SHADER_SZ 100000

// Load shader sources from files and compile them into a program.
GLuint create_shader_program_from_files(
  const char* vertex_shader_filename,
  const char* fragment_shader_filename ) {

  char vs_shader_str[MAX_SHADER_SZ];
  char fs_shader_str[MAX_SHADER_SZ];
  vs_shader_str[0] = fs_shader_str[0] = '\0';

  FILE* fp = fopen( vertex_shader_filename, "r" );
  if ( !fp ) {
    fprintf( stderr, "ERROR: could not open vertex shader file `%s`\n", vertex_shader_filename );
    return 0;
  }
  size_t count = fread( vs_shader_str, 1, MAX_SHADER_SZ - 1, fp );
  assert( count < MAX_SHADER_SZ - 1 );
  vs_shader_str[count] = '\0';
  fclose( fp );

  fp = fopen( fragment_shader_filename, "r" );
  if ( !fp ) {
    fprintf( stderr, "ERROR: could not open fragment shader file `%s`\n", fragment_shader_filename );
    return 0;
  }
  count = fread( fs_shader_str, 1, MAX_SHADER_SZ - 1, fp );
  assert( count < MAX_SHADER_SZ - 1 );
  fs_shader_str[count] = '\0';
  fclose( fp );

  return create_shader_program_from_strings( vs_shader_str, fs_shader_str );
}

// Reload a shader program from files. Only replaces the program if compilation succeeds.
void reload_shader_program_from_files(
  GLuint* program,
  const char* vertex_shader_filename,
  const char* fragment_shader_filename ) {

  assert( program && vertex_shader_filename && fragment_shader_filename );

  GLuint reloaded_program = create_shader_program_from_files(
    vertex_shader_filename, fragment_shader_filename );

  if ( reloaded_program ) {
    glDeleteProgram( *program );
    *program = reloaded_program;
  }
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

  // Load and compile shaders from external files.
  GLuint shader_program = create_shader_program_from_files( "myshader.vert", "myshader.frag" );
  if ( !shader_program ) { return 1; }

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
    // Press R to hot-reload shaders from files without restarting.
    if ( GLFW_PRESS == glfwGetKey( window, GLFW_KEY_R ) ) {
      reload_shader_program_from_files( &shader_program, "myshader.vert", "myshader.frag" );
    }

		// Check if the window resized.
    glfwGetWindowSize( window, &win_w, &win_h );
    // Update the viewport (drawing area) to fill the window dimensions.
		glViewport( 0, 0, win_w, win_h );

    // Wipe the drawing surface clear.
    glClearColor( 0.6f, 0.6f, 0.8f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glUseProgram( shader_program );
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
