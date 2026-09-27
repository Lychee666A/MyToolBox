; ============================================================
;  MyToolBox 安装包脚本（Inno Setup 6）
;  由 build.bat 通过 /D 传入：
;    /DMyAppVersion=1.0.0
;    /DMySourceDir=E:\...\build\Release
;    /DMyOutputDir=E:\...\build\installer
; ============================================================

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#ifndef MySourceDir
  #define MySourceDir "..\build\Release"
#endif
#ifndef MyOutputDir
  #define MyOutputDir "..\build\installer"
#endif

#define MyAppName      "多功能轻量工具箱"
#define MyAppPublisher "Lychee666"
#define MyAppCopyright "Copyright (C) 2025 Lychee666. All rights reserved."
#define MyAppExeName   "MyToolBox.exe"

[Setup]
AppId={{8F3C2A1B-4D5E-4F6A-9B7C-1E2D3F4A5B6C}
AppName={#MyAppName}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppCopyright={#MyAppCopyright}
DefaultDirName={localappdata}\MyToolBox
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
LicenseFile=..\resources\license.txt

OutputDir={#MyOutputDir}
OutputBaseFilename=MyToolBox_Setup_{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\resources\icon.ico

AppPublisherURL=https://github.com/Lychee666A/MyToolBox
AppSupportURL=https://github.com/Lychee666A/MyToolBox/issues
AppUpdatesURL=https://github.com/Lychee666A/MyToolBox/releases

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "startmenuicon"; Description: "创建开始菜单快捷方式"; GroupDescription: "附加任务:"; Flags: checkedonce
Name: "desktopicon";   Description: "创建桌面快捷方式";     GroupDescription: "附加任务:"; Flags: unchecked

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; \
    Excludes: "*.pdb,*.ilk,*.exp,*.lib,WebView2Data\*,MyToolBox.log,MyToolBox.ini"

[Icons]
Name: "{group}\{#MyAppName}";                 Filename: "{app}\{#MyAppExeName}"; Tasks: startmenuicon
Name: "{group}\卸载 {#MyAppName}";            Filename: "{uninstallexe}";        Tasks: startmenuicon
Name: "{userdesktop}\{#MyAppName}";           Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "立即运行 {#MyAppName}"; \
    Flags: nowait postinstall skipifsilent
Filename: "https://github.com/Lychee666A/MyToolBox/"; \
    Description: "访问 GitHub 项目主页（查看更新、反馈问题）"; \
    Flags: shellexec postinstall skipifsilent nowait

[UninstallDelete]
Type: filesandordirs; Name: "{app}\WebView2Data"
Type: files;          Name: "{app}\MyToolBox.log"
Type: files;          Name: "{app}\MyToolBox.ini"

; ============================================================
;  [Code] 禁止路径含空格
; ============================================================
[Code]
function HasSpace(const S: String): Boolean;
var
  I: Integer;
begin
  Result := False;
  for I := 1 to Length(S) do
  begin
    if S[I] = ' ' then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  // 安装目录选择页
  if CurPageID = wpSelectDir then
  begin
    if HasSpace(WizardDirValue) then
    begin
      MsgBox('安装路径不能包含空格。' + #13#10 + #13#10 +
             '当前路径：' + WizardDirValue + #13#10 + #13#10 +
             '请重新选择一个不含空格的路径（例如：' +
             ExpandConstant('{localappdata}') + '\MyToolBox）。',
             mbError, MB_OK);
      Result := False;
      Exit;
    end;
  end;
end;

function InitializeSetup(): Boolean;
var
  OldVersion: String;
begin
  Result := True;
  // 检测是否已安装
  if RegQueryStringValue(HKEY_CURRENT_USER,
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\{8F3C2A1B-4D5E-4F6A-9B7C-1E2D3F4A5B6C}_is1',
    'DisplayVersion', OldVersion) then
  begin
    // 已有旧版本
    if MsgBox('检测到已安装版本 ' + OldVersion + '。' + #13#10 +
              '是否继续安装（会覆盖）？',
              mbConfirmation, MB_YESNO) = IDNO then
    begin
      Result := False;
    end;
  end;
end;