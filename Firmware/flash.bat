@echo off
setlocal
if exist esptool.exe goto :FINISH
echo Downloading esptool.exe to current directory..
set "PS_CMD=$rel = Invoke-RestMethod -Uri 'https://api.github.com/repos/espressif/esptool/releases' | Where-Object { $_.assets.name -match 'win64' } | Sort-Object { [version]($_.tag_name -replace '[^0-9.]','') } -Descending | Select-Object -First 1; $asset = $rel.assets | Where-Object { $_.name -match 'win64' -and $_.name -match '.zip' } | Select-Object -First 1; if ($asset) { $asset.browser_download_url }"

for /f "usebackq tokens=*" %%a in (`powershell -ExecutionPolicy Bypass -Command "%PS_CMD%"`) do set "URL=%%a"

if "%URL%"=="" (
    echo Error: Could not find esptool.exe on GitHub. Please download it to this directory and run this script again.
    exit /b
)

curl -L -s "%URL%" | tar -xf - --strip-components 1

for %%f in (espefuse.exe esp_rfc2217_server.exe README.md LICENSE) do (
    if exist "%%f" del /f /q "%%f"
)
:FINISH

echo Programming OTGW32 with firmware
.\esptool.exe --port-filter vid=0x303A --port-filter pid=0x1001 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x0 otgw32.bin
:PAUSE
pause