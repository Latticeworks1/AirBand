; Inno Setup script for the AirBand Windows VST3 installer.
; Build with:
;   ISCC /DAppVersion=<x.y.z> /DSourceDir=<dir containing AirBand.vst3> /DRepoDir=<repo root> /O<outdir> installer\AirBand.iss

#ifndef AppVersion
  #error AppVersion must be defined
#endif
#ifndef SourceDir
  #error SourceDir must be defined
#endif
#ifndef RepoDir
  #error RepoDir must be defined
#endif

[Setup]
AppId={{28B284A1-659F-4A78-8BD0-ABD96BAEABC0}
AppName=AirBand
AppVersion={#AppVersion}
AppVerName=AirBand {#AppVersion}
AppPublisher=Latticeworks1
AppPublisherURL=https://github.com/Latticeworks1/AirBand
AppSupportURL=https://github.com/Latticeworks1/AirBand/issues
; The plugin goes into the shared VST3 folder; {app} only holds the uninstaller.
DefaultDirName={autopf}\AirBand
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallFilesDir={app}
UninstallDisplayName=AirBand {#AppVersion} (VST3)
LicenseFile={#RepoDir}\LICENSE
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=AirBand-windows-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no

[Files]
Source: "{#SourceDir}\AirBand.vst3\*"; DestDir: "{commoncf64}\VST3\AirBand.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\AirBand.vst3"

[Messages]
FinishedLabel=AirBand has been installed to the shared VST3 folder. Rescan plugins in your DAW to find it.
