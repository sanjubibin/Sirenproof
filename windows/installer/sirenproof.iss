; Sirenproof's installer (Inno Setup 6.3 or later).
;
; Build it with tools\package.ps1, which stages the programs, Qt and the compiler's runtime first and
; passes the version. Compiled by hand, it expects that staging folder:
;
;   ISCC.exe /DAppVersion=0.1.0 /DStageDir=..\build\package\stage installer\sirenproof.iss
;
; What it promises:
; - It installs only into Program Files, since the service runs as SYSTEM and must not be replaceable
;   by anyone else (sirenproof-service refuses to install from anywhere else).
; - It won't uninstall, or update, while a lock holds. The service is the judge: Windows only lets it be
;   stopped while no lock holds (sirenproof-service can-stop / uninstall, exit code 3).
; - The tray starts at sign-in, for every account on this PC.

#if Ver < EncodeVer(6, 3, 0)
  #error Inno Setup 6.3 or later is needed (for ExecAndCaptureOutput)
#endif

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\build\package\stage"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\package"
#endif

[Setup]
AppId={{6F1D7C42-93B8-4E57-A0C4-2E8B51D9F3A6}
AppName=Sirenproof
AppVersion={#AppVersion}
AppVerName=Sirenproof {#AppVersion}
AppPublisher=Sirenproof contributors
AppPublisherURL=https://github.com/sanjubibin/Sirenproof
AppSupportURL=https://github.com/sanjubibin/Sirenproof/issues
AppUpdatesURL=https://github.com/sanjubibin/Sirenproof/releases
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\Sirenproof
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
LicenseFile={#StageDir}\LICENSE.txt
OutputDir={#OutputDir}
OutputBaseFilename=Sirenproof-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Sirenproof
UninstallDisplayIcon={app}\sirenproof-ui.exe
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
#ifdef SignTool
SignTool={#SignTool}
SignedUninstaller=yes
#endif

[Tasks]
Name: desktopicon; Description: "Put Sirenproof on the desktop"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Sirenproof"; Filename: "{app}\sirenproof-ui.exe"; Comment: "Your rules, locks and breaks"
Name: "{autodesktop}\Sirenproof"; Filename: "{app}\sirenproof-ui.exe"; Tasks: desktopicon

[Registry]
; The tray starts at sign-in for everyone on this PC; it opens the main window when clicked.
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Sirenproof"; \
    ValueData: """{app}\sirenproof-tray.exe"""; Flags: uninsdeletevalue

[Run]
Filename: "{app}\sirenproof-tray.exe"; Description: "Start Sirenproof now"; \
    Flags: postinstall nowait runasoriginaluser skipifsilent
Filename: "{app}\sirenproof-ui.exe"; Description: "Open the main window"; \
    Flags: postinstall nowait runasoriginaluser skipifsilent unchecked

[Code]
const
  ExitLocked = 3;        // sirenproof-service: a lock holds
  ExitNotInstalled = 4;  // sirenproof-service: there's no service to remove
  LockedText = 'A lock is active, and Sirenproof can''t be changed until it ends. ' +
               'That''s the point of a lock: you chose it. Try again once it''s over.';

function ServiceExe(): String;
begin
  Result := ExpandConstant('{app}\sirenproof-service.exe');
end;

// Runs sirenproof-service with Params, hidden. Output gets what it printed, for messages.
function RunService(const Params: String; var Code: Integer; var Output: String): Boolean;
var
  Captured: TExecOutput;
  I: Integer;
begin
  Output := '';
  Result := ExecAndCaptureOutput(ServiceExe(), Params, ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated,
                                 Code, Captured);
  if not Result then begin
    Output := SysErrorMessage(Code);
    Exit;
  end;
  for I := 0 to GetArrayLength(Captured.StdOut) - 1 do Output := Output + Captured.StdOut[I] + #13#10;
  for I := 0 to GetArrayLength(Captured.StdErr) - 1 do Output := Output + Captured.StdErr[I] + #13#10;
  Output := Trim(Output);
  Log(Format('sirenproof-service %s: exit %d: %s', [Params, Code, Output]));
end;

// The tray, the window and any browser relays, so their files can be replaced or removed. Only
// Sirenproof's own programs, by name.
procedure StopOurPrograms();
var
  Code: Integer;
begin
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM sirenproof-tray.exe /IM sirenproof-ui.exe /IM sirenproof-host.exe',
       '', SW_HIDE, ewWaitUntilTerminated, Code);
end;

// ---- Installing and updating ----

// An update first removes the running service (keeping its data); a lock refuses that.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Code: Integer;
  Output: String;
begin
  Result := '';
  if not FileExists(ServiceExe()) then Exit;

  if RunService('can-stop', Code, Output) and (Code = ExitLocked) then begin
    Result := LockedText;
    Exit;
  end;
  if not RunService('uninstall', Code, Output) then begin
    Result := 'Setup couldn''t run the installed Sirenproof to stop it: ' + Output;
    Exit;
  end;
  if Code = ExitLocked then begin
    Result := LockedText;
    Exit;
  end;
  if (Code <> 0) and (Code <> ExitNotInstalled) then begin
    Result := 'Setup couldn''t stop the installed Sirenproof:' + #13#10 + Output;
    Exit;
  end;
  StopOurPrograms();
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Code: Integer;
  Output: String;
begin
  if CurStep <> ssPostInstall then Exit;
  WizardForm.StatusLabel.Caption := 'Starting the Sirenproof service...';
  if not RunService('install --enforce', Code, Output) or (Code <> 0) then
    SuppressibleMsgBox('Sirenproof was copied, but its service didn''t start:' + #13#10#13#10 + Output + #13#10#13#10 +
                       'Nothing is blocked until it runs. To try again, run this in an administrator prompt:' + #13#10 +
                       '"' + ServiceExe() + '" install', mbError, MB_OK, IDOK);
end;

// ---- Uninstalling ----

function InitializeUninstall(): Boolean;
var
  Code: Integer;
  Output: String;
begin
  Result := True;
  if RunService('can-stop', Code, Output) and (Code = ExitLocked) then begin
    SuppressibleMsgBox(LockedText, mbInformation, MB_OK, IDOK);
    Result := False;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Code: Integer;
  Output, Params: String;
begin
  if CurUninstallStep <> usUninstall then Exit;

  Params := 'uninstall';
  if not UninstallSilent and
     (MsgBox('Also delete your rules, settings and Sirenproof''s log?' + #13#10#13#10 +
             'Choose No to keep them for a later reinstall.', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES) then
    Params := Params + ' --remove-data';

  // A lock may have started since the check above.
  if not RunService(Params, Code, Output) then begin
    SuppressibleMsgBox('Sirenproof''s service couldn''t be removed: ' + Output, mbError, MB_OK, IDOK);
    Abort;
  end;
  if Code = ExitLocked then begin
    SuppressibleMsgBox(LockedText, mbInformation, MB_OK, IDOK);
    Abort;
  end;
  if (Code <> 0) and (Code <> ExitNotInstalled) then begin
    SuppressibleMsgBox('Sirenproof''s service couldn''t be removed:' + #13#10 + Output, mbError, MB_OK, IDOK);
    Abort;
  end;
  StopOurPrograms();
end;
