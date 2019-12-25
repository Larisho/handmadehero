@echo off

pushd %cd%

where cl >nul 2>nul
if %errorlevel% NEQ 0 call init_env

popd

REM /link -subsystem:windows,5.1 to 32bit build to run on XP
set commonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4211 -wd4100 -wd4189 -wd4505 -D_CRT_SECURE_NO_WARNINGS=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7

set commonLinkerFlags=-incremental:no -opt:ref user32.lib Gdi32.lib winmm.lib

REM cl %commonCompilerFlags% %USERPROFILE%\sourceCode\\"Handmade Hero"\code\win32_handmade.cpp /link -subsystem:windows,5.1 %commonLinkerFlags%

del *.pdb > NUL 2> NUL

cl %commonCompilerFlags% %USERPROFILE%\sourceCode\\"Handmade Hero"\code\handmade.cpp -Fmhandmade.map -LD /link -incremental:no -opt:ref /PDB:handmade_%random%.pdb -EXPORT:gameUpdateAndRender -EXPORT:gameGetSoundSamples
cl %commonCompilerFlags% %USERPROFILE%\sourceCode\\"Handmade Hero"\code\win32_handmade.cpp -Fmwin32_handmade.map /link %commonLinkerFlags%
