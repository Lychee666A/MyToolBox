#pragma once

#include <QString>
#include <QSysInfo>
#include <QtGlobal>

namespace AppInfo {
	
#ifdef MYTOOLBOX_VERSION
	inline QString Version()     { return QStringLiteral(MYTOOLBOX_VERSION); }
#else
	inline QString Version()     { return QStringLiteral("1.0.0"); }
#endif
	
#ifdef MYTOOLBOX_DISPLAY_NAME
	inline QString DisplayName() { return QStringLiteral(MYTOOLBOX_DISPLAY_NAME); }
#else
	inline QString DisplayName() { return QStringLiteral("多功能轻量工具箱"); }
#endif
	
	inline QString Name()          { return QStringLiteral("MyToolBox"); }
	inline QString DisplayNameEn() { return QStringLiteral("MyToolBox"); }
	inline QString Publisher()     { return QStringLiteral("Lychee666"); }
	inline QString Copyright()     { return QStringLiteral("Copyright © 2025 Lychee666"); }
	inline QString License()       { return QStringLiteral("GPLv3"); }
	inline QString Homepage()      { return QStringLiteral("https://github.com/Lychee666A/MyToolBox"); }
	inline QString Email()         { return QStringLiteral("785114852@qq.com"); }
	inline QString Description()   { return QStringLiteral(
														   "一个集成拍照 / 录像 / 录屏 / 播放 / 压缩包预览 / 图片查看 / 文件搜索 / 内嵌浏览器的轻量工具箱"); }
	
	inline QString BuildDate()     { return QStringLiteral(__DATE__); }
	inline QString BuildTime()     { return QStringLiteral(__TIME__); }
	inline QString BuildDateTime() { return QStringLiteral(__DATE__ " " __TIME__); }
	
	inline QString FullInfoText()
	{
		return QString(
					   "%1 v%2\n"
					   "Qt %3\n"
					   "OS: %4 (%5)\n"
					   "Build: %6\n"
					   "License: %7\n"
					   "%8")
		.arg(DisplayName(),
			 Version(),
			 QT_VERSION_STR,
			 QSysInfo::prettyProductName(),
			 QSysInfo::currentCpuArchitecture(),
			 BuildDateTime(),
			 License(),
			 Copyright());
	}
	
} // namespace AppInfo
