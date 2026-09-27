// helper/main.cpp
// MyToolBox 独立浏览器（Helper）

#include "WebBrowserWidget.h"
#include "SettingsManager.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QVBoxLayout>
#include <QWidget>
#include <QIcon>
#include <QUrl>
#include <QDebug>
#include <QTimer>

// ============================================================
//  独立浏览器窗口
// ============================================================
class BrowserWindow : public QWidget
{
	Q_OBJECT
public:
	explicit BrowserWindow(QWidget *parent = nullptr)
	: QWidget(parent)
	{
		setWindowTitle(tr("MyToolBox 浏览器"));
		setWindowIcon(QIcon(":/icon.ico"));
		resize(1200, 780);
		setMinimumSize(700, 480);
		
		auto *lay = new QVBoxLayout(this);
		lay->setContentsMargins(0, 0, 0, 0);
		lay->setSpacing(0);
		
		m_browser = new WebBrowserWidget(this);
		lay->addWidget(m_browser, 1);
	}
	
	// ★ Kiosk（必须在 show 之前调用）
	void setKioskMode(bool enabled)
	{
		if (m_browser) m_browser->setKioskMode(enabled);
	}
	
	// ★ 设置初始 URL（必须在 show 之前调用）
	void setInitialUrl(const QString &url)
	{
		if (m_browser) m_browser->setInitialUrl(url);
	}
	
	// 保留兼容
	void navigate(const QString &url)
	{
		if (m_browser) m_browser->navigateTo(url);
	}
	
	WebBrowserWidget *browser() const { return m_browser; }
	
private:
	WebBrowserWidget *m_browser = nullptr;
};

// ============================================================
//  URL 规范化
// ============================================================
static QString normalizeUrl(const QString &raw)
{
	QString t = raw.trimmed();
	if (t.isEmpty()) return QString();
	
	if (t.startsWith("http://") || t.startsWith("https://")
		|| t.startsWith("file://") || t.startsWith("about:")
		|| t.startsWith("edge:") || t.startsWith("data:"))
		return t;
	
	if (t.contains('.') && !t.contains(' '))
		return "https://" + t;
	
	auto eng = SettingsManager::instance().defaultSearchEngine();
	if (!eng.url.isEmpty() && eng.url.contains("%1")) {
		QString u = eng.url;
		u.replace("%1", QString::fromUtf8(QUrl::toPercentEncoding(t)));
		return u;
	}
	return "https://www.baidu.com/s?ie=UTF-8&wd=" +
	QString::fromUtf8(QUrl::toPercentEncoding(t));
}

// ============================================================
//  main
// ============================================================
int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	
	QApplication::setApplicationName("MyToolBox");
	QApplication::setOrganizationName("MyToolBox");
	QApplication::setApplicationDisplayName(QObject::tr("MyToolBox 浏览器"));
	QApplication::setWindowIcon(QIcon(":/icon.ico"));
	
	ThemeManager::instance().loadFromSettings();
	ThemeManager::instance().apply(&app);
	
	QCommandLineParser parser;
	parser.setApplicationDescription(
									 QObject::tr("MyToolBox 独立浏览器（WebView2）"));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("url",
								 QObject::tr("要打开的网址，可多个"), "[url...]");
	
	QCommandLineOption optNewWindow(
									QStringList() << "n" << "new-window",
									QObject::tr("每个 URL 各开一个独立窗口"));
	parser.addOption(optNewWindow);
	
	QCommandLineOption optKiosk(
								QStringList() << "k" << "kiosk",
								QObject::tr("只读模式：禁用地址栏、F12、新建标签、菜单等交互"));
	parser.addOption(optKiosk);
	
	QCommandLineOption optTitle(
								QStringList() << "title",
								QObject::tr("自定义窗口标题"),
								"title");
	parser.addOption(optTitle);
	
	parser.process(app);
	
	const bool kiosk = parser.isSet(optKiosk);
	const QString customTitle = parser.value(optTitle);
	
	QStringList urls = parser.positionalArguments();
	if (urls.isEmpty()) urls << "https://www.hao123.com";
	
	// --new-window：每个 URL 一个窗口
	if (parser.isSet(optNewWindow)) {
		for (const QString &raw : urls) {
			QString u = normalizeUrl(raw);
			if (u.isEmpty()) continue;
			
			auto *w = new BrowserWindow;
			w->setAttribute(Qt::WA_DeleteOnClose);
			if (!customTitle.isEmpty())
				w->setWindowTitle(customTitle);
			
			// ★ 关键：先设初始 URL 和 kiosk，再 show
			w->setInitialUrl(u);
			w->setKioskMode(kiosk);
			w->show();
		}
	} else {
		auto *w = new BrowserWindow;
		w->setAttribute(Qt::WA_DeleteOnClose);
		if (!customTitle.isEmpty())
			w->setWindowTitle(customTitle);
		
		// ★ 第一个 URL 作为初始 URL
		QString first = normalizeUrl(urls.first());
		if (!first.isEmpty()) {
			w->setInitialUrl(first);
		}
		w->setKioskMode(kiosk);
		w->show();
		
		// 剩余的用 openUrlInNewTab
		QStringList rest = urls.mid(1);
		if (!rest.isEmpty()) {
			QTimer::singleShot(800, w, [w, rest]() {
				if (auto *bw = w->browser()) {
					for (const QString &raw : rest) {
						QString u = normalizeUrl(raw);
						if (u.isEmpty()) continue;
						bw->openUrlInNewTab(u);
					}
				}
			});
		}
	}
	
	return app.exec();
}

#include "main.moc"
