@echo off
title Adaptive Emergency Healthcare Server
echo ===================================================================
echo   Starting Adaptive Emergency Healthcare Server (Dynamic Thread Pool)
echo ===================================================================
echo.
echo Server is starting on port 8080...
echo Open your browser at: http://localhost:8080/
echo.
wsl bash -c "cd '/mnt/d/3rd Year/First Sem/OS/OS CP' && ./bin/healthcare_server 8080 4 16 3"
pause
