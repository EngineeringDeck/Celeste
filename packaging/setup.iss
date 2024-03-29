#define MyAppName "Celeste"
#define MyAppVersion "2.00"
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
Source: "{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "Pulsar.dll"; DestDir: "{code:OBSLocation}"; Flags: ignoreversion; Components: pulsar
Source: "libcrypto-3-x64.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "libssl-3-x64.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6Mqtt.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6Multimedia.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6MultimediaWidgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6Svg.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6WebSockets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "multimedia\*"; DestDir: "{app}\multimedia"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs createallsubdirs

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