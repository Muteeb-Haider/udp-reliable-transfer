@echo off
setlocal enabledelayedexpansion

if "%1"=="build" goto build
if "%1"=="run-server" goto run-server
if "%1"=="run-client" goto run-client
if "%1"=="test" goto test
if "%1"=="test-basic" goto test-basic
if "%1"=="test-transfer" goto test-transfer
if "%1"=="test-error" goto test-error
if "%1"=="clean" goto clean
if "%1"=="compose-up" goto compose-up

echo Usage: build.bat [build^|run-server^|run-client^|test^|test-basic^|test-transfer^|test-error^|clean^|compose-up]
exit /b 1

:build
echo Building project...
cmake -B build -S .
cmake --build build --config Release
goto end

:run-server
echo Starting server...
build\bin\server.exe --port 9000 --out .\server_data
goto end

:run-client
echo Running client...
build\bin\client.exe --host 127.0.0.1 --port 9000 --file .\sample_data\sample.bin
goto end

:test
echo Running all tests...
python tests\robot\run_tests.py --install-requirements --output-dir .\test_results
goto end

:test-basic
echo Running basic tests...
python tests\robot\run_tests.py --tags basic --output-dir .\test_results
goto end

:test-transfer
echo Running transfer tests...
python tests\robot\run_tests.py --tags transfer --output-dir .\test_results
goto end

:test-error
echo Running error tests...
python tests\robot\run_tests.py --tags error --output-dir .\test_results
goto end

:clean
echo Cleaning build artifacts...
if exist build rmdir /s /q build
if exist server_data\* del /q server_data\*
if exist test_results\* del /q test_results\*
goto end

:compose-up
echo Starting Docker Compose...
docker compose up --remove-orphans
goto end

:end
endlocal
