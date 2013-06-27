; example2.nsi
;
; This script is based on example1.nsi, but it remember the directory, 
; has uninstall support and (optionally) installs start menu shortcuts.
;
; It will install example2.nsi into a directory that the user selects,
;--------------------------------
!include "MUI2.nsh"

; The name of the installer
Name "FreePiano"

; The file to write
OutFile "nsis/freepiano_setup.exe"

; The default installation directory
InstallDir $PROGRAMFILES\FreePiano

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\FreePiano" "Install_Dir"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------
;Interface Settings

!define MUI_ABORTWARNING
!define MUI_ICON "..\res\icon.ico"
!define MUI_UNICON "..\res\icon.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\freepiano.exe"
!define MUI_COMPONENTSPAGE_NODESC

;--------------------------------
; Pages

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages
!insertmacro MUI_LANGUAGE "SimpChinese"

;--------------------------------

; The stuff to install
Section "FreePiano 主程序"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  !cd ..\data
  File /r "*"
  File /oname=freepiano.exe ..\vc\Release\freepiano.exe
  !cd ..\
  
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\FreePiano "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FreePiano" "DisplayName" "FreePiano"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FreePiano" "DisplayIcon" "$INSTDIR\freepiano.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FreePiano" "DisplayVersion" "1.8"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FreePiano" "Publisher" "tiwb.com"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FreePiano" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FreePiano" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FreePiano" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

; Optional section (can be disabled by the user)
Section "开始菜单"

  CreateDirectory "$SMPROGRAMS\FreePiano"
  CreateShortCut "$SMPROGRAMS\FreePiano\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\FreePiano\FreePiano.lnk" "$INSTDIR\freepiano.exe" "" "$INSTDIR\freepiano.exe" 0
  
SectionEnd

; Optional section (can be disabled by the user)
Section "桌面快捷方式"

  CreateShortCut "$DESKTOP\FreePiano.lnk" "$INSTDIR\freepiano.exe" "" "$INSTDIR\freepiano.exe" 0
  
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FreePiano"
  DeleteRegKey HKLM SOFTWARE\FreePiano

  ; Remove directories used
  Delete "$DESKTOP\FreePiano.lnk"
  RMDir /r "$SMPROGRAMS\FreePiano"
  RMDir /r "$INSTDIR"

SectionEnd
