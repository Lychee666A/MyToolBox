// helper/main.cpp
// MyToolBox 独立浏览器（Helper）
// 复用主程序 WebBrowserWidget 的所有功能：
//   多标签页、前进/后退/刷新/主页、地址栏搜索、F12 开发者工具、
//   window.open / target=_blank 拦截为新标签、WebView2 检测与安装引导、主题跟随。

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
//  独立浏览器窗口：整窗就是一个 WebBrowserWidget
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
	
	// 不是网址 -> 走默认搜索引擎
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
	
	// ★ applicationName / organization 与主程序保持一致，
	//   这样 QSettings 共享（主页、搜索引擎、主题等设置两边一致）
	QApplication::setApplicationName("MyToolBox");
	QApplication::setOrganizationName("MyToolBox");
	QApplication::setApplicationDisplayName(QObject::tr("MyToolBox 浏览器"));
	QApplication::setWindowIcon(QIcon(":/icon.ico"));
	
	// ★ 应用主题：先从 QSettings 读取，再应用到 QApplication
	ThemeManager::instance().loadFromSettings();   // 读取上次亮/暗
	ThemeManager::instance().apply(&app);          // 应用 QSS
	
	QCommandLineParser parser;
	parser.setApplicationDescription(
									 QObject::tr("MyToolBox 独立浏览器（WebView2）"));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("url",
								 QObject::tr("要打开的网址，可多个"), "[url...]");
	
	QCommandLineOption optNewWindow(
									QStringList() << "n" << "new-window",
									QObject::tr("每个 URL 各开一个独立窗口（默认共用一个窗口多标签）"));
	parser.addOption(optNewWindow);
	
	parser.process(app);
	
	QStringList urls = parser.positionalArguments();
	if (urls.isEmpty()) urls << "https://www.hao123.com";
	
	// --new-window：每个 URL 一个窗口
	if (parser.isSet(optNewWindow)) {
		for (const QString &raw : urls) {
			QString u = normalizeUrl(raw);
			if (u.isEmpty()) continue;
			
			auto *w = new BrowserWindow;
			w->setAttribute(Qt::WA_DeleteOnClose);
			w->show();
			
			QTimer::singleShot(200, w, [w, u]() { w->navigate(u); });
		}
	} else {
		// 默认：一个窗口，把多个 URL 依次打开（第 1 个在首个标签，其余新建标签）
		auto *w = new BrowserWindow;
		w->setAttribute(Qt::WA_DeleteOnClose);
		w->show();
		
		QString first = normalizeUrl(urls.first());
		if (!first.isEmpty()) {
			QTimer::singleShot(200, w, [w, first]() { w->navigate(first); });
		}
		
		QStringList rest = urls.mid(1);
		if (!rest.isEmpty()) {
			QTimer::singleShot(600, w, [w, rest]() {
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
