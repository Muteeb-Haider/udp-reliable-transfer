@echo off
echo Starting UDP Reliable Transfer System with Docker...
echo ==================================================

REM Check if Docker is running
docker version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Docker is not running or not installed
    echo Please start Docker Desktop and try again
    pause
    exit /b 1
)

REM Create necessary directories
if not exist "server_data" mkdir server_data
if not exist "sample_data" mkdir sample_data

REM Build and start the system
echo Building Docker image...
docker-compose build

echo Starting services...
docker-compose up -d

echo.
echo ==================================================
echo System is starting up...
echo.
echo Web UI will be available at: http://localhost:5000
echo UDP Server will be available on port: 9000
echo.
echo To view logs: docker-compose logs -f
echo To stop: docker-compose down
echo ==================================================

REM Wait a moment and open browser
timeout /t 5 /nobreak >nul
start http://localhost:5000

echo.
echo Press any key to view logs or Ctrl+C to exit...
pause >nul
docker-compose logs -f
