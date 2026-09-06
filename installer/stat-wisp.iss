#ifndef AppVersion
  #define AppVersion "0.4.0"
#endif

#define AppName "Stat Wisp"
#define AppPublisher "grayfvll01"
#define AppUrl "https://github.com/grayfvll01/stat-wisp"

[Setup]
AppId={{96D1C574-E951-4E2D-AC31-BBDBB1B5915F}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
DefaultDirName={localappdata}\Programs\stat-wisp
DefaultGroupName=Stat Wisp
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=stat-wisp-{#AppVersion}-setup
SetupIconFile=..\assets\stat-wisp.ico
UninstallDisplayIcon={app}\stat-wisp.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
CloseApplications=yes
RestartApplications=no
ChangesAssociations=no
VersionInfoVersion={#AppVersion}.0
VersionInfoDescription=Stat Wisp setup

[Files]
Source: "..\dist\stat-wisp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\Stat Wisp"; Filename: "{app}\stat-wisp.exe"
Name: "{group}\Uninstall Stat Wisp"; Filename: "{uninstallexe}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "stat-wisp"; ValueType: none; Flags: uninsdeletevalue dontcreatekey

[Run]
Filename: "{app}\stat-wisp.exe"; Description: "Start Stat Wisp"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usUninstall) and not UninstallSilent then
  begin
    if MsgBox('Keep your Stat Wisp settings for a future installation?', mbConfirmation, MB_YESNO) = IDNO then
      DelTree(ExpandConstant('{localappdata}\stat-wisp'), True, True, True);
  end;
end;
