; <COMPILER: v1.1.37.02>
#NoEnv
#SingleInstance, Force
SetWorkingDir, %A_ScriptDir%
SendMode, Input
SetBatchLines, -1
SetKeyDelay, -1, -1
SetCapsLockState, AlwaysOff
If FileExist("icone.ico")
Menu, Tray, Icon, icone.ico
CurrentProfile := 1
RecoilAmount := 4
RecoilName := "AK-47"
RecoilDelay := 20
JumpDelay := 10
UseScreenShake := True
UseHUD := True
global Ativo := False
global AimActive := False
global BhopActive := False
global BhopSending := False
Gui, HUD:+LastFound +AlwaysOnTop -Caption +ToolWindow +E0x20
Gui, HUD:Color, 181818
Gui, HUD:Font, s9 bold, Segoe UI
Gui, HUD:Add, Text, vStatusText cFF3333 Center w170, CS 1.6 [OFF]
Gui, HUD:Add, Text, vProfileText cFFAA00 Center w170, [AK-47] Recoil: 4
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
global RecoilName, RecoilAmount, UseHUD
if (!UseHUD)
return
Gui, HUD:Font, cFFAA00
GuiControl, HUD:Font, ProfileText
GuiControl, HUD:, ProfileText, [%RecoilName%] Recoil: %RecoilAmount%
}
ReleaseKeys() {
global BhopActive, BhopSending
BhopActive := False
BhopSending := False
SetTimer, ApplyBhop, Off
Send, {Space up}
Send, {LButton up}
Send, {RButton up}
Send, {w up}
Send, {s up}
Send, {a up}
Send, {d up}
Send, {Ctrl up}
SetTimer, ApplyRecoil, Off
Return
}
Insert::
Ativo := !Ativo
UpdateHUD()
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
CurrentProfile++
if (CurrentProfile > 4)
CurrentProfile := 1
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
UpdateProfileHUD()
SoundBeep, 700 + (CurrentProfile * 100), 100
TrayTip, Perfil, %RecoilName% - Recoil: %RecoilAmount%, 1, 16
Return
End::
UseHUD := !UseHUD
if (UseHUD) {
Gui, HUD:Show, NoActivate
UpdateHUD()
UpdateProfileHUD()
SoundBeep, 800, 100
TrayTip, HUD, HUD VISÃVEL, 1, 16
} else {
Gui, HUD:Hide
SoundBeep, 600, 100
TrayTip, HUD, HUD OCULTO, 1, 16
}
Return
F12::
SoundBeep, 750, 300
ExitApp
Return
#If CanTrigger()
~*LButton::
If (!AimActive) {
AimActive := True
DllCall("mouse_event", "UInt", 0x0001, "UInt", 0, "UInt", -1, "UInt", 0, "UInt", 0)
SetTimer, ApplyRecoil, %RecoilDelay%
}
Return
#If
~*LButton Up::
If (AimActive) {
AimActive := False
SetTimer, ApplyRecoil, Off
}
Return
ApplyRecoil:
If (!CanTrigger() || !AimActive) {
SetTimer, ApplyRecoil, Off
AimActive := False
Return
}
DllCall("mouse_event", "UInt", 0x0001, "UInt", 0, "UInt", RecoilAmount, "UInt", 0, "UInt", 0)
If (UseScreenShake) {
Random, randX, -1, 1
Random, randY, -1, 1
DllCall("mouse_event", "UInt", 0x0001, "UInt", randX, "UInt", randY, "UInt", 0, "UInt", 0)
}
Return
#If CanTrigger()
$*Space::
BhopActive := True
BhopSending := False
SetTimer, ApplyBhop, %JumpDelay%
Return
#If

$*Space Up::
If (BhopSending)
Return
If (BhopActive) {
BhopActive := False
SetTimer, ApplyBhop, Off
Send, {Space up}
}
Return

ApplyBhop:
If (!CanTrigger() || !BhopActive || !GetKeyState("Space", "P")) {
BhopActive := False
BhopSending := False
SetTimer, ApplyBhop, Off
Send, {Space up}
Return
}
BhopSending := True
Send, {Space down}
Send, {Space up}
BhopSending := False
Return

#If CanTrigger()
~$*w::
Return
#If