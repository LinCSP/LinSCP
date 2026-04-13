; LinSCP — Inno Setup installer script
; Called from CI:
;   ISCC.exe /O. /DMyAppVersion=0.1.0 packaging\inno\linscp.iss
; All files to bundle must be staged in deploy\ (repo root) by windeployqt
; before running this script.

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0-dev"
#endif

#define MyAppName      "LinSCP"
#define MyAppPublisher "LinSCP"
#define MyAppURL       "https://github.com/LinCSP/LinSCP"
#define MyAppExeName   "linscp.exe"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputBaseFilename=LinSCP-{#MyAppVersion}-windows-x64
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
; Installer/uninstaller wizard icon (path relative to this .iss file)
SetupIconFile=..\..\packaging\windows\linscp.ico
; Source path is relative to the location of this .iss file
SourceDir=..\..\

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
  GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; All binaries, Qt DLLs, plugins and vcpkg DLLs staged by windeployqt
Source: "deploy\*"; DestDir: "{app}"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; IconFilename is optional — Windows reads the icon from the EXE resource (ID 1).
; Listed explicitly so the entry is self-documenting.
Name: "{group}\{#MyAppName}";                       Filename: "{app}\{#MyAppExeName}"; \
  IconFilename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";                 Filename: "{app}\{#MyAppExeName}"; \
  IconFilename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; Install MSVC runtime silently if bundled by windeployqt
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
  StatusMsg: "Installing Visual C++ Runtime..."; \
  Flags: runascurrentuser skipifdoesntexist
Filename: "{app}\{#MyAppExeName}"; \
  Description: "{cm:LaunchProgram,{#MyAppName}}"; \
  Flags: nowait postinstall skipifsilent
