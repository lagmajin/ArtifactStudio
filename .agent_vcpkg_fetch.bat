@echo off
for /l %%i in (1,1,30) do (
  git -C C:\vcpkg rev-parse --is-shallow-repository > C:\dev\artifactstudio\shallow.txt 2>&1
  findstr /c:"false" C:\dev\artifactstudio\shallow.txt >nul && goto :done
  echo === attempt %%i === >> C:\dev\artifactstudio\vcpkg_fetch.log
  git -C C:\vcpkg fetch --unshallow origin >> C:\dev\artifactstudio\vcpkg_fetch.log 2>&1
  timeout /t 5 >nul
)
:done
git -C C:\vcpkg rev-parse --is-shallow-repository > C:\dev\artifactstudio\shallow.txt 2>&1
echo FETCH_LOOP_DONE
