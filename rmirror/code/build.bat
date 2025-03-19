@ECHO off

SET CompilerFlags=-nologo -O2 -Oi -favor:INTEL64 -EHa -fsanitize=address -Fmwin32_rmirror.map -Zi -FC -fp:fast -I R:\code\ -Gm- -GR- -WX -WL
REM -P to output preprocessor
SET LinkerFlags=-incremental:no -opt:ref

IF NOT EXIST build mkdir build

pushd build

cl.exe %CompilerFlags% ..\rmirror\code\win32_rmirror.cpp /link %LinkerFlags%

popd
