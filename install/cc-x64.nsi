; Constrol Center NSIS Modern User Interface

;--------------------------------
; Include Modern UI

  !include "MUI2.nsh"

  !include x64.nsh

; Handle 64bit mode
Function .onInit

        ${If} ${RunningX64}

        ${EnableX64FSRedirection}

        ${else}

        MessageBox MB_OK "Sorry this application runs only on x64 machines"

        Abort

        ${EndIf}

FunctionEnd

;--------------------------------
; General

  ; Name and file
  Name "Control Center 0.5"
  OutFile "cc05win64.exe"

  ; Default installation folder
  InstallDir "$PROGRAMFILES64\Control Center"
  
  ; Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\ControlCenter" ""

  ; Request application privileges for Windows Vista
  RequestExecutionLevel user

;--------------------------------
; Variables

  Var StartMenuFolder

;--------------------------------
; Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
; Pages

  !insertmacro MUI_PAGE_LICENSE "..\scripts\license.txt"
  ;!insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  
  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\ControlCenter" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  
  !insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder
  
  !insertmacro MUI_PAGE_INSTFILES
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
; Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Installer Sections

Section "Dummy Section" SecDummy

  SetOutPath "$INSTDIR"
  
  ; Binaries
  File cc.exe
  File *.dll
  ; Help
  File readme.txt
    
  ; Store installation folder
  WriteRegStr HKCU "Software\ControlCenter" "" $INSTDIR
  
  ; Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    
    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Control Center.lnk" "$INSTDIR\cc.exe"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    
    ; Create Uninstall infor 
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ControlCenter" \
                     "DisplayName" "Control Center"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ControlCenter" \
                     "UninstallString" "$\"$INSTDIR\uninstall.exe$\""  
  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

;--------------------------------
; Descriptions

  ; Language strings
  ;LangString DESC_SecDummy ${LANG_ENGLISH} "A test section."

  ; Assign language strings to sections
  ;!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  ;  !insertmacro MUI_DESCRIPTION_TEXT ${SecDummy} $(DESC_SecDummy)
  ;!insertmacro MUI_FUNCTION_DESCRIPTION_END
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ; Binaries
  Delete "$INSTDIR\cc.exe"
  Delete "$INSTDIR\dw.dll"
  Delete "$INSTDIR\dwcompat.dll"
  ; Help
  Delete "$INSTDIR\readme.txt"

  Delete "$INSTDIR\Uninstall.exe"

  RMDir "$INSTDIR"
  
  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
    
  Delete "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Control Center.lnk"
  RMDir "$SMPROGRAMS\$StartMenuFolder"
  
  DeleteRegKey /ifempty HKCU "Software\ControlCenter"

SectionEnd