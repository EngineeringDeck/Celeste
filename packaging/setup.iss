#define MyAppName "Celeste"
#define MyAppVersion "1.0"
#define MyAppPublisher "The Engineering Deck"
#define MyAppURL "https://github.com/EngineeringDeck/Celeste"
#define MyAppExeName "Celeste.exe"

[Setup]
AppId={{E0BBEFE0-640F-40D9-BB9D-C859E51D087D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=SetupCeleste
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon="{app}\celeste.exe"
SetupIconFile="..\resources\celeste.ico"

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "C:\src\Celeste\packaging\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\Pulsar.dll"; DestDir: "{code:OBSLocation}"; Flags: ignoreversion; Components: pulsar
Source: "C:\src\Celeste\packaging\D3Dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\libcrypto-1_1-x64.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\libEGL.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\libGLESv2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\libssl-1_1-x64.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\Qt5Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\Qt5Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\Qt5Multimedia.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\Qt5MultimediaWidgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\Qt5Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\Qt5Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\Qt5WinExtras.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\src\Celeste\packaging\audio\*"; DestDir: "{app}\audio"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\src\Celeste\packaging\bearer\*"; DestDir: "{app}\bearer"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\src\Celeste\packaging\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\src\Celeste\packaging\mediaservice\*"; DestDir: "{app}\mediaservice"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\src\Celeste\packaging\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\src\Celeste\packaging\playlistformats\*"; DestDir: "{app}\playlistformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\src\Celeste\packaging\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Components]
Name: "celeste"; Description: "Celeste"; Types: full compact custom; Flags: fixed
Name: "pulsar"; Description: "Pulsar OBS Studio Plugin"; Types: full

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
var
  PulsarPrompted: Boolean;
  Pulsar: Boolean;
  OBSFolderSelect: TInputDirWizardPage;

function InitializeSetup(): Boolean;
begin
  PulsarPrompted := False;
  Pulsar := False
  Result := True
end;

procedure InitializeWizard();
var
  OBSInstallationPath: String;
begin
  OBSInstallationPath := ExpandConstant('{autopf}');
  if RegQueryStringValue(HKLM,'SOFTWARE\OBS Studio','',OBSInstallationPath) then
  begin
    OBSInstallationPath := OBSInstallationPath + '\obs-plugins\64bit';
  end;
  OBSFolderSelect := CreateInputDirPage(wpSelectComponents,'Locate OBS Studio','Please locate your OBS Studio installation''s plugins folder','',False,'');
  OBSFolderSelect.Add('Plugins Folder');
  OBSFolderSelect.Values[0] := OBSInstallationPath;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = OBSFolderSelect.ID then
  begin
    Result := not WizardIsComponentSelected('pulsar');
  end;
end;

function OBSLocation(Directory: String): String;
begin
  Result := OBSFolderSelect.Values[0];
end;