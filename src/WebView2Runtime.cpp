#include "WebView2Runtime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>

#include <windows.h>
#include <WebView2.h>
#include <shellapi.h>

WebView2Runtime &WebView2Runtime::instance()
{
	static WebView2Runtime s;
	return s;
}

WebView2Runtime::WebView2Runtime() : QObject(nullptr) {}

bool WebView2Runtime::isInstalled() const
{
	LPWSTR version = nullptr;
	HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
	if (SUCCEEDED(hr) && version != nullptr) {
		CoTaskMemFree(version);
		return true;
	}
	return false;
}

QString WebView2Runtime::installedVersion() const
{
	LPWSTR version = nullptr;
	HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
	if (SUCCEEDED(hr) && version != nullptr) {
		QString v = QString::fromWCharArray(version);
		CoTaskMemFree(version);
		return v;
	}
	return QString();
}

bool WebView2Runtime::silentInstall(const QString &installerPath)
{
	if (!QFileInfo::exists(installerPath)) {
		qWarning() << "安装器不存在:" << installerPath;
		return false;
	}
	
	qDebug() << "开始静默安装 WebView2 Runtime:" << installerPath;
	
	QProcess proc;
	proc.setProgram(installerPath);
	proc.setArguments({ "/silent", "/install" });
	proc.setProcessChannelMode(QProcess::MergedChannels);
	
	proc.start();
	if (!proc.waitForStarted(10000)) {
		qWarning() << "安装器启动失败:" << proc.errorString();
		return false;
	}
	if (!proc.waitForFinished(10 * 60 * 1000)) {
		qWarning() << "安装器超时";
		proc.kill();
		return false;
	}
	
	const int code = proc.exitCode();
	qDebug() << "安装器退出码:" << code;
	
	if (code != 0 && code != 3010) {
		qWarning() << "安装器返回错误码:" << code;
		return false;
	}
	
	if (isInstalled()) {
		qDebug() << "WebView2 Runtime 安装成功，版本:" << installedVersion();
		return true;
	}
	qWarning() << "安装器返回成功，但 Runtime 未检测到";
	return false;
}

void WebView2Runtime::installAsync(const QString &installerPath)
{
	if (m_installing) {
		emit installProgress(tr("安装已在进行中…"));
		return;
	}
	if (!QFileInfo::exists(installerPath)) {
		emit installFinished(false, tr("找不到安装器：%1").arg(installerPath));
		return;
	}
	
	m_installing = true;
	emit installProgress(tr("正在安装 WebView2 Runtime，请稍候…"));
	
	auto *proc = new QProcess(this);
	proc->setProgram(installerPath);
	proc->setArguments({ "/silent", "/install" });
	proc->setProcessChannelMode(QProcess::MergedChannels);
	
	connect(proc,
			QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this,
			[this, proc](int exitCode, QProcess::ExitStatus status) {
				proc->deleteLater();
				m_installing = false;
				
				if (status != QProcess::NormalExit) {
					emit installFinished(false, tr("安装进程异常退出。"));
					return;
				}
				// 0 成功，3010 需要重启（也算成功，Runtime 已就绪）
				if (exitCode != 0 && exitCode != 3010) {
					emit installFinished(false,
										 tr("安装器返回错误码：%1").arg(exitCode));
					return;
				}
				if (isInstalled()) {
					emit installFinished(true,
										 tr("安装成功，版本：%1").arg(installedVersion()));
				} else {
					emit installFinished(false,
										 tr("安装器执行完毕，但未检测到 Runtime。"));
				}
			});
	
	connect(proc, &QProcess::errorOccurred, this,
			[this, proc](QProcess::ProcessError err) {
				// finished 也会触发；这里只处理启动失败
				if (err == QProcess::FailedToStart) {
					proc->deleteLater();
					m_installing = false;
					emit installFinished(false, tr("安装器启动失败。"));
				}
			});
	
	proc->start();
}

bool WebView2Runtime::ensureAvailable()
{
	if (isInstalled()) return true;
	
	const QString appDir = QCoreApplication::applicationDirPath();
	const QString installerPath =
	QDir(appDir).filePath("MicrosoftEdgeWebview2Setup.exe");
	
	if (!QFileInfo::exists(installerPath)) {
		qWarning() << "找不到 WebView2 安装器:" << installerPath;
		return false;
	}
	return silentInstall(installerPath);
}
