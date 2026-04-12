; LinSCP — NSIS installer script
; Invoked from CI: makensis /DVERSION=v0.1.0 packaging\nsis\linscp.nsi
; Files are staged in deploy\ relative to the repo root (by windeployqt).

!ifndef VERSION
  !define VERSION "dev"
!endif

!define APP_NAME   "LinSCP"
!define APP_EXE    "linscp.exe"
!define INST_KEY   "Software\LinSCP"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\LinSCP"

Name            "${APP_NAME} ${VERSION}"
OutFile         "LinSCP-${VERSION}-windows-x64.exe"
InstallDir      "$PROGRAMFILES64\LinSCP"
InstallDirRegKey HKLM "${INST_KEY}" "InstallDir"
RequestExecutionLevel admin
Unicode True
SetCompressor /SOLID lzma

!include "MUI2.nsh"

!define MUI_WELCOMEPAGE_TITLE "Welcome to ${APP_NAME} ${VERSION}"
!define MUI_FINISHPAGE_RUN         "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT    "Launch ${APP_NAME}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"

; ── Install ───────────────────────────────────────────────────────────────────
Section "LinSCP" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "deploy\*.*"

  WriteRegStr HKLM "${INST_KEY}" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Start Menu
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortcut  "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"   "$INSTDIR\Uninstall.exe"

  ; Add/Remove Programs entry
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${APP_NAME}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "LinSCP"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "${UNINST_KEY}" "URLInfoAbout"    "https://github.com/LinCSP/LinSCP"
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify"        1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair"        1
SectionEnd

; ── Uninstall ─────────────────────────────────────────────────────────────────
Section "Uninstall"
  RMDir /r "$INSTDIR"

  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\${APP_NAME}"

  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "${INST_KEY}"
SectionEnd
