; Cutting the Cake screensaver installer.
; Installs for all users (asks for administrator rights) so that it appears in the
; Windows list of screensavers like any other.
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

!ifndef VERSION
  !define VERSION "1.0.0"
!endif

Name "Cutting the Cake ${VERSION}"
OutFile "..\..\dist\CuttingTheCake-Setup-${VERSION}.exe"
InstallDir "$PROGRAMFILES64\Cutting the Cake"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
Unicode true
!define MUI_ICON "cutting-the-cake.ico"
!define MUI_UNICON "cutting-the-cake.ico"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Open Screen Saver Settings"
!define MUI_FINISHPAGE_RUN_FUNCTION OpenSettings

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!define UNKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\CuttingTheCake"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Cutting the Cake needs 64-bit Windows."
    Abort
  ${EndIf}
FunctionEnd

Function OpenSettings
  ; 64-bit rundll32: a 32-bit one looks for screen savers in SysWOW64 and shows (None)
  Exec '"$WINDIR\Sysnative\rundll32.exe" shell32.dll,Control_RunDLL desk.cpl,,1'
FunctionEnd

Section "Cutting the Cake" SecMain
  SectionIn RO
  SetRegView 64
  SetOutPath "$INSTDIR"
  File "..\..\build\win\cutting-the-cake.exe"
  File "..\..\build\win\*.dll"
  File /oname=README.txt "..\..\README.md"
  File /oname=LICENSE.txt "..\..\LICENSE"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\CuttingTheCake" "InstallDir" "$INSTDIR"

  ; the screensaver file itself goes in the real (64-bit) system folder
  ${DisableX64FSRedirection}
  SetOutPath "$WINDIR\System32"
  File "..\..\build\win\CuttingTheCake.scr"
  ${EnableX64FSRedirection}
  SetOutPath "$INSTDIR"

  ; make it the current screensaver for whoever is installing
  WriteRegStr HKCU "Control Panel\Desktop" "SCRNSAVE.EXE" "$WINDIR\System32\CuttingTheCake.scr"
  WriteRegStr HKCU "Control Panel\Desktop" "ScreenSaveActive" "1"
  ReadRegStr $1 HKCU "Control Panel\Desktop" "ScreenSaveTimeOut"
  StrCmp $1 "" 0 +2
    WriteRegStr HKCU "Control Panel\Desktop" "ScreenSaveTimeOut" "300"

  CreateDirectory "$SMPROGRAMS\Cutting the Cake"
  CreateShortcut "$SMPROGRAMS\Cutting the Cake\Cutting the Cake (window).lnk" "$INSTDIR\cutting-the-cake.exe" "--window"
  CreateShortcut "$SMPROGRAMS\Cutting the Cake\Cutting the Cake (full screen).lnk" "$INSTDIR\cutting-the-cake.exe" "/s"
  CreateShortcut "$SMPROGRAMS\Cutting the Cake\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "${UNKEY}" "DisplayName" "Cutting the Cake screensaver"
  WriteRegStr HKLM "${UNKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNKEY}" "DisplayIcon" "$INSTDIR\cutting-the-cake.exe"
  WriteRegStr HKLM "${UNKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNKEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $2 $3 $4
  IntFmt $2 "0x%08X" $2
  WriteRegDWORD HKLM "${UNKEY}" "EstimatedSize" "$2"
SectionEnd

Section "Uninstall"
  SetRegView 64
  ; stop being the screensaver, if we still are
  ReadRegStr $1 HKCU "Control Panel\Desktop" "SCRNSAVE.EXE"
  StrCmp $1 "$WINDIR\System32\CuttingTheCake.scr" 0 +2
    DeleteRegValue HKCU "Control Panel\Desktop" "SCRNSAVE.EXE"
  ${DisableX64FSRedirection}
  Delete "$WINDIR\System32\CuttingTheCake.scr"
  ${EnableX64FSRedirection}

  Delete "$INSTDIR\cutting-the-cake.exe"
  Delete "$INSTDIR\SDL2.dll"
  Delete "$INSTDIR\SDL3.dll"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Cutting the Cake\Cutting the Cake (window).lnk"
  Delete "$SMPROGRAMS\Cutting the Cake\Cutting the Cake (full screen).lnk"
  Delete "$SMPROGRAMS\Cutting the Cake\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Cutting the Cake"
  DeleteRegKey HKLM "${UNKEY}"
  DeleteRegKey HKLM "Software\CuttingTheCake"
SectionEnd
