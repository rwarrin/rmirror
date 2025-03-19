@ECHO off

SET WorkDir=%cd:~0,-12%

SUBST R: /D
SUBST R: %WorkDir%

PUSHD R:

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
start gvim.exe

