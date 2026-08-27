@echo off
setlocal enabledelayedexpansion
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set "PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe
for /l %%i in (1,1,60) do (
  !CMAKE! --preset x64-Debug-Agent -DCMAKE_MAKE_PROGRAM="!NINJA!" > C:\dev\artifactstudio\configure.log 2>&1
  findstr /c:"Configuring done" C:\dev\artifactstudio\configure.log >nul && (
    echo CONFIGURED at attempt %%i
    goto :configured
  )
  set SHA=
  for /f "tokens=6" %%s in ('findstr /c:"failed to unpack tree object" C:\dev\artifactstudio\out\build\x64-Debug-Agent\vcpkg-manifest-install.log') do set SHA=%%s
  if "!SHA!"=="" for /f "tokens=4" %%s in ('findstr /c:"could not fetch" C:\dev\artifactstudio\out\build\x64-Debug-Agent\vcpkg-manifest-install.log') do set SHA=%%s
  if "!SHA!"=="" for /f "tokens=2 delims=()" %%s in ('findstr /c:"unable to read sha1 file" C:\dev\artifactstudio\out\build\x64-Debug-Agent\vcpkg-manifest-install.log') do set SHA=%%s
  if "!SHA!"=="" (
    echo NO_SHA_FOUND
    goto :done
  )
  echo attempt %%i fetching !SHA!
  git -C C:\vcpkg fetch origin !SHA! >> C:\dev\artifactstudio\vcpkg_fetch.log 2>&1
)
:configured
!CMAKE! --build out\build\x64-Debug-Agent --target ArtifactCore -- -k 0 > C:\dev\artifactstudio\build_core.log 2>&1
echo BUILD_EXITCODE=!errorlevel!
goto :eof
:done
echo DONE_NO_PROGRESS
