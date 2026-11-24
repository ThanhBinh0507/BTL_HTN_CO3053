@echo off
chcp 65001 >nul
echo ================================================================
echo    KHOI CHAY HE THONG SMART VIRTUAL SCHOOL [LOCAL DOCKER]
echo ================================================================
echo.

cd /d "%~dp0"

echo [1/3] Kiem tra Docker Daemon...
docker info >nul 2>&1
if %ERRORLEVEL% NEQ 0 goto :docker_error

echo [2/3] Dang khoi chay 4 container: Mosquitto, InfluxDB, Node-RED, Grafana...
docker-compose up -d --build
if %ERRORLEVEL% NEQ 0 goto :compose_error

echo.
echo ================================================================
echo    HE THONG DA KHOI CHAY THANH CONG!
echo ================================================================
echo  - Mosquitto MQTT Broker : localhost:1883
echo  - InfluxDB 2.0 Web UI   : http://localhost:8086 [User: admin / Pass: adminpassword123]
echo  - Node-RED Flow Editor  : http://localhost:1880
echo  - Grafana Dashboard     : http://localhost:3000 [User: admin / Pass: admin]
echo ================================================================
echo.
echo Ban co the chay tiep run_simulator.bat de bom du lieu mau vao he thong!
goto :end

:docker_error
echo.
echo [CANH BAO] Docker Desktop chua duoc bat hoac chua san sang!
echo Vui long mo ung dung Docker Desktop tren Windows va cho den khi bieu tuong Docker chuyen sang mau xanh [Engine running].
echo Sau do hay chay lai file nay.
goto :end

:compose_error
echo.
echo [LOI] Khong the khoi dong docker-compose!
goto :end

:end
echo.
pause
