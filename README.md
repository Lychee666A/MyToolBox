# MyToolBox 多功能轻量工具箱

<<<<<<< HEAD
> 集成拍照、录像、录屏、播放、压缩包预览、图片查看、文件搜索、内嵌浏览器的轻量工具箱

---

## 目录

- [简介](#简介)
- [功能一览](#功能一览)
- [技术栈](#技术栈)
- [环境要求](#环境要求)
- [目录结构](#目录结构)
- [编译构建](#编译构建)
- [打包安装](#打包安装)
- [独立浏览器 Helper](#独立浏览器-helper)
- [快捷键](#快捷键)
- [配置说明](#配置说明)
- [第三方依赖](#第三方依赖)
- [常见问题](#常见问题)
- [许可协议](#许可协议)

---

## 简介

**MyToolBox** 是一个 Windows 桌面端的多功能轻量工具箱，将日常开发、办公中常用的小工具集成到一个程序中，避免安装一堆独立软件。

主要特点：

- **单程序集成** —— 拍照、录屏、播放、压缩包、图片、搜索、浏览器一站搞定
- **标签页式布局** —— 顶部 Tab 快速切换各功能模块
- **内嵌浏览器** —— 基于微软 WebView2，无需依赖外部浏览器
- **独立浏览器可执行文件** —— `MyToolBoxBrowser.exe` 可被第三方程序调用
- **深/浅主题** —— 支持一键切换亮色/暗色
- **系统托盘** —— 最小化到托盘，录像/录音后台不中断

---

## 功能一览

| 模块 | 功能 |
|------|------|
| **拍照 / 录像 / 录音** | 调用摄像头拍照、录像（H.264/MP4），独立录音（AAC/M4A） |
| **录屏 / 截屏** | 全屏/窗口录屏（H.264），支持系统声音 + 麦克风，截屏保存 PNG/JPG |
| **音视频播放** | 基于 VLC，支持几乎所有常见音视频格式 |
| **压缩包预览** | 基于 bit7z，直接预览压缩包内容，无需解压 |
| **图片查看** | 支持缩放、适应窗口、背景色自定义 |
| **快捷网页** | 收藏常用网址，双击在**内置浏览器**打开 |
| **文件搜索** | 基于 Everything SDK，毫秒级全盘搜索 |
| **浏览器** | 基于 WebView2，多标签、历史记录、下载记录、快捷键齐全 |

---

## 技术栈

| 项目 | 版本 / 说明 |
|------|-------------|
| **语言** | C++17 |
| **框架** | Qt 6.11.2 (MSVC 2022 64-bit) |
| **编译器** | MSVC 2022 (Visual Studio 2026) |
| **构建** | CMake 3.19+ |
| **打包** | Inno Setup 6 |
| **浏览器内核** | Microsoft Edge WebView2 |

---

## 环境要求

### 开发环境

- **操作系统**：Windows 10 / 11 (x64)
- **Qt**：`F:\Qt\6.11.2\msvc2022_64`（路径可在 `build.bat` 中修改）
- **Visual Studio**：2022 / 2026，需含 C++ 桌面开发工作负载
- **CMake**：≥ 3.19
- **Inno Setup**：6.x（用于打包，可选）

### 运行环境

- Windows 10 1809+ / Windows 11
- **Microsoft Edge WebView2 Runtime**（首次运行浏览器会提示安装，也可用程序内下载引导）

---

## 目录结构

```
MyToolBox/
├── CMakeLists.txt
├── build.bat                # 一键构建 + 打包
├── build_inner.bat          # MSVC 环境下的构建脚本
├── README.md
├── src/                     # 主程序源码
│   ├── main.cpp
│   ├── MainWindow.h/.cpp
│   ├── CameraRecorder.h/.cpp
│   ├── ScreenRecorderWidget.h/.cpp
│   ├── WasapiLoopbackCapture.h/.cpp
│   ├── VlcPlayerWidget.h/.cpp
│   ├── ArchivePreviewWidget.h/.cpp
│   ├── ImageViewerWidget.h/.cpp
│   ├── QuickLinksWidget.h/.cpp
│   ├── FileSearchWidget.h/.cpp
│   ├── EverythingApi.h/.cpp
│   ├── WebBrowserWidget.h/.cpp      # 内嵌浏览器
│   ├── WebPageView.h/.cpp           # WebView2 单页封装
│   ├── WebView2Runtime.h/.cpp       # WebView2 检测/安装
│   ├── BrowserHistory.h/.cpp        # 历史记录
│   ├── DownloadsManager.h/.cpp      # 下载记录
│   ├── HistoryDialog.h/.cpp
│   ├── DownloadsDialog.h/.cpp
│   ├── SettingsManager.h/.cpp
│   ├── SettingsDialog.h/.cpp
│   ├── SearchEngineDialog.h/.cpp
│   ├── ThemeManager.h/.cpp
│   ├── AboutDialog.h/.cpp
│   └── AppInfo.h
├── helper/                  # 独立浏览器
│   └── main.cpp
├── resources/
│   ├── icon.ico
│   ├── resources.qrc
│   ├── version.rc.in
│   └── version_helper.rc.in
├── installer/
│   └── MyToolBox.iss        # Inno Setup 脚本
└── third_party/
    ├── bit7z/
    ├── vlc/
    ├── everything/
    └── WebView2/
```

---

## 编译构建

### 方式一：一键脚本（推荐）

双击运行 `build.bat`，或命令行：

```bat
:: 完整编译 + 询问是否打包
build.bat

:: 完整编译，不打包
build.bat --build-only

:: 完整编译 + 直接打包
build.bat --pack

:: 跳过编译，只打包（要求 build\Release 已存在）
build.bat --pack
```

### 方式二：手动 CMake

```bat
:: 1. 进入 MSVC 环境
call "E:\Microsoft\...\VC\Auxiliary\Build\vcvars64.bat"

:: 2. 加 Qt 到 PATH
set PATH=F:\Qt\6.11.2\msvc2022_64\bin;%PATH%

:: 3. 配置
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_PREFIX_PATH="F:\Qt\6.11.2\msvc2022_64" ..

:: 4. 构建
cmake --build . --config Release --parallel
```

### 构建产物

```
build/Release/
├── MyToolBox.exe                        # 主程序
├── MyToolBoxBrowser.exe                 # 独立浏览器
├── Qt6Core.dll / Qt6Gui.dll / Qt6Widgets.dll
├── Qt6Multimedia.dll / Qt6MultimediaWidgets.dll
├── WebView2Loader.dll
├── MicrosoftEdgeWebView2RuntimeInstallerX64.exe
├── 7z.dll / libvlc.dll / libvlccore.dll / plugins/
├── Everything.exe / Everything64.dll / Everything.lng
└── ...
```

---

## 打包安装

### 前置

1. 安装 [Inno Setup 6](https://jrsoftware.org/isdl.php)
2. 确保 `installer/MyToolBox.iss` 存在
3. 编译成功

### 打包

```bat
build.bat --pack
```

安装包输出到：

```
build/installer/MyToolBox_Setup_1.0.0.exe
```

### 安装路径规则

安装程序**默认安装到用户目录**，避免 `Program Files` 写权限问题：

```
%LOCALAPPDATA%\MyToolBox\
```

同时**禁止**用户选择含空格的路径（Inno Setup `[Code]` 段校验）。

---

## 独立浏览器 Helper

`MyToolBoxBrowser.exe` 是**从主程序抽出的独立浏览器**，可被第三方程序调用。

### 命令行用法

```bat
:: 打开 hao123（默认主页）
MyToolBoxBrowser.exe

:: 打开单个网址
MyToolBoxBrowser.exe https://www.baidu.com
MyToolBoxBrowser.exe baidu.com

:: 多标签
MyToolBoxBrowser.exe https://www.baidu.com https://www.bilibili.com

:: 独立窗口
MyToolBoxBrowser.exe --new-window https://www.baidu.com

:: 搜索
MyToolBoxBrowser.exe "Qt WebView2 教程"

:: 帮助
MyToolBoxBrowser.exe --help
```

### 从其他程序调用

```cpp
// C++
ShellExecuteW(nullptr, L"open",
    L"C:\\Path\\To\\MyToolBoxBrowser.exe",
    L"https://www.baidu.com",
    nullptr, SW_SHOWNORMAL);
```

```csharp
// C#
System.Diagnostics.Process.Start(
    "MyToolBoxBrowser.exe",
    "https://www.baidu.com");
```

```python
# Python
import subprocess
subprocess.Popen([r"C:\Path\To\MyToolBoxBrowser.exe",
                  "https://www.baidu.com"])
```

---

## 快捷键

### 标签页

| 快捷键 | 功能 |
|--------|------|
| `Ctrl + T` | 新建标签页 |
| `Ctrl + W` / `Ctrl + F4` | 关闭当前标签 |
| `Ctrl + Tab` | 下一个标签 |
| `Ctrl + Shift + Tab` | 上一个标签 |
| `Ctrl + 1` ~ `8` | 切换到第 N 个标签 |
| `Ctrl + 9` | 切换到最后一个标签 |
| `Ctrl + Shift + T` | 恢复最近关闭的标签 |

### 页面操作

| 快捷键 | 功能 |
|--------|------|
| `F5` / `Ctrl + R` | 刷新 |
| `Ctrl + Shift + R` | 忽略缓存刷新 |
| `Ctrl + +` / `Ctrl + =` | 放大 |
| `Ctrl + -` | 缩小 |
| `Ctrl + 0` | 重置缩放 |
| `Ctrl + L` / `Alt + D` | 选中地址栏 |
| `Ctrl + Enter` | 地址栏补全 www / .com |
| `Ctrl + F` | 页面查找 |
| `Ctrl + G` | 下一个查找结果 |
| `Esc` | 停止加载 / 关闭查找栏 |

### 特殊功能

| 快捷键 | 功能 |
|--------|------|
| `F12` / `Ctrl + Shift + I` | 开发者工具 |
| `Ctrl + H` | 历史记录 |
| `Ctrl + J` | 下载记录 |
| `Ctrl + Shift + N` | InPrivate 模式 |
| `Ctrl + Shift + Delete` | 清除数据 |
| `Alt + Home` | 主页 |

---

## 配置说明

所有配置通过 `QSettings` 持久化，位置：

```
HKEY_CURRENT_USER\Software\MyToolBox\MyToolBox
```

**主程序与 Helper 共享同一份配置**（`applicationName` 与 `organizationName` 相同）。

### 主要配置项

| 键 | 说明 | 默认值 |
|----|------|--------|
| `browser/homeUrl` | 浏览器主页 | `https://www.hao123.com` |
| `browser/defaultSearchEngineId` | 默认搜索引擎 | `baidu` |
| `browser/searchEngines` | 搜索引擎列表 | 百度 / Bing / Google / 搜狗 |
| `general/restoreLastTab` | 启动时恢复上次标签 | `true` |
| `general/lastTabIndex` | 上次标签索引 | `0` |
| `image/keepAspect` | 图片保持宽高比 | `true` |
| `image/backgroundColor` | 图片背景色 | `#222222` |
| `screen/fps` | 录屏帧率 | `30` |
| `screen/codec` | 录屏编码 | `H264` |
| `screen/includeCursor` | 录屏包含光标 | `true` |
| `camera/outputDir` | 摄像头输出目录 | `%USERPROFILE%\Videos` |
| `filesearch/pageSize` | 文件搜索分页大小 | `1000` |

### WebView2 用户数据目录

```
%LOCALAPPDATA%\MyToolBox\MyToolBox\
├── WebView2Data_MyToolBox\          # 主程序
└── WebView2Data_MyToolBoxBrowser\   # 独立浏览器
```

两者独立，可同时运行不冲突。

---

## 第三方依赖

| 库 | 用途 | 许可 |
|----|------|------|
| **Qt 6** | UI 框架 | LGPLv3 / 商业 |
| **WebView2 SDK** | 内嵌浏览器 | Microsoft 许可 |
| **VLC (libvlc)** | 音视频播放 | LGPLv2.1+ |
| **bit7z** | 压缩包解析 | MIT |
| **7-Zip** | 压缩算法 | LGPL + unRAR restriction |
| **Everything SDK** | 文件搜索 | MIT |

---

## 常见问题

### Q1: 启动时弹出"Microsoft Edge 无法读取和写入其数据目录"

**原因**：WebView2 数据目录不可写。

**解决**：程序已自动将数据目录放到 `%LOCALAPPDATA%\MyToolBox\`。若仍报错，请检查该目录权限。

### Q2: 浏览器页签显示"需要 WebView2 Runtime"

**原因**：系统未安装 Microsoft Edge WebView2 Runtime。

**解决**：
1. 点击程序内"下载并安装"按钮
2. 或访问 [微软官方下载页](https://go.microsoft.com/fwlink/p/?LinkId=2124703)

### Q3: 编译报错 `Permission denied` / 无法写入 `.obj`

**原因**：上次的 `MyToolBox.exe` 未退出，或杀毒软件锁文件。

**解决**：
1. 关闭所有 `MyToolBox*.exe`
2. 关闭杀毒软件实时防护
3. 删除 `build/` 目录重新构建

### Q4: 打包时报 `%\Inno was unexpected at this time`

**原因**：命令行 `echo %ProgramFiles(x86)%` 括号被 CMD 误解。

**解决**：已修复（使用 `set ISCC` 而非直接 echo）。请使用最新版 `build.bat`。

### Q5: 安装到 `Program Files` 后浏览器报错

**原因**：`Program Files` 只读。

**解决**：安装程序已限制默认装到 `%LOCALAPPDATA%`，且禁止含空格路径。请保持默认路径。

### Q6: 浏览器标签页的关闭按钮不显示

**原因**：全局 QSS 覆盖了 `QTabBar::close-button`。

**解决**：`WebBrowserWidget` 已显式设置关闭按钮图标。若仍不显示，检查 `ThemeManager` 的 QSS 里是否有 `QTabBar::close-button { image: none; }`。

---

## 许可协议

```
Copyright (C) 2025 Lychee666. All rights reserved.
```

本软件为私有软件。未经作者书面许可，不得复制、修改、分发或用于商业用途。

---

## 致谢

感谢以下开源项目：

- [Qt](https://www.qt.io/)
- [VLC](https://www.videolan.org/vlc/)
- [7-Zip / bit7z](https://github.com/rikyoz/bit7z)
- [Everything](https://www.voidtools.com/)
- [Microsoft WebView2](https://developer.microsoft.com/microsoft-edge/webview2/)

---

*最后更新：2025*
=======
> 🧰 集拍照、录像、录屏、播放、压缩包预览、图片查看、文件搜索、内嵌浏览器于一体的 Windows 桌面工具箱

[![Qt](https://img.shields.io/badge/Qt-6.11.2-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)]()
[![Compiler](https://img.shields.io/badge/MSVC-2022-5C2D91?logo=visualstudio&logoColor=white)]()
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)]()
[![CMake](https://img.shields.io/badge/CMake-3.19%2B-064F8C?logo=cmake&logoColor=white)]()
[![License](https://img.shields.io/badge/License-Proprietary-red.svg)]()

---

**MyToolBox** 是一个 Windows 桌面端的多功能轻量工具箱，将日常开发、办公中常用的小工具集成到一个程序中，避免安装一堆独立软件。
>>>>>>> e6e697f833300592405fa4a6b7e67918746e14de
