#ifndef StageDir
  #error Pass /DStageDir with an absolute role-specific payload directory.
#endif
#ifndef AppVersion
  #error Pass /DAppVersion with the CMake project version.
#endif
#ifndef OutputDir
  #define OutputDir "Output"
#endif
#define AppExe "sharekeys_" + Role + ".exe"
#if !FileExists(StageDir + "\" + AppExe)
  #error The staged executable for this role is missing.
#endif

[Setup]
AppId={#AppGuid}
AppName={#AppTitle}
AppVersion={#AppVersion}
VersionInfoVersion={#AppVersion}.0
VersionInfoDescription={#AppTitle} Setup
DefaultDirName={localappdata}\Programs\{#InstallFolder}
DefaultGroupName={#AppTitle}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename={#SetupName}{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\resources\windows\app.ico
DisableDirPage=no
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\{#AppExe}
#ifdef SignPackages
SignTool=kvmSign
SignedUninstaller=yes
#else
SignedUninstaller=no
#endif

[Files]
; Payload is already restricted to one role and independently verified.
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Icons]
Name: "{group}\{#ShortcutName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppTitle}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#ShortcutName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; Optional startup is controlled by the GUI. Uninstall touches only this role.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "{#StartupValue}"; Flags: dontcreatekey uninsdeletevalue

[Run]
Filename: "{app}\{#AppExe}"; Description: "Open {#AppTitle}"; Flags: postinstall nowait skipifsilent unchecked

[Code]
function InitializeSetup(): Boolean;
begin
  { The historical combined product had one uninstaller for both roles. Require
    its removal first; it retains the ShareKeysHost/Agent profile directories. }
  Result := not RegKeyExists(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{E2F4A277-D939-4E36-BA22-138CAD33C523}_is1');
  if not Result then
    MsgBox('Please stop both apps and uninstall the older combined ShareKeys/KVM-Win package first. Its uninstaller preserves your pairings and settings. Then install the new Host and/or Agent packages.', mbError, MB_OK);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
#if Role == "host"
  if FileExists(ExpandConstant('{app}\sharekeys_agent.exe')) then
#else
  if FileExists(ExpandConstant('{app}\sharekeys_host.exe')) then
#endif
    Result := 'Host and Agent must use separate installation directories. Please choose a different folder.';
end;
