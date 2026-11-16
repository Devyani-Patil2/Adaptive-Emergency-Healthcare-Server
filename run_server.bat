@echo off
title Adaptive Emergency Healthcare Server
echo ===================================================================
echo   Starting Adaptive Emergency Healthcare Server (Dynamic Thread Pool)
echo ===================================================================
echo.
echo Freeing port 8080 and starting server...
echo Open your browser at: http://localhost:8080/
echo.
wsl bash -c "cd '/mnt/d/3rd Year/First Sem/OS/OS CP' && fuser -k 8080/tcp 2>/dev/null || true && ./bin/healthcare_server 8080 4 16 4"
pause
