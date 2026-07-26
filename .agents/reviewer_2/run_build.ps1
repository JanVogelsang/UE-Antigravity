$env:uebp_UATMutexNoWait = "1"
$RunUAT = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"
$PluginDir = "C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\AgentFramework\AgentFramework.uplugin"
$OutputDir = "C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\Packaged\AgentFramework"

Write-Host "Running BuildPlugin..."
& $RunUAT BuildPlugin -plugin="$PluginDir" -package="$OutputDir" -Rocket -NoMutex
