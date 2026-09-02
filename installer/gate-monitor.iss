#ifndef AppVersion
  #define AppVersion "0.3.0"
#endif

#define AppName "gate-monitor"
#define AppPublisher "grayfvll01"
#define AppUrl "https://github.com/grayfvll01/gate-monitor"

[Setup]
AppId={{96D1C574-E951-4E2D-AC31-BBDBB1B5915F}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
DefaultDirName={localappdata}\Programs\gate-monitor
DefaultGroupName=gate-monitor
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=gate-monitor-{#AppVersion}-setup
SetupIconFile=..\assets\gate-monitor.ico
UninstallDisplayIcon={app}\gate-monitor.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
CloseApplications=yes
RestartApplications=no
ChangesAssociations=no
VersionInfoVersion={#AppVersion}.0
VersionInfoDescription=gate-monitor setup

[Files]
Source: "..\dist\gate-monitor.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\gate-monitor"; Filename: "{app}\gate-monitor.exe"
Name: "{group}\Uninstall gate-monitor"; Filename: "{uninstallexe}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "gate-monitor"; ValueType: none; Flags: uninsdeletevalue dontcreatekey

[Run]
Filename: "{app}\gate-monitor.exe"; Description: "Start gate-monitor"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    if not Exec(ExpandConstant('{app}\gate-monitor.exe'), '--configure-only', '', SW_SHOWNORMAL,
      ewWaitUntilTerminated, ResultCode) then
      RaiseException('gate-monitor first-run setup could not be started.');
    if ResultCode <> 0 then
      RaiseException('Select at least one tray metric to complete gate-monitor installation.');
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    if MsgBox('Keep your gate-monitor settings for a future installation?', mbConfirmation, MB_YESNO) = IDNO then
      DelTree(ExpandConstant('{localappdata}\gate-monitor'), True, True, True);
  end;
end;
