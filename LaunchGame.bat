@echo off
title Avvio Istante Standalone GRILL DIS

set UE_EDITOR_PATH="D:\EpicGames\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set PROJECT_PATH="%cd%\TankGame.uproject"

echo Launching game for Player 1 (Tank ID 1)...
start "" %UE_EDITOR_PATH% %PROJECT_PATH% -game -log -windowed -WinX=0 -WinY=0 -resx=960 -resy=540 -nosound -ExecCmds="r.Streaming.PoolSize 500,sg.TextureQuality 1,sg.ShadowQuality 1" ?PlayerID=1 

:: Waits 3 seconds to let the first process open UDP port correctly
timeout /t 3 /nobreak > nul

echo Launching game for Player 2 (Tank ID 2)...
start "" %UE_EDITOR_PATH% %PROJECT_PATH% -game -log -windowed -WinX=0 -WinY=600 -resx=960 -resy=540 -nosound -ExecCmds="r.Streaming.PoolSize 500,sg.TextureQuality 1,sg.ShadowQuality 1" ?PlayerID=2

echo Both game instances started!