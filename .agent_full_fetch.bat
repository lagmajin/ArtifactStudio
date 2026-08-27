@echo off
for /l %%i in (1,1,20) do (
  echo === full fetch attempt %%i === >> C:\dev\artifactstudio\vcpkg_fetch.log
  git -C C:\vcpkg fetch --refetch origin >> C:\dev\artifactstudio\vcpkg_fetch.log 2>&1
  if not errorlevel 1 goto :done
  timeout /t 5 >nul
)
:done
echo FULL_FETCH_DONE >> C:\dev\artifactstudio\vcpkg_fetch.log
