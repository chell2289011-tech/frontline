@echo off
chcp 65001 >nul
setlocal
rem === Збірка Frontline 2022 -> dist\frontline.exe (потрібен MSYS2: https://www.msys2.org) ===
set MSYS=C:\msys64
if not exist "%MSYS%\usr\bin\bash.exe" (
  echo MSYS2 не знайдено в %MSYS%. Встанови його з https://www.msys2.org і запусти цей файл ще раз.
  pause & exit /b 1
)
set MSYSTEM=MINGW64
set CHERE_INVOKING=1
"%MSYS%\usr\bin\bash.exe" -lc "pacman -S --needed --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-pkgconf mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf && cmake -S . -B build -G Ninja && cmake --build build && rm -rf dist && mkdir dist && cp build/frontline.exe dist/ && cp -r assets dist/ && ldd build/frontline.exe | grep -i mingw64 | awk '{print $3}' | xargs -I{} cp {} dist/"
if errorlevel 1 ( echo ПОМИЛКА збірки & pause & exit /b 1 )
echo.
echo Готово! Запускай: dist\frontline.exe
pause
