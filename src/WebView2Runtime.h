#pragma once

#include <QObject>
#include <QString>

// WebView2 Runtime 检测与安装（单例，支持异步安装）
class WebView2Runtime : public QObject
{
	Q_OBJECT
public:
	static WebView2Runtime &instance();
	
	// 是否已安装
	bool isInstalled() const;
	// 获取已安装版本（未安装返回空）
	QString installedVersion() const;
	
	// 同步静默安装（阻塞，仅内部/兼容用；UI 请用 installAsync）
	bool silentInstall(const QString &installerPath);
	
	// 异步静默安装：立即返回；完成后发 installFinished
	// 期间发 installProgress 便于 UI 显示
	void installAsync(const QString &installerPath);
	
	// 是否正在安装
	bool isInstalling() const { return m_installing; }
	
	// 便捷：确保 WebView2 可用。如未安装，自动从程序目录查找安装器并同步安装
	bool ensureAvailable();
	
	signals:
	// ok=true 表示安装成功且已检测到 Runtime
	void installFinished(bool ok, const QString &message);
	// 进度文本（如"正在安装…"）
	void installProgress(const QString &text);
	
private:
	WebView2Runtime();
	Q_DISABLE_COPY(WebView2Runtime)
	
	bool m_installing = false;
};
