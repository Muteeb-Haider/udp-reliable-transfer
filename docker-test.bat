@echo off
echo Testing UDP Reliable Transfer System...
echo =====================================

REM Test file transfer using Docker
echo Sending test file through Docker...
docker-compose run --rm udp-client

echo.
echo Test completed!
echo Check the server_data directory for received files.
pause
