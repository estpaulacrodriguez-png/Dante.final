@echo off
cd /d "%~dp0"
if exist imgui\imgui.cpp (
  echo Dear ImGui ya esta instalado.
  pause
  exit /b
)
echo Descargando Dear ImGui v1.91.5 ...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri 'https://github.com/ocornut/imgui/archive/refs/tags/v1.91.5.zip' -OutFile imgui.zip; Expand-Archive -Path imgui.zip -DestinationPath imgui_tmp -Force; Move-Item imgui_tmp\imgui-1.91.5 imgui; Remove-Item imgui.zip; Remove-Item imgui_tmp -Recurse -Force"
if exist imgui\imgui.cpp (
  echo.
  echo Listo. Abre DanteViewer.sln y presiona F5.
) else (
  echo.
  echo No se pudo descargar. Revisa tu conexion a internet.
)
pause
