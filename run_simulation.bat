@echo off
title Healthcare Client Traffic Simulator
echo ===================================================================
echo   Running Healthcare Client Traffic Simulator
echo ===================================================================
echo.
wsl bash -c "cd '/mnt/d/VIT/3rd year/Sem I/OS/Adaptive-Emergency-Healthcare-Server' && ./bin/client_simulator"
pause
