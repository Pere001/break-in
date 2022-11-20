@echo off
pushd ..\build
call python -m http.server 8080
popd