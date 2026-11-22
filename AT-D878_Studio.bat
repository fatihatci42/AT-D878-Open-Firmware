@echo off
title AT-D878 Studio Baslatiliyor...
cd /d "%~dp0\desktop\at_d878_studio"
start "" ".venv\Scripts\pythonw.exe" "main.py"
exit
