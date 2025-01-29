XPStyle on

!include "LogicLib.nsh"
!include "x64.nsh"
!include "nsProcess.nsh"

!define VERSION "1.0.0.0"
!define DISPLAY_NAME "Shift Switch"
!define EXE32 "ShiftSwitchUI.exe"
!define EXE64 "ShiftSwitchUI64.exe"
!define UNINSTALLER_NAME "uninstaller.exe"
!define REG_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall"
!define REG_RUN_KEY "Software\Microsoft\Windows\CurrentVersion\Run"

Name "${DISPLAY_NAME} Installer"
OutFile "Shift-Switch-Installer_${VERSION}.exe"
Icon "..\ShiftSwitchUI\app.ico"

InstallDir "$PROGRAMFILES64\${DISPLAY_NAME}"

Var EXE
Var FULLPATH

!macro KillExe UN
Function ${UN}KillExe
    Exch $0
    Push $R0
    DetailPrint "Searching process: $0"
    nsProcess::_FindProcess "$0"
    Pop $R0
    ${If} $R0 = 0
        DetailPrint "Found. Close process:$0"
        nsProcess::_CloseProcess "$0"
        Pop $R0

    ${EndIf}
    Pop $R0
    Pop $0
FunctionEnd
!macroend

#!insertmacro KillExe ""
!insertmacro KillExe un.

!define CheckDllExists "!insertmacro CheckDllExists"
!macro CheckDllExists
    IfFileExists $INSTDIR\*.dll reboot exit
    
reboot:
    Delete /REBOOTOK $INSTDIR\*.dll        
    MessageBox MB_YESNO|MB_ICONSTOP "Uninstall previous version need reboot. Reboot now?" /SD IDNO IDYES +2
    Abort
    Reboot
exit:    
!macroend

!macro UninstallExisting exitcode uninstcommand
Push `${uninstcommand}`
Call UninstallExisting
Pop ${exitcode}
!macroend

Function UninstallExisting
Exch $1 ; uninstcommand
Push $2 ; Uninstaller
Push $3 ; Len
StrCpy $3 ""
StrCpy $2 $1 1
StrCmp $2 '"' qloop sloop
sloop:
	StrCpy $2 $1 1 $3
	IntOp $3 $3 + 1
	StrCmp $2 "" +2
	StrCmp $2 ' ' 0 sloop
	IntOp $3 $3 - 1
	Goto run
qloop:
	StrCmp $3 "" 0 +2
	StrCpy $1 $1 "" 1 ; Remove initial quote
	IntOp $3 $3 + 1
	StrCpy $2 $1 1 $3
	StrCmp $2 "" +2
	StrCmp $2 '"' 0 qloop
run:
	StrCpy $2 $1 $3 ; Path to uninstaller
	StrCpy $1 161 ; ERROR_BAD_PATHNAME
	GetFullPathName $3 "$2\.." ; $InstDir
	IfFileExists "$2" 0 +4
    DetailPrint "Uninstalling previous version..."    
	ExecWait '"$2" /S _?=$3' $1 ; This assumes the existing uninstaller is a NSIS uninstaller, other uninstallers don't support /S nor _?=
	IntCmp $1 0 "" fail fail ; Don't delete the installer if it was aborted
    DetailPrint "Done"    
	Delete "$2" ; Delete the uninstaller
	RMDir "$3" ; Try to delete $InstDir
	RMDir "$3\.." ; (Optional) Try to delete the parent of $InstDir
    Return
fail:    
    DetailPrint "Fail. =("    
    Pop $3
    Pop $2
    Exch $1 ; exitcode
FunctionEnd

Function .onGuiInit

FunctionEnd

Section
    SetShellVarContext all

    SetOutPath $INSTDIR
    
    ReadRegStr $0 HKLM "${REG_UNINSTALL_KEY}\${DISPLAY_NAME}" "UninstallString"
    ${If} $0 != ""
        !insertmacro UninstallExisting $0 $0
#        ${If} $0 <> 0
#            MessageBox MB_YESNO|MB_ICONSTOP "Failed to uninstall previuos version, continue anyway?" /SD IDYES IDYES +2
#            Abort
#        ${EndIf}
        ${CheckDllExists}
    ${EndIf}    

    WriteUninstaller $INSTDIR\${UNINSTALLER_NAME}
    
    ${If} ${RunningX64}
        File /r "..\Release\*.exe" "..\Release\*.dll" 
    ${Else} 
        File /r /x "..\Release\*64.*" "..\Release\*.exe" "..\Release\*.dll" 
    ${EndIf}	

    ${If} ${RunningX64}
        StrCpy $EXE ${EXE64}
    ${Else}
        StrCpy $EXE ${EXE32}
    ${EndIf}
    StrCpy $FULLPATH "$INSTDIR\$EXE"
    
    Push $0
    StrCpy $0 "${REG_UNINSTALL_KEY}\${DISPLAY_NAME}"
    
    WriteRegStr HKLM $0 "DisplayName" "${DISPLAY_NAME}"
    WriteRegStr HKLM $0 "UninstallString" '"$INSTDIR\${UNINSTALLER_NAME}"'
    WriteRegStr HKLM $0 "DisplayIcon" '"$FULLPATH"'
    WriteRegStr HKLM $0 "Publisher" "alezhu"
    WriteRegStr HKLM $0 "DisplayVersion" "${VERSION}"
    
    Pop $0

    WriteRegStr HKLM "${REG_RUN_KEY}" "${DISPLAY_NAME}" "$FULLPATH"
    Exec "$FULLPATH"

SectionEnd

Section "Uninstall"
    DeleteRegKey HKLM "${REG_RUN_KEY}\${DISPLAY_NAME}"
    
    Push "${EXE32}"
    Call un.KillExe
    Push "${EXE64}"
    Call un.KillExe

    ${nsProcess::Unload}
    Push $0
    ${For} $0 1 60
       ${If} $0 > 1
         DetailPrint "Waiting unlock *.dll. Try: $0/60"        
         Sleep 1000
       ${EndIf}
        ClearErrors
        Delete $INSTDIR\*.dll
        IfFileExists $INSTDIR\*.dll next
        ${Break}
next:        
    ${Next}
    Pop $0
    
    ${CheckDllExists}

    Delete $INSTDIR\*.exe
    Delete $INSTDIR\${UNINSTALLER_NAME} ; delete self 
    RMDir $INSTDIR
    DeleteRegKey HKLM "${REG_UNINSTALL_KEY}\${DISPLAY_NAME}"
SectionEnd

