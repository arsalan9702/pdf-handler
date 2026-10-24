; Inno Setup script. Build with:
;   iscc /DAppVersion=0.1.0 /DSourceDir=<install prefix> /DIconFile=<app.ico> /DOutputDir=<dir> installer.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{6F2B1E4A-8C3D-4F7A-9B21-5D0C3E8A7F14}
AppName=PDF Handler
AppVersion={#AppVersion}
AppPublisher=PDF Handler
DefaultDirName={autopf}\PDF Handler
DefaultGroupName=PDF Handler
DisableProgramGroupPage=yes
; Per-user install by default (no admin prompt); users may choose all-users.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=PDF-Handler-{#AppVersion}-Setup
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\bin\pdf-handler.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
ChangesEnvironment=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "addtopath"; Description: "Add the pdfh command to PATH"; GroupDescription: "Command line:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\PDF Handler"; Filename: "{app}\bin\pdf-handler.exe"
Name: "{autodesktop}\PDF Handler"; Filename: "{app}\bin\pdf-handler.exe"; Tasks: desktopicon

[Registry]
; "Open with" entries for PDFs and images without taking over the default viewer.
Root: HKA; Subkey: "Software\Classes\Applications\pdf-handler.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "PDF Handler"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\pdf-handler.exe\shell\open\command"; ValueType: string; ValueData: """{app}\bin\pdf-handler.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\Applications\pdf-handler.exe\SupportedTypes"; ValueType: string; ValueName: ".pdf"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\pdf-handler.exe\SupportedTypes"; ValueType: string; ValueName: ".png"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\pdf-handler.exe\SupportedTypes"; ValueType: string; ValueName: ".jpg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\pdf-handler.exe\SupportedTypes"; ValueType: string; ValueName: ".jpeg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\PdfHandler.Document"; ValueType: string; ValueData: "PDF Document"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\PdfHandler.Document\DefaultIcon"; ValueType: string; ValueData: "{app}\bin\pdf-handler.exe,0"
Root: HKA; Subkey: "Software\Classes\PdfHandler.Document\shell\open\command"; ValueType: string; ValueData: """{app}\bin\pdf-handler.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\.pdf\OpenWithProgids"; ValueType: string; ValueName: "PdfHandler.Document"; ValueData: ""; Flags: uninsdeletevalue

[Run]
Filename: "{app}\bin\pdf-handler.exe"; Description: "{cm:LaunchProgram,PDF Handler}"; Flags: nowait postinstall skipifsilent

[Code]
const
  EnvKey = 'Environment';

function BinDir: string;
begin
  Result := ExpandConstant('{app}\bin');
end;

procedure AddToPath;
var
  Paths: string;
begin
  if not RegQueryStringValue(HKCU, EnvKey, 'Path', Paths) then
    Paths := '';
  if Pos(';' + Uppercase(BinDir) + ';', ';' + Uppercase(Paths) + ';') = 0 then
  begin
    if (Paths <> '') and (Copy(Paths, Length(Paths), 1) <> ';') then
      Paths := Paths + ';';
    RegWriteExpandStringValue(HKCU, EnvKey, 'Path', Paths + BinDir);
  end;
end;

procedure RemoveFromPath;
var
  Paths: string;
  P: Integer;
begin
  if RegQueryStringValue(HKCU, EnvKey, 'Path', Paths) then
  begin
    P := Pos(';' + Uppercase(BinDir), ';' + Uppercase(Paths));
    if P > 0 then
    begin
      Delete(Paths, P - 1, Length(BinDir) + 1);
      RegWriteExpandStringValue(HKCU, EnvKey, 'Path', Paths);
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then
    AddToPath;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromPath;
end;
