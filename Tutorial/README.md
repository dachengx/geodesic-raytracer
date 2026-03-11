This folder is brief record of learning OpenGL and general relativity.

The OpenGL is learnt from https://antongerdelan.net/opengl.

# Compiler installation

```
sudo apt install build-essential
```

# Glad headers generation

Go to https://gen.glad.sh. Select "API: gl Version 4.1" + "Compatibility/Core profile: Core". Download the generated zip file and extract to this repo.

# GLFW installation

```
sudo apt install libglfw3-dev
```

# Build executive file in each project folder

```
gcc main.c glad/src/gl.c -o a.out -I glad/include -lglfw
```
