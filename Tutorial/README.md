This folder is brief record of learning OpenGL and general relativity.

The OpenGL is learnt from https://antongerdelan.net/opengl.

The software is installed mostly via MSYS2 UCRT64 and ran in PowerShell.

# Environment

## Install MSYS2

Following instructions of https://www.msys2.org

## Install mingw-w64

Start MSYS2 UCRT64, then

```
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-make
```

or

```
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain
```

Note `mingw32-make` but not `make` will be installed.

## Modify PATH

Add `C:\msys64\ucrt64\bin` to PATH. Test with minimum gcc compiling.

## Glad headers generation

Go to https://gen.glad.sh. Select "API: gl Version 4.1" + "Compatibility/Core profile: Core". Download the generated zip file and extract to this repo.

## Install GLFW

Start MSYS2 UCRT64, then

```
pacman -S --needed mingw-w64-ucrt-x86_64-glfw
```

# Build executive file in each project folder

```
gcc main.c glad/src/gl.c -o app.exe -I glad/include -lglfw3
```
