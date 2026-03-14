@echo off
set IDF_PATH=D:\esp\v5.5.2\esp-idf
set IDF_TOOLS_PATH=C:\Espressif\tools
set IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v5.5.2\venv
set ESP_IDF_VERSION=5.5

:: Clear MSYS/MinGW environment inherited from Git Bash
set MSYSTEM=
set MSYS=
set MINGW_PREFIX=
set CHERE_INVOKING=
set TERM=

set PATH=C:\Espressif\tools\python\v5.5.2\venv\Scripts;C:\Espressif\tools\idf-exe\1.0.3;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin;C:\Espressif\tools\riscv32-esp-elf-gdb\16.3_20250913\riscv32-esp-elf-gdb\bin;C:\Espressif\tools\ccache\4.11.2;C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20250707\openocd-esp32\bin;C:\Espressif\tools\esp-clang\esp-19.1.2_20250312\esp-clang\bin;%IDF_PATH%\tools;C:\Windows\system32;C:\Windows

cd /d %~dp0
python %IDF_PATH%\tools\idf.py build
