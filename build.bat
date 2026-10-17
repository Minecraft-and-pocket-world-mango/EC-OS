@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ========================================
echo   EC-OS Build Script
echo   EasyCloud Studio
echo ========================================
echo.

set "PY_PATH="
set /p PY_PATH="Please enter your Python path (or press Enter for default): "
if "%PY_PATH%"=="" set "PY_PATH=python"

set "NASM_PATH="
set /p NASM_PATH="Please enter your NASM path (or press Enter for default): "
if "%NASM_PATH%"=="" set "NASM_PATH=nasm"

set "GCC_PATH="
set /p GCC_PATH="Please enter your i686-GCC path (or press Enter for default): "
if "%GCC_PATH%"=="" set "GCC_PATH=i686-elf-gcc"

set "OBJCOPY_PATH=i686-elf-objcopy"
if not "%GCC_PATH%"=="i686-elf-gcc" (
    set "OBJCOPY_PATH=%GCC_PATH:gcc=objcopy%"
)

where /q "%PY_PATH%" 2>nul
if errorlevel 1 (
    if not exist "%PY_PATH%" (
        echo [ERROR] Python not found at: %PY_PATH%
        pause & exit /b 1
    )
)

where /q "%NASM_PATH%" 2>nul
if errorlevel 1 (
    if not exist "%NASM_PATH%" (
        echo [ERROR] NASM not found at: %NASM_PATH%
        pause & exit /b 1
    )
)

where /q "%GCC_PATH%" 2>nul
if errorlevel 1 (
    if not exist "%GCC_PATH%" (
        echo [ERROR] i686-GCC not found at: %GCC_PATH%
        pause & exit /b 1
    )
)

if not exist build mkdir build

if "%RES_HIGH%"=="" set "RES_HIGH=0"
set "CFLAGS=-ffreestanding -nostdlib -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -m32 -mno-sse -mno-mmx -mno-80387 -Wall -Wextra -O2 -I system -DRES_HIGH=%RES_HIGH%"

if "%RES_HIGH%"=="1" (
    set "NSW=1920"
    set "NSH=1080"
) else (
    set "NSW=1280"
    set "NSH=1024"
)

set "SRCS=lib vbe vga keyboard fs shell ecdos gfx font font16x32 mouse window afm file_explorer minidos desktop settings i18n icon_data filetype open text_edit idt ata rename_dialog"

echo.
echo [1/6] Assembling boot sector...
"%NASM_PATH%" -f bin boot\boot.asm -DKRES_W=%NSW% -DKRES_H=%NSH% -o build\boot.bin
if errorlevel 1 goto :error

echo [2/6] Assembling kernel entry...
"%NASM_PATH%" -f elf32 kernel\kernel_entry.asm -o build\kernel_entry.o
if errorlevel 1 goto :error

echo [3/6] Generating 16x32 font...
"%PY_PATH%" tools\gen_font.py
if errorlevel 1 goto :error

echo [4/6] Generating cursor sprite from cursor.png...
"%PY_PATH%" tools\png2cursor.py
if errorlevel 1 goto :error

echo [4b/6] Generating icon sprite from file.png...
"%PY_PATH%" tools\png2icon.py
if errorlevel 1 goto :error

echo [5/6] Compiling kernel sources...
set "OBJS=build\kernel_entry.o"
for %%S in (%SRCS%) do (
    echo     cc %%S.c
    "%GCC_PATH%" %CFLAGS% -c system\%%S.c -o build\%%S.o
    if errorlevel 1 goto :error
    set "OBJS=!OBJS! build\%%S.o"
)

echo [6/6] Linking kernel.elf...
"%GCC_PATH%" -T system\linker.ld -nostdlib -Wl,--build-id=none -o build\kernel.elf !OBJS! -lgcc
if errorlevel 1 goto :error

echo [objcopy] Generating kernel.bin...
"%OBJCOPY_PATH%" -O binary build\kernel.elf build\kernel.bin
if errorlevel 1 goto :error

echo [pack] Packing disk + floppy images...
"%PY_PATH%" tools\pack.py
if errorlevel 1 goto :error

echo [pack] Building bootable ISO (FOR DISTRIBUTION ONLY!)...
"%PY_PATH%" tools\pack.py --iso
if errorlevel 1 goto :error

echo [vmdk] Converting disk image...
"%PY_PATH%" img2vmdk.py
if errorlevel 1 goto :error

echo.
echo ========================================
echo   Build complete!
echo ========================================
echo   Resolution: %NSW% x %NSH% x 32bpp (RES_HIGH=%RES_HIGH%)
echo.
echo **************************************************
echo   !!! CRITICAL WARNING !!!
echo   DO NOT BOOT FROM ec-os.iso !!!
echo   You MUST boot from build\disk.vmdk to test persistence.
echo **************************************************
echo.
pause
goto :eof

:error
echo.
echo [FAILED] errorlevel=%errorlevel%
pause
exit /b 1