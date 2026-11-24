@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo Dang dung toan bo he thong Smart School...
docker-compose down
echo He thong da dung.
pause

