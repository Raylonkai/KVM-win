#ifndef StageDir
  #define StageDir "..\build\stage"
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
[Setup]
AppId={{E2F4A277-D939-4E36-BA22-138CAD33C523}
AppName=ShareKeys
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\ShareKeys
DefaultGroupName=ShareKeys
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=ShareKeys-{#AppVersion}-x64
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\sharekeys_host.exe

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Tasks]
Name: "desktopicons"; Description: "Create desktop shortcuts"; Flags: unchecked

[Icons]
Name: "{group}\ShareKeys Host (main PC)"; Filename: "{app}\sharekeys_host.exe"
Name: "{group}\ShareKeys Agent (remote PC)"; Filename: "{app}\sharekeys_agent.exe"
Name: "{group}\Uninstall ShareKeys"; Filename: "{uninstallexe}"
Name: "{autodesktop}\ShareKeys Host (main PC)"; Filename: "{app}\sharekeys_host.exe"; Tasks: desktopicons
Name: "{autodesktop}\ShareKeys Agent (remote PC)"; Filename: "{app}\sharekeys_agent.exe"; Tasks: desktopicons

[Registry]
; Startup is chosen inside each app. These entries only remove our own Run keys
; during uninstall; installation does not enable either role automatically.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "ShareKeysHost"; Flags: dontcreatekey uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "ShareKeysAgent"; Flags: dontcreatekey uninsdeletevalue

[InstallDelete]
; Remove the old installer-managed shortcut when upgrading this development build.
Type: files; Name: "{userstartup}\ShareKeys Agent.lnk"

[Run]
Filename: "{app}\sharekeys_host.exe"; Description: "Open ShareKeys Host (main PC)"; Flags: postinstall nowait skipifsilent unchecked
Filename: "{app}\sharekeys_agent.exe"; Description: "Open ShareKeys Agent (remote PC)"; Flags: postinstall nowait skipifsilent unchecked
