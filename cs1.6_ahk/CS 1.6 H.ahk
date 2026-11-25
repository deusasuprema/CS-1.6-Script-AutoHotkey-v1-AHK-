; <COMPILER: v1.1.37.02>
#NoEnv
#SingleInstance, Force
SetWorkingDir, %A_ScriptDir%
SendMode, Input
SetBatchLines, -1
SetKeyDelay, -1, -1
SetCapsLockState, AlwaysOff
OnExit, AutoSaveOnExit
If FileExist("icone.ico")
Menu, Tray, Icon, icone.ico

CurrentProfile := 1
RecoilAmount := 4
RecoilName := "AK-47"
RecoilDelay := 20
UseScreenShake := True
UseHUD := True
BhopDelay := 20
UseNoSpreadSim := True

global Ativo := False
global AimActive := False
global BhopEnabled := False
global ShotStep := 0
global ConfigDir := A_AppData "\cs16ahk"
global ConfigFile := ConfigDir "\settings.ini"

LoadSettings()
ApplyProfileSettings(CurrentProfile)

Gui, HUD:+LastFound +AlwaysOnTop -Caption +ToolWindow +E0x20
Gui, HUD:Color, 181818
Gui, HUD:Font, s9 bold, Segoe UI
Gui, HUD:Add, Text, vStatusText cFF3333 Center w170, CS 1.6 [OFF]
Gui, HUD:Add, Text, vProfileText cFFAA00 Center w170, [%RecoilName%] NR/NS
Gui, HUD:Add, Text, vBhopText cFF3333 Center w170, Bhop [OFF]
WinSet, Transparent, 180
xPos := A_ScreenWidth - 200
yPos := A_ScreenHeight - 180
if (UseHUD) {
Gui, HUD:Show, x%xPos% y%yPos% NoActivate
}
TrayTip, Status, Script Carregado com Sucesso!, 2, 16
SoundBeep, 800, 150
Return

CanTrigger() {
global Ativo
if (!Ativo)
return False
if (!WinActive("ahk_exe hl.exe") && !WinActive("ahk_exe cstrike.exe") && !WinActive("ahk_exe cs.exe"))
return False
if (A_Cursor = "Arrow" || A_Cursor = "IBeam")
return False
return True
}

ApplyProfileSettings(profile) {
global CurrentProfile, RecoilAmount, RecoilName
if (profile < 1 || profile > 4)
profile := 1
CurrentProfile := profile
if (CurrentProfile = 1) {
RecoilAmount := 4
RecoilName := "AK-47"
} else if (CurrentProfile = 2) {
RecoilAmount := 3
RecoilName := "M4A1"
} else if (CurrentProfile = 3) {
RecoilAmount := 2
RecoilName := "SMG"
} else if (CurrentProfile = 4) {
RecoilAmount := 1
RecoilName := "Pistola"
}
}

LoadSettings() {
global ConfigFile, CurrentProfile, UseHUD, UseScreenShake, RecoilDelay, BhopDelay
IniRead, loadedProfile, %ConfigFile%, Settings, CurrentProfile, %CurrentProfile%
IniRead, loadedHUD, %ConfigFile%, Settings, UseHUD, %UseHUD%
IniRead, loadedShake, %ConfigFile%, Settings, UseScreenShake, %UseScreenShake%
IniRead, loadedRecoilDelay, %ConfigFile%, Settings, RecoilDelay, %RecoilDelay%
IniRead, loadedBhopDelay, %ConfigFile%, Settings, BhopDelay, %BhopDelay%

if loadedProfile is integer
{
if (loadedProfile >= 1 && loadedProfile <= 4)
CurrentProfile := loadedProfile
}
UseHUD := (loadedHUD = 1 || loadedHUD = "1" || loadedHUD = "true" || loadedHUD = "True") ? True : False
UseScreenShake := (loadedShake = 1 || loadedShake = "1" || loadedShake = "true" || loadedShake = "True") ? True : False
if loadedRecoilDelay is integer
{
if (loadedRecoilDelay >= 1 && loadedRecoilDelay <= 500)
RecoilDelay := loadedRecoilDelay
}
if loadedBhopDelay is integer
{
if (loadedBhopDelay >= 1 && loadedBhopDelay <= 500)
BhopDelay := loadedBhopDelay
}
}

SaveSettings() {
global ConfigDir, ConfigFile, CurrentProfile, UseHUD, UseScreenShake, RecoilDelay, BhopDelay
FileCreateDir, %ConfigDir%
hudVal := UseHUD ? 1 : 0
shakeVal := UseScreenShake ? 1 : 0
IniWrite, %CurrentProfile%, %ConfigFile%, Settings, CurrentProfile
IniWrite, %hudVal%, %ConfigFile%, Settings, UseHUD
IniWrite, %shakeVal%, %ConfigFile%, Settings, UseScreenShake
IniWrite, %RecoilDelay%, %ConfigFile%, Settings, RecoilDelay
IniWrite, %BhopDelay%, %ConfigFile%, Settings, BhopDelay
}

GetRecoilStep(profile, step) {
    if (profile = 1) { ; AK-47 - simulacao de padrao com compensacao horizontal
        dy := 4
        if (step <= 4)
            dx := 0
        else if (step <= 8)
            dx := -1
        else if (step <= 12)
            dx := -2
        else if (step <= 16)
            dx := -1
        else if (step <= 20)
            dx := 1
        else if (step <= 24)
            dx := 2
        else if (step <= 28)
            dx := 1
        else
            dx := 0
    } else if (profile = 2) { ; M4A1
        dy := 3
        if (step <= 6)
            dx := 0
        else if (step <= 12)
            dx := -1
        else if (step <= 18)
            dx := 1
        else
            dx := 0
    } else if (profile = 3) { ; SMG
        dy := 2
        if (Mod(step, 6) = 0)
            dx := -1
        else if (Mod(step, 3) = 0)
            dx := 1
        else
            dx := 0
    } else { ; Pistola
        dy := 1
        dx := 0
    }
    return [dx, dy]
}

UpdateHUD() {
global Ativo, UseHUD
if (!UseHUD)
return
if (Ativo) {
Gui, HUD:Font, c33FF33
GuiControl, HUD:Font, StatusText
GuiControl, HUD:, StatusText, CS 1.6 [ON]
} else {
Gui, HUD:Font, cFF3333
GuiControl, HUD:Font, StatusText
GuiControl, HUD:, StatusText, CS 1.6 [OFF]
}
}

UpdateProfileHUD() {
global RecoilName, UseHUD
if (!UseHUD)
return
Gui, HUD:Font, cFFAA00
GuiControl, HUD:Font, ProfileText
GuiControl, HUD:, ProfileText, [%RecoilName%] NR/NS
}

UpdateBhopHUD() {
global BhopEnabled, UseHUD
if (!UseHUD)
return
if (BhopEnabled) {
Gui, HUD:Font, c33FF33
GuiControl, HUD:Font, BhopText
GuiControl, HUD:, BhopText, Bhop [ON]
} else {
Gui, HUD:Font, cFF3333
GuiControl, HUD:Font, BhopText
GuiControl, HUD:, BhopText, Bhop [OFF]
}
}

ReleaseKeys() {
global ShotStep
Send, {LButton up}
Send, {RButton up}
Send, {w up}
Send, {s up}
Send, {a up}
Send, {d up}
Send, {Ctrl up}
SetTimer, ApplyRecoil, Off
ShotStep := 0
Return
}

Insert::
Ativo := !Ativo
BhopEnabled := Ativo
UpdateHUD()
UpdateBhopHUD()
If (Ativo) {
SoundBeep, 1000, 150
TrayTip, Status, Script ATIVADO!, 2, 16
} Else {
SoundBeep, 500, 150
TrayTip, Status, Script DESATIVADO!, 2, 16
AimActive := False
ReleaseKeys()
}
Return

Home::
if (!Ativo)
return
newProfile := CurrentProfile + 1
if (newProfile > 4)
newProfile := 1
ApplyProfileSettings(newProfile)
UpdateProfileHUD()
SaveSettings()
SoundBeep, 700 + (CurrentProfile * 100), 100
TrayTip, Perfil, %RecoilName% - NR/NS, 1, 16
Return

End::
UseHUD := !UseHUD
if (UseHUD) {
Gui, HUD:Show, NoActivate
UpdateHUD()
UpdateProfileHUD()
UpdateBhopHUD()
SoundBeep, 800, 100
TrayTip, HUD, HUD VISÍVEL, 1, 16
} else {
Gui, HUD:Hide
SoundBeep, 600, 100
TrayTip, HUD, HUD OCULTO, 1, 16
}
SaveSettings()
Return

F12::
SaveSettings()
SoundBeep, 750, 300
ExitApp
Return

AutoSaveOnExit:
SaveSettings()
ExitApp
Return

#If CanTrigger()
~*LButton::
If (!AimActive) {
AimActive := True
ShotStep := 0
Gosub, ApplyRecoil
SetTimer, ApplyRecoil, %RecoilDelay%
}
Return
#If
~*LButton Up::
If (AimActive) {
AimActive := False
SetTimer, ApplyRecoil, Off
ShotStep := 0
}
Return
ApplyRecoil:
If (!CanTrigger() || !AimActive) {
SetTimer, ApplyRecoil, Off
AimActive := False
ShotStep := 0
Return
}
ShotStep++
move := GetRecoilStep(CurrentProfile, ShotStep)
dx := move[1]
dy := move[2]
DllCall("mouse_event", "UInt", 0x0001, "Int", dx, "Int", dy, "UInt", 0, "UInt", 0)
If (UseScreenShake) {
Random, randX, -1, 1
Random, randY, -1, 1
DllCall("mouse_event", "UInt", 0x0001, "Int", randX, "Int", randY, "UInt", 0, "UInt", 0)
}
Return
#If (CanTrigger() && BhopEnabled)
*$Space::
While GetKeyState("Space", "P")
{
    if (!CanTrigger() || !BhopEnabled)
        break
    Send, {Blind}{Space}
    Sleep, %BhopDelay%
}
Return
#If
#If CanTrigger()
~$*w::
Return
#If