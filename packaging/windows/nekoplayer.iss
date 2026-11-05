; Inno Setup script for the NekoPlayer portable payload.
; Build the payload with packaging/windows/deploy.sh, then run:
;   iscc /DAppVersion=0.2.0 /DPayload=..\..\build\windows-portable packaging/windows/nekoplayer.iss
; The installer is optional; the portable zip remains the primary artifact.

#ifndef AppVersion
  #define AppVersion "0.2.0"
#endif
#ifndef Payload
  #define Payload "..\..\build\windows-portable"
#endif

[Setup]
AppId={{6E1B7C43-9B3A-4E42-9D7B-3B0B7C0D5A11}
AppName=NekoPlayer
AppVersion={#AppVersion}
AppPublisher=NekoRain
AppPublisherURL=https://github.com/NekoRain404/NekoPlayer
AppSupportURL=https://github.com/NekoRain404/NekoPlayer/issues
AppUpdatesURL=https://github.com/NekoRain404/NekoPlayer/releases
DefaultDirName={localappdata}\Programs\NekoPlayer
DefaultGroupName=NekoPlayer
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\..\build\windows-installer
OutputBaseFilename=nekoplayer-{#AppVersion}-windows-x86_64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\NekoPlayer.exe
LicenseFile=..\..\LICENSE

[Tasks]
Name: "associatevideo"; Description: "Associate video files with NekoPlayer"; GroupDescription: "File associations:"; Flags: checkedonce
Name: "associateaudio"; Description: "Associate audio files with NekoPlayer"; GroupDescription: "File associations:"; Flags: checkedonce
Name: "associateplaylists"; Description: "Associate playlist files with NekoPlayer"; GroupDescription: "File associations:"; Flags: checkedonce
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
Source: "{#Payload}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\NekoPlayer"; Filename: "{app}\NekoPlayer.exe"; AppUserModelID: "NekoRain.NekoPlayer"
Name: "{autodesktop}\NekoPlayer"; Filename: "{app}\NekoPlayer.exe"; Tasks: desktopicon; AppUserModelID: "NekoRain.NekoPlayer"

[Registry]
Root: HKCU; Subkey: "Software\Classes\.mkv"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.mp4"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.avi"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.mov"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.webm"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.m4v"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.ts"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.m2ts"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.mpg"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.mpeg"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.wmv"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Media"; Flags: uninsdeletevalue; Tasks: associatevideo
Root: HKCU; Subkey: "Software\Classes\.mp3"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Audio"; Flags: uninsdeletevalue; Tasks: associateaudio
Root: HKCU; Subkey: "Software\Classes\.flac"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Audio"; Flags: uninsdeletevalue; Tasks: associateaudio
Root: HKCU; Subkey: "Software\Classes\.aac"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Audio"; Flags: uninsdeletevalue; Tasks: associateaudio
Root: HKCU; Subkey: "Software\Classes\.m4a"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Audio"; Flags: uninsdeletevalue; Tasks: associateaudio
Root: HKCU; Subkey: "Software\Classes\.ogg"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Audio"; Flags: uninsdeletevalue; Tasks: associateaudio
Root: HKCU; Subkey: "Software\Classes\.opus"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Audio"; Flags: uninsdeletevalue; Tasks: associateaudio
Root: HKCU; Subkey: "Software\Classes\.wav"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Audio"; Flags: uninsdeletevalue; Tasks: associateaudio
Root: HKCU; Subkey: "Software\Classes\.m3u8"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Playlist"; Flags: uninsdeletevalue; Tasks: associateplaylists
Root: HKCU; Subkey: "Software\Classes\.m3u"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Playlist"; Flags: uninsdeletevalue; Tasks: associateplaylists
Root: HKCU; Subkey: "Software\Classes\.pls"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Playlist"; Flags: uninsdeletevalue; Tasks: associateplaylists
Root: HKCU; Subkey: "Software\Classes\.xspf"; ValueType: string; ValueName: ""; ValueData: "NekoPlayer.Playlist"; Flags: uninsdeletevalue; Tasks: associateplaylists
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Media"; ValueType: string; ValueName: ""; ValueData: "Media file"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Media\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\NekoPlayer.exe,0"
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Media\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\NekoPlayer.exe"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Audio"; ValueType: string; ValueName: ""; ValueData: "Audio file"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Audio\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\NekoPlayer.exe,0"
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Audio\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\NekoPlayer.exe"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Playlist"; ValueType: string; ValueName: ""; ValueData: "Playlist"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Playlist\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\NekoPlayer.exe,0"
Root: HKCU; Subkey: "Software\Classes\NekoPlayer.Playlist\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\NekoPlayer.exe"" ""%1"""

[Run]
Filename: "{app}\NekoPlayer.exe"; Description: "Launch NekoPlayer"; Flags: nowait postinstall skipifsilent
