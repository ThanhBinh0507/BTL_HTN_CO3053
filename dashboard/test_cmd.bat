@echo off
chcp 65001 >nul
echo ================================================================
echo    TEST DIEU KHIEN 2 CHIEU & XAC NHAN BAO DONG [MUC 3 & 7.2]
echo ================================================================
echo Lua chon thao tac:
echo   [1] Gui lenh dieu khien: Tat den, Bat quat, Mode Manual [A1-201]
echo   [2] Gui lenh Xac nhan Bao dong [ack_alarm = true cho A1-201]
echo   [3] Chuyen phong A1-201 ve Mode Auto
echo.
set /p opt="Nhap lua chon [1, 2 hoac 3]: "

if "%opt%"=="1" goto :opt1
if "%opt%"=="2" goto :opt2
if "%opt%"=="3" goto :opt3
goto :invalid

:opt1
echo.
echo Dang gui lenh Tat den, Bat quat...
curl.exe -X POST http://localhost:1880/api/cmd -H "Content-Type: application/json" -d "{\"building\":\"A1\",\"floor\":\"2\",\"room\":\"A1-201\",\"light\":\"off\",\"fan\":\"on\",\"mode\":\"manual\"}"
goto :done

:opt2
echo.
echo Dang gui lenh ack_alarm...
curl.exe -X POST http://localhost:1880/api/cmd -H "Content-Type: application/json" -d "{\"building\":\"A1\",\"floor\":\"2\",\"room\":\"A1-201\",\"ack_alarm\":true}"
goto :done

:opt3
echo.
echo Dang chuyen phong A1-201 ve mode Auto...
curl.exe -X POST http://localhost:1880/api/cmd -H "Content-Type: application/json" -d "{\"building\":\"A1\",\"floor\":\"2\",\"room\":\"A1-201\",\"mode\":\"auto\"}"
goto :done

:invalid
echo.
echo Lua chon khong hop le!
goto :done

:done
echo.
echo.
echo Kiem tra log cua simulator de xem phong da nhan duoc lenh hay chua!
pause
