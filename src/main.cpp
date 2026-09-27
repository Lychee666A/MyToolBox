#include <QApplication>
#include <QSettings>
#include <QIcon>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QSplashScreen>
#include <QTimer>
#include <QFont>
#include <QDateTime>
#include <windows.h>
#include <shellapi.h>
#include "MainWindow.h"
#include "ThemeManager.h"
#include "AppInfo.h"

// 检查 Everything IPC 窗口是否存在
static bool isEverythingRunning()
{
	return ::FindWindowW(L"EVERYTHING_TASKBAR_NOTIFICATION", nullptr) != nullptr;
}

// 启动内置 Everything.exe（如未运行）
static void ensureEverythingRunning()
{
	const QString exePath = QDir(QCoreApplication::applicationDirPath())
	.filePath("Everything.exe");
	
	if (!QFileInfo::exists(exePath)) {
		qWarning() << "Everything.exe 未找到：" << exePath;
		return;
	}
	
	if (isEverythingRunning()) {
		qDebug() << "Everything 已在运行";
		return;
	}
	
	SHELLEXECUTEINFOW sei = { sizeof(sei) };
	sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
	sei.lpVerb       = L"runas";
	sei.lpFile       = reinterpret_cast<LPCWSTR>(exePath.utf16());
	sei.lpParameters = L"-startup -minimized";
	sei.nShow        = SW_HIDE;
	
	if (!ShellExecuteExW(&sei)) {
		DWORD err = GetLastError();
		qWarning() << "ShellExecuteEx(runas) 失败，GetLastError =" << err;
		if (err == ERROR_CANCELLED) {
			qWarning() << "用户取消了 UAC 提权";
		}
		return;
	}
	
	qDebug() << "Everything 已启动（提权）";
	
	for (int i = 0; i < 50; ++i) {
		if (isEverythingRunning()) {
			qDebug() << "Everything IPC 窗口已就绪，用时" << (i * 100) << "ms";
			break;
		}
		::Sleep(100);
	}
	
	if (!isEverythingRunning()) {
		qWarning() << "Everything IPC 窗口 5 秒内未出现";
	}
	
	if (sei.hProcess) CloseHandle(sei.hProcess);
}

// ============================================================
//                        Splash 辅助
// ============================================================
static QSplashScreen *createSplash()
{
	QPixmap pix(":/icon.jpg");
	if (pix.isNull()) {
		qWarning() << "[Splash] 无法加载 :/icon.jpg，跳过启动画面";
		return nullptr;
	}
	
	// 限制最大尺寸，避免超大图变糊或占满屏幕
	if (pix.width() > 640 || pix.height() > 400) {
		pix = pix.scaled(640, 400,
						 Qt::KeepAspectRatio,
						 Qt::SmoothTransformation);
	}
	
	auto *splash = new QSplashScreen(pix);
	splash->setWindowFlag(Qt::WindowStaysOnTopHint, true);
	splash->setWindowFlag(Qt::FramelessWindowHint, true);
	
	// 底部居中的提示文字
	QFont f = splash->font();
	f.setPointSize(10);
	splash->setFont(f);
	
	splash->showMessage(
						QString("%1 v%2   正在启动…")
						.arg(AppInfo::DisplayName(), AppInfo::Version()),
						Qt::AlignBottom | Qt::AlignHCenter,
						Qt::white);
	
	splash->show();
	QApplication::processEvents();   // 立即绘制
	return splash;
}

// ============================================================
//                        main
// ============================================================
int main(int argc, char *argv[])
{
	// ---- 控制台窗口处理 ----
	bool showConsole = false;
	for (int i = 1; i < argc; ++i) {
		if (qstrcmp(argv[i], "-c") == 0) { showConsole = true; break; }
	}
	if (!showConsole) {
		HWND hwnd = GetConsoleWindow();
		if (hwnd) ShowWindow(hwnd, SW_HIDE);
	}
	
	// ---- QApplication ----
	QApplication app(argc, argv);
	
	// ★ 用 AppInfo 统一设置
	app.setApplicationName(AppInfo::Name());
	app.setOrganizationName(AppInfo::Name());
	app.setApplicationVersion(AppInfo::Version());
	app.setApplicationDisplayName(AppInfo::DisplayName());
	
	QApplication::setQuitOnLastWindowClosed(false);
	
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
					   QApplication::applicationDirPath());
	
	// 任务栏 / 窗口图标（ico）
	app.setWindowIcon(QIcon(":/icon.ico"));
	
	// ---- 显示启动画面（icon.jpg） ----
	QSplashScreen *splash = createSplash();
	
	// 让 splash 至少显示一小会儿（即使后续初始化很快）
	const int kSplashMinMs = 800;
	const qint64 splashStart = QDateTime::currentMSecsSinceEpoch();
	
	// ---- 加载主题 ----
	ThemeManager::instance().loadFromSettings();
	ThemeManager::instance().apply(&app);
	
	// ---- 启动 Everything ----
	ensureEverythingRunning();
	
	// ---- 创建主窗口 ----
	MainWindow w;
	w.resize(1100, 720);
	w.show();
	
	// ---- 关闭 splash ----
	if (splash) {
		const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - splashStart;
		const int wait = qMax(0, int(kSplashMinMs - elapsed));
		
		QTimer::singleShot(wait, splash, [splash, &w]() {
			splash->finish(&w);       // 主窗口激活后淡出
			splash->deleteLater();
		});
	}
	
	return app.exec();
}
