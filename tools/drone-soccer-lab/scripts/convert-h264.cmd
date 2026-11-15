@echo off
setlocal DisableDelayedExpansion
if "%~1"=="" (
 echo Drag a local video onto this CMD file.
 pause
 exit /b 2
)
if not exist "%~1" (
 echo Input file not found.
 pause
 exit /b 2
)
set "FFMPEG=ffmpeg.exe"
if exist "%~dp0ffmpeg.exe" set "FFMPEG=%~dp0ffmpeg.exe"
"%FFMPEG%" -version >nul 2>&1
if errorlevel 1 (
 echo FFmpeg is not installed. See https://ffmpeg.org/download.html
 echo Put a trusted ffmpeg.exe next to this script, or add it to PATH.
 pause
 exit /b 3
)
set "OUTPUT=%~dpn1_h264.mp4"
if exist "%OUTPUT%" (
 echo Output already exists. Nothing was overwritten.
 pause
 exit /b 4
)
"%FFMPEG%" -n -i "%~1" -map 0:v:0 -map 0:a? -c:v libx264 -preset fast -crf 22 -pix_fmt yuv420p -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:a aac -b:a 128k -movflags +faststart "%OUTPUT%"
if errorlevel 1 (
 echo Conversion failed. The original is unchanged.
 pause
 exit /b 5
)
echo Created: "%OUTPUT%"
echo Start a NEW review project with the converted video.
pause
