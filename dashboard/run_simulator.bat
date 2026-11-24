@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo ================================================================
echo    CHAY LOCAL ROOM NODE SIMULATOR (PYTHON)
echo ================================================================
echo Dang ket noi toi localhost:1883 de gui du lieu sensor mau...
echo Nhan Ctrl+C de dung simulator bat cu luc nao.
echo.
python simulator\simulator.py
pause

