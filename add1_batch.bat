@echo off
set global_loopcount=10
set width=4225
set height=256
set "para="
set "cmd1=bin\Release\add.exe -w %width% -h %height% %para%"

set "cmd2=bin\Release\add_vec4.exe -w %width% -h %height% %para%"

set "cmd3=bin\Release\add_image.exe -w %width% -h %height% %para%"

set "cmd4=bin\Release\add_imager32f.exe -w %width% -h %height% %para%"

echo %cmd1%
for /l %%x in (1, 1, %global_loopcount%) do (
  %cmd1%
)

echo %cmd2%
for /l %%x in (1, 1, %global_loopcount%) do (
  %cmd2%
)

echo %cmd3%
for /l %%x in (1, 1, %global_loopcount%) do (
  %cmd3%
)

echo %cmd4%
for /l %%x in (1, 1, %global_loopcount%) do (
  %cmd4%
)


