@echo off
chcp 65001 >nul
mode con cols=80 lines=40
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" "%~dp0check_board.py" %*
pause
