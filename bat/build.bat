@echo off

SET SOURCE_MAIN=..\code\bi_main.cpp
SET BUILD_DIR=..\build
SET RAYLIB_LIB=..\lib\libraylib.a
SET INCLUDE_DIR=..\lib\src
SET RESOURCES_DIR=..\resources
SET SHELL_PATH=..\code\my_shell.html

SET WARNING_FLAGS=-Wno-null-dereference -Wno-missing-braces -Wno-unused-variable -Wno-writable-strings 

pushd %BUILD_DIR%

@echo on

call emcc -o game.html %SOURCE_MAIN% -Os -Wall %RAYLIB_LIB% -I. -I%INCLUDE_DIR% -L. -s USE_GLFW=3 -DPLATFORM_WEB --preload-file %RESOURCES_DIR% -s EXPORTED_RUNTIME_METHODS=ccall --shell-file %SHELL_PATH% %WARNING_FLAGS%

@echo off

popd
