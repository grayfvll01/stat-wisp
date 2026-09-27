#ifndef AppVersion
  #error Build with scripts/build-release.ps1 or pass /DAppVersion=MAJOR.MINOR.PATCH.
#endif

#define AppName "Stat Wisp"
#define AppPublisher "grayfvll01"
#define AppUrl "https://github.com/grayfvll01/stat-wisp"

[Setup]
AppId={{96D1C574-E951-4E2D-AC31-BBDBB1B5915F}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
; Per-user by default; administrators can deploy for all users with /ALLUSERS.
DefaultDirName={autopf}\stat-wisp
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
DefaultGroupName=Stat Wisp
DisableProgramGroupPage=yes
DisableReadyPage=yes
AllowNoIcons=yes
MinVersion=10.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=stat-wisp-{#AppVersion}-setup
SetupIconFile=..\assets\stat-wisp.ico
UninstallDisplayIcon={app}\stat-wisp.exe
UninstallDisplayName={#AppName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
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
; In-app updates pass /RELAUNCH=1 so the updated app starts again after a silent install.
Filename: "{app}\stat-wisp.exe"; Parameters: "--relaunch"; Flags: nowait runasoriginaluser; Check: ShouldRelaunch

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\stat-wisp\updates"

[Code]
const
  WM_COMMAND = $0111;
  ExitCommand = 4504;

function ShouldRelaunch: Boolean;
begin
  Result := ExpandConstant('{param:RELAUNCH|0}') = '1';
end;

{ Ask a running Stat Wisp to exit normally so its tray icons are removed. }
procedure CloseRunningApp;
var
  Window: HWND;
  Attempts: Integer;
begin
  Window := FindWindowByClassName('stat-wisp-message-window');
  if Window = 0 then
    exit;
  PostMessage(Window, WM_COMMAND, ExitCommand, 0);
  Attempts := 0;
  while (FindWindowByClassName('stat-wisp-message-window') <> 0) and (Attempts < 50) do
  begin
    Sleep(100);
    Attempts := Attempts + 1;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  CloseRunningApp;
  Result := '';
end;

function InitializeUninstall: Boolean;
begin
  CloseRunningApp;
  Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usUninstall) and not UninstallSilent then
  begin
    if MsgBox('Keep your Stat Wisp settings for a future installation?', mbConfirmation, MB_YESNO) = IDNO then
      DelTree(ExpandConstant('{localappdata}\stat-wisp'), True, True, True);
  end;
end;
