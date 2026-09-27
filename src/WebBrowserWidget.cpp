#include "WebBrowserWidget.h"
#include "WebPageView.h"
#include "SettingsManager.h"
#include "WebView2Runtime.h"
#include "BrowserHistory.h"
#include "DownloadsManager.h"
#include "HistoryDialog.h"
#include "DownloadsDialog.h"

#include <QShowEvent>
#include <QHideEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QResizeEvent>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QUrl>
#include <QMessageBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QDesktopServices>
#include <QFrame>
#include <QFileInfo>
#include <QTabBar>
#include <QShortcut>
#include <QMenu>
#include <QStandardPaths>

#include <functional>
#include <memory>

#include <windows.h>
#include <wrl/event.h>

using namespace Microsoft::WRL;

static const char *kWebView2DownloadUrl =
"https://go.microsoft.com/fwlink/p/?LinkId=2124703";

static const char *kDefaultHomeUrl = "https://www.hao123.com";

// ============================================================
//                      构造 / 析构
// ============================================================
WebBrowserWidget::WebBrowserWidget(QWidget *parent)
: QWidget(parent)
{
	m_history   = &BrowserHistory::instance();
	m_downloads = &DownloadsManager::instance();
	
	m_homeUrl = SettingsManager::instance().browserHomeUrl();
	if (m_homeUrl.isEmpty()) m_homeUrl = kDefaultHomeUrl;
	
	buildUi();
	setupShortcuts();
	
	m_sizeTimer = new QTimer(this);
	m_sizeTimer->setInterval(100);
	connect(m_sizeTimer, &QTimer::timeout, this, &WebBrowserWidget::syncWebViewSize);
	
	connect(&SettingsManager::instance(), &SettingsManager::settingsChanged,
			this, &WebBrowserWidget::applySettings);
	
	QTimer::singleShot(0, this, [this]() {
		auto &rt = WebView2Runtime::instance();
		if (!rt.isInstalled()) {
			qDebug() << "WebView2 Runtime 未安装，显示下载引导";
			showDownloadPage();
		} else {
			qDebug() << "WebView2 Runtime 已安装:" << rt.installedVersion();
			showBrowserPage();
			initWebView();
		}
	});
}

WebBrowserWidget::~WebBrowserWidget()
{
	m_env = nullptr;
}

// ============================================================
//                      UI
// ============================================================
void WebBrowserWidget::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	
	m_stack = new QStackedWidget(this);
	root->addWidget(m_stack);
	
	m_browserPage = new QWidget(this);
	buildBrowserPage(m_browserPage);
	m_stack->addWidget(m_browserPage);
	
	m_downloadPage = new QWidget(this);
	buildDownloadPage(m_downloadPage);
	m_stack->addWidget(m_downloadPage);
}

void WebBrowserWidget::buildBrowserPage(QWidget *page)
{
	auto *root = new QVBoxLayout(page);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(4);
	
	// ---- 导航栏 ----
	auto *nav = new QHBoxLayout();
	nav->setSpacing(2);
	
	m_btnBack     = new QPushButton(tr("←"),   page);
	m_btnForward  = new QPushButton(tr("→"),   page);
	m_btnRefresh  = new QPushButton(tr("刷新"), page);
	m_btnStop     = new QPushButton(tr("停止"), page);
	m_btnHome     = new QPushButton(tr("主页"), page);
	m_btnGo       = new QPushButton(tr("转到"), page);
	m_btnDevTools = new QPushButton(tr("F12"), page);
	m_btnNewTab   = new QPushButton(tr("+"),   page);
	m_btnMenu     = new QPushButton(tr("☰"),   page);   // ★ 菜单
	
	m_btnBack->setFixedWidth(32);
	m_btnForward->setFixedWidth(32);
	m_btnStop->setFixedWidth(50);
	m_btnHome->setFixedWidth(50);
	m_btnDevTools->setFixedWidth(40);
	m_btnNewTab->setFixedWidth(32);
	m_btnMenu->setFixedWidth(32);
	m_btnNewTab->setToolTip(tr("新建标签页 (Ctrl+T)"));
	m_btnMenu->setToolTip(tr("菜单"));
	
	m_addressEdit = new QLineEdit(page);
	m_addressEdit->setPlaceholderText(tr("输入网址或搜索内容…"));
	
	nav->addWidget(m_btnBack);
	nav->addWidget(m_btnForward);
	nav->addWidget(m_btnRefresh);
	nav->addWidget(m_btnStop);
	nav->addWidget(m_btnHome);
	nav->addWidget(m_addressEdit, 1);
	nav->addWidget(m_btnGo);
	nav->addWidget(m_btnDevTools);
	nav->addWidget(m_btnNewTab);
	nav->addWidget(m_btnMenu);      // ★
	
	root->addLayout(nav);
	
	// ---- QTabWidget ----
	m_tabWidget = new QTabWidget(page);
	m_tabWidget->setTabsClosable(true);
	m_tabWidget->setMovable(true);
	m_tabWidget->setDocumentMode(true);
	m_tabWidget->setElideMode(Qt::ElideRight);
	
	m_tabWidget->tabBar()->setTabsClosable(true);
	m_tabWidget->tabBar()->setExpanding(false);
	m_tabWidget->tabBar()->setUsesScrollButtons(true);
	
	m_tabWidget->setStyleSheet(
							   "QTabBar::close-button {"
							   "  image: url(:/qt-project.org/styles/commonstyle/images/standardbutton-closetab-16.png);"
							   "  subcontrol-position: right;"
							   "}"
							   "QTabBar::close-button:hover {"
							   "  image: url(:/qt-project.org/styles/commonstyle/images/standardbutton-closetab-hover-16.png);"
							   "}");
	
	root->addWidget(m_tabWidget, 1);
	
	// ---- 状态栏 ----
	m_statusLabel = new QLabel(tr("就绪"), page);
	m_statusLabel->setStyleSheet("padding: 2px 6px;");
	root->addWidget(m_statusLabel);
	
	// ---- 信号 ----
	connect(m_btnBack,    &QPushButton::clicked, this, &WebBrowserWidget::onBackClicked);
	connect(m_btnForward, &QPushButton::clicked, this, &WebBrowserWidget::onForwardClicked);
	connect(m_btnRefresh, &QPushButton::clicked, this, &WebBrowserWidget::onRefreshClicked);
	connect(m_btnStop,    &QPushButton::clicked, this, &WebBrowserWidget::onStopClicked);
	connect(m_btnHome,    &QPushButton::clicked, this, &WebBrowserWidget::onHomeClicked);
	connect(m_btnGo,      &QPushButton::clicked, this, &WebBrowserWidget::onGoClicked);
	connect(m_btnDevTools,&QPushButton::clicked, this, &WebBrowserWidget::onDevToolsClicked);
	connect(m_btnNewTab,  &QPushButton::clicked, this, &WebBrowserWidget::onNewTab);
	connect(m_btnMenu,    &QPushButton::clicked, this, &WebBrowserWidget::showMenuPopup);
	connect(m_addressEdit, &QLineEdit::returnPressed, this, &WebBrowserWidget::onGoClicked);
	
	connect(m_tabWidget, &QTabWidget::currentChanged,
			this, &WebBrowserWidget::onTabChanged);
	connect(m_tabWidget, &QTabWidget::tabCloseRequested,
			this, &WebBrowserWidget::onTabCloseRequested);
	
	updateButtons();
}

// ============================================================
//                      快捷键
//  ★ 注意：Qt 6.11 不允许 QShortcut(key, parent, receiver, slot) 这种 4 参
//    数构造，所以这里统一用 2 参数构造 + 手动 connect。
// ============================================================
void WebBrowserWidget::setupShortcuts()
{
	// 辅助：创建一个快捷键并绑定 lambda
	auto addShortcut = [this](const QKeySequence &key,
							  std::function<void()> handler) -> QShortcut * {
		auto *sc = new QShortcut(key, this);
		sc->setContext(Qt::WindowShortcut);
		connect(sc, &QShortcut::activated, this, [handler]() { handler(); });
		return sc;
	};
	
	// ---- 标签页 ----
	addShortcut(QKeySequence("Ctrl+T"), [this]() { onNewTab(); });
	addShortcut(QKeySequence("Ctrl+W"), [this]() { closeCurrentTab(); });
	addShortcut(QKeySequence("Ctrl+F4"), [this]() { closeCurrentTab(); });
	addShortcut(QKeySequence("Ctrl+Tab"), [this]() {
		int n = m_tabWidget->count();
		if (n < 2) return;
		m_tabWidget->setCurrentIndex((m_tabWidget->currentIndex() + 1) % n);
	});
	addShortcut(QKeySequence("Ctrl+Shift+Tab"), [this]() {
		int n = m_tabWidget->count();
		if (n < 2) return;
		m_tabWidget->setCurrentIndex((m_tabWidget->currentIndex() - 1 + n) % n);
	});
	for (int i = 1; i <= 9; ++i) {
		addShortcut(QKeySequence(QString("Ctrl+%1").arg(i)), [this, i]() {
			int idx = (i == 9) ? m_tabWidget->count() - 1 : i - 1;
			if (idx >= 0 && idx < m_tabWidget->count())
				m_tabWidget->setCurrentIndex(idx);
		});
	}
	addShortcut(QKeySequence("Ctrl+Shift+T"), [this]() { restoreLastClosedTab(); });
	
	// ---- 页面 ----
	addShortcut(QKeySequence("F5"), [this]() { onRefreshClicked(); });
	addShortcut(QKeySequence("Ctrl+R"), [this]() { onRefreshClicked(); });
	addShortcut(QKeySequence("Ctrl+Shift+R"), [this]() {
		if (auto *p = currentPage()) p->reload();
	});
	addShortcut(QKeySequence("Esc"), [this]() {
		if (m_findBar && m_findBar->isVisible()) {
			if (auto *p = currentPage()) p->stopFind();
			m_findBar->hide();
		} else if (auto *p = currentPage()) {
			p->stop();
		}
	});
	
	// ---- 缩放 ----
	addShortcut(QKeySequence("Ctrl++"), [this]() { zoomInCurrent(); });
	addShortcut(QKeySequence("Ctrl+="), [this]() { zoomInCurrent(); });
	addShortcut(QKeySequence("Ctrl+-"), [this]() { zoomOutCurrent(); });
	addShortcut(QKeySequence("Ctrl+0"), [this]() { resetZoomCurrent(); });
	
	// ---- 地址栏 ----
	addShortcut(QKeySequence("Ctrl+L"), [this]() { focusAddressBar(); });
	addShortcut(QKeySequence("Alt+D"),  [this]() { focusAddressBar(); });
	addShortcut(QKeySequence("Ctrl+Enter"), [this]() {
		QString t = m_addressEdit->text().trimmed();
		if (!t.isEmpty() && !t.contains('.') && !t.contains('/') && !t.contains(':'))
			t = "www." + t + ".com";
		navigateTo(t);
	});
	
	// ---- 查找 ----
	addShortcut(QKeySequence("Ctrl+F"), [this]() { showFindBar(); });
	addShortcut(QKeySequence("Ctrl+G"), [this]() {
		if (m_findBar && m_findBar->isVisible()) {
			if (auto *p = currentPage()) p->findText(m_findEdit->text(), true);
		}
	});
	
	// ---- 开发者工具 ----
	addShortcut(QKeySequence("F12"),           [this]() { onDevToolsClicked(); });
	addShortcut(QKeySequence("Ctrl+Shift+I"),  [this]() { onDevToolsClicked(); });
	
	// ---- 历史 / 下载 ----
	addShortcut(QKeySequence("Ctrl+H"), [this]() { openHistoryDialog(); });
	addShortcut(QKeySequence("Ctrl+J"), [this]() { openDownloadsDialog(); });
	
	// ---- 其他 ----
	addShortcut(QKeySequence("Alt+Home"), [this]() { onHomeClicked(); });
	
	// ---- InPrivate（简化：开新标签） ----
	addShortcut(QKeySequence("Ctrl+Shift+N"), [this]() {
		openInNewTab(m_homeUrl);
	});
	
	// ---- 清数据（简化：打开历史） ----
	addShortcut(QKeySequence("Ctrl+Shift+Delete"), [this]() { openHistoryDialog(); });
}

// ============================================================
//                      菜单
// ============================================================
void WebBrowserWidget::showMenuPopup()
{
	QMenu menu(this);
	menu.addAction(tr("新建标签页") + "\tCtrl+T", this, &WebBrowserWidget::onNewTab);
	menu.addAction(tr("关闭当前标签") + "\tCtrl+W", this, &WebBrowserWidget::closeCurrentTab);
	menu.addAction(tr("恢复关闭的标签") + "\tCtrl+Shift+T", this, &WebBrowserWidget::restoreLastClosedTab);
	menu.addSeparator();
	menu.addAction(tr("历史记录") + "\tCtrl+H", this, &WebBrowserWidget::openHistoryDialog);
	menu.addAction(tr("下载记录") + "\tCtrl+J", this, &WebBrowserWidget::openDownloadsDialog);
	menu.addSeparator();
	menu.addAction(tr("查找") + "\tCtrl+F", this, &WebBrowserWidget::showFindBar);
	menu.addAction(tr("开发者工具") + "\tF12", this, &WebBrowserWidget::onDevToolsClicked);
	menu.addSeparator();
	menu.addAction(tr("主页"), this, &WebBrowserWidget::onHomeClicked);
	
	menu.exec(m_btnMenu->mapToGlobal(QPoint(0, m_btnMenu->height())));
}

// ============================================================
//                     show / hide / resize
// ============================================================
void WebBrowserWidget::syncWebViewSize()
{
	for (int i = 0; i < m_tabWidget->count(); ++i) {
		if (auto *page = qobject_cast<WebPageView *>(m_tabWidget->widget(i))) {
			if (page->isVisible()) page->syncSize();
		}
	}
}

// ============================================================
//                     初始化 WebView2 环境
// ============================================================
void WebBrowserWidget::initWebView()
{
	if (!WebView2Runtime::instance().isInstalled()) {
		showDownloadPage();
		return;
	}
	if (m_envReady) return;
	
	// WebView2 用户数据目录放到用户可写的位置（避免 Program Files 只读）
	const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
	QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	if (baseDir.isEmpty()) baseDir = QDir::homePath() + "/.MyToolBox";
	const QString userDataDir = baseDir + "/WebView2Data_" + exeName;
	QDir().mkpath(userDataDir);
	
	qDebug() << "[Browser] WebView2 user data dir:" << userDataDir;
	
	const std::wstring wUserData =
	QDir::toNativeSeparators(userDataDir).toStdWString();
	
	HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
														  nullptr, wUserData.c_str(), nullptr,
														  Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
																															   [this](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
																																   if (FAILED(result) || !env) {
																																	   m_statusLabel->setText(
																																							  tr("WebView2 环境创建失败（0x%1）")
																																							  .arg(quint32(result), 8, 16, QChar('0')));
																																	   return S_OK;
																																   }
																																   m_env = env;
																																   m_envReady = true;
																																   m_statusLabel->setText(tr("WebView2 就绪"));
																																   onNewTab();
																																   return S_OK;
																															   }).Get());
	
	if (FAILED(hr)) {
		m_statusLabel->setText(
							   tr("创建 WebView2 环境失败（0x%1）")
							   .arg(quint32(hr), 8, 16, QChar('0')));
	}
}

// ============================================================
//                      标签页管理
// ============================================================
WebPageView *WebBrowserWidget::currentPage() const
{
	if (!m_tabWidget) return nullptr;
	return qobject_cast<WebPageView *>(m_tabWidget->currentWidget());
}

void WebBrowserWidget::navigateToHomeWhenReady(WebPageView *page, int attempt)
{
	if (!page) return;
	if (page->isReady()) {
		page->navigateToHome(m_homeUrl);
		return;
	}
	if (attempt >= 30) {
		qWarning() << "[Browser] WebView2 未就绪，放弃主页导航";
		return;
	}
	QTimer::singleShot(100, page, [this, page, attempt]() {
		navigateToHomeWhenReady(page, attempt + 1);
	});
}

void WebBrowserWidget::wirePage(WebPageView *page)
{
	if (!page) return;
	
	connect(page, &WebPageView::urlChanged,
			this, [this, page](const QString &url) {
				if (currentPage() == page)
					m_addressEdit->setText(url);
				updateTabTitle(m_tabWidget->indexOf(page));
			});
	
	connect(page, &WebPageView::titleChanged,
			this, [this, page](const QString &title) {
				const int i = m_tabWidget->indexOf(page);
				if (i < 0) return;
				m_tabWidget->setTabText(i, title.isEmpty() ? tr("新标签页") : title);
				m_tabWidget->setTabToolTip(i, title);
			});
	
	connect(page, &WebPageView::navigationCompleted,
			this, [this, page](bool ok) {
				if (currentPage() == page) {
					m_statusLabel->setText(ok ? tr("加载完成") : tr("加载失败"));
					updateButtons();
				}
				if (ok) {
					// ★ 记录历史
					m_history->recordVisit(page->currentUrl(), page->currentTitle());
				}
			});
	
	connect(page, &WebPageView::backForwardChanged,
			this, [this, page](bool, bool) {
				if (currentPage() == page) updateButtons();
			});
	
	connect(page, &WebPageView::newWindowRequested,
			this, [this](const QString &url) {
				openInNewTab(url);
			});
	
	QTimer::singleShot(0,   page, &WebPageView::syncSize);
	QTimer::singleShot(80,  page, &WebPageView::syncSize);
	QTimer::singleShot(250, page, &WebPageView::syncSize);
}

void WebBrowserWidget::onNewTab()
{
	if (!m_envReady) return;
	
	auto *page = new WebPageView(m_tabWidget);
	page->initialize(m_env);
	
	const int idx = m_tabWidget->addTab(page, tr("新标签页"));
	m_tabWidget->setCurrentIndex(idx);
	
	wirePage(page);
	navigateToHomeWhenReady(page, 0);
	
	updateButtons();
}

void WebBrowserWidget::openInNewTab(const QString &url)
{
	if (!m_envReady) return;
	if (url.isEmpty()) return;
	
	qDebug() << "[Browser] openInNewTab:" << url;
	
	auto *page = new WebPageView(m_tabWidget);
	page->initialize(m_env);
	
	const int idx = m_tabWidget->addTab(page, tr("新标签页"));
	m_tabWidget->setCurrentIndex(idx);
	
	wirePage(page);
	
	{
		auto retryFn = std::make_shared<std::function<void(int)>>();
		*retryFn = [page, url, retryFn](int attempt) {
			if (!page) return;
			if (page->isReady()) {
				page->navigate(url);
				return;
			}
			if (attempt >= 30) {
				qWarning() << "[Browser] WebView2 未就绪，放弃新标签导航";
				return;
			}
			QTimer::singleShot(100, page, [retryFn, attempt]() {
				(*retryFn)(attempt + 1);
			});
		};
		(*retryFn)(0);
	}
	
	updateButtons();
}

void WebBrowserWidget::closeCurrentTab()
{
	int idx = m_tabWidget->currentIndex();
	if (idx >= 0) onCloseTab(idx);
}

void WebBrowserWidget::restoreLastClosedTab()
{
	if (!m_history->hasClosedTab()) return;
	auto p = m_history->popClosedTab();
	if (p.first.isEmpty()) return;
	openInNewTab(p.first);
}

void WebBrowserWidget::onCloseTab(int index)
{
	if (index < 0 || index >= m_tabWidget->count()) return;
	
	// ★ 记录到"最近关闭"
	if (auto *page = qobject_cast<WebPageView *>(m_tabWidget->widget(index))) {
		m_history->pushClosedTab(page->currentUrl(), page->currentTitle());
	}
	
	QWidget *w = m_tabWidget->widget(index);
	m_tabWidget->removeTab(index);
	w->deleteLater();
	
	if (m_tabWidget->count() == 0)
		onNewTab();
}

void WebBrowserWidget::onTabCloseRequested(int index) { onCloseTab(index); }

void WebBrowserWidget::onTabChanged(int /*index*/)
{
	updateAddressFromCurrent();
	updateButtons();
	
	if (auto *page = currentPage()) {
		m_statusLabel->setText(tr("就绪"));
		QTimer::singleShot(0,   page, &WebPageView::syncSize);
		QTimer::singleShot(80,  page, &WebPageView::syncSize);
		QTimer::singleShot(250, page, &WebPageView::syncSize);
	}
}

void WebBrowserWidget::updateAddressFromCurrent()
{
	if (auto *page = currentPage())
		m_addressEdit->setText(page->currentUrl());
	else
		m_addressEdit->clear();
}

void WebBrowserWidget::updateTabTitle(int index)
{
	if (index < 0 || index >= m_tabWidget->count()) return;
	auto *page = qobject_cast<WebPageView *>(m_tabWidget->widget(index));
	if (!page) return;
	const QString title = page->currentTitle();
	m_tabWidget->setTabText(index, title.isEmpty() ? tr("新标签页") : title);
}

// ============================================================
//                      历史 / 下载
// ============================================================
void WebBrowserWidget::openHistoryDialog()
{
	HistoryDialog dlg(this);
	connect(&dlg, &HistoryDialog::openUrl, this, [this](const QString &url) {
		openUrlInNewTab(url);
	});
	dlg.exec();
}

void WebBrowserWidget::openDownloadsDialog()
{
	DownloadsDialog dlg(this);
	dlg.exec();
}

// ============================================================
//                      缩放
// ============================================================
void WebBrowserWidget::zoomInCurrent()   { if (auto *p = currentPage()) p->zoomIn(); }
void WebBrowserWidget::zoomOutCurrent()  { if (auto *p = currentPage()) p->zoomOut(); }
void WebBrowserWidget::resetZoomCurrent(){ if (auto *p = currentPage()) p->resetZoom(); }

// ============================================================
//                      地址栏
// ============================================================
void WebBrowserWidget::focusAddressBar()
{
	m_addressEdit->setFocus();
	m_addressEdit->selectAll();
}

// ============================================================
//                      查找栏
// ============================================================
void WebBrowserWidget::showFindBar()
{
	if (!m_findBar) {
		m_findBar = new QWidget(m_browserPage);
		auto *lay = new QHBoxLayout(m_findBar);
		lay->setContentsMargins(4, 2, 4, 2);
		
		m_findEdit  = new QLineEdit(m_findBar);
		m_findEdit->setPlaceholderText(tr("查找内容…"));
		m_findPrev  = new QPushButton(tr("上一个"), m_findBar);
		m_findNext  = new QPushButton(tr("下一个"), m_findBar);
		m_findClose = new QPushButton(tr("×"), m_findBar);
		m_findClose->setFixedWidth(28);
		
		lay->addWidget(new QLabel(tr("查找:"), m_findBar));
		lay->addWidget(m_findEdit, 1);
		lay->addWidget(m_findPrev);
		lay->addWidget(m_findNext);
		lay->addWidget(m_findClose);
		
		// 插入到导航栏下面
		if (auto *rootLay = qobject_cast<QVBoxLayout *>(m_browserPage->layout()))
			rootLay->insertWidget(1, m_findBar);
		
		connect(m_findEdit, &QLineEdit::returnPressed, this, [this]() {
			if (auto *p = currentPage()) p->findText(m_findEdit->text(), true);
		});
		connect(m_findNext, &QPushButton::clicked, this, [this]() {
			if (auto *p = currentPage()) p->findText(m_findEdit->text(), true);
		});
		connect(m_findPrev, &QPushButton::clicked, this, [this]() {
			if (auto *p = currentPage()) p->findText(m_findEdit->text(), false);
		});
		connect(m_findClose, &QPushButton::clicked, this, [this]() {
			if (auto *p = currentPage()) p->stopFind();
			m_findBar->hide();
		});
	}
	m_findBar->show();
	m_findEdit->setFocus();
	m_findEdit->selectAll();
}

// ============================================================
//                      下载引导页
// ============================================================
void WebBrowserWidget::buildDownloadPage(QWidget *page)
{
	auto *root = new QVBoxLayout(page);
	root->setContentsMargins(40, 40, 40, 40);
	root->addStretch(1);
	
	m_downloadTitle = new QLabel(tr("浏览器功能需要 WebView2 Runtime"), page);
	m_downloadTitle->setAlignment(Qt::AlignCenter);
	QFont titleFont = m_downloadTitle->font();
	titleFont.setPointSize(18);
	titleFont.setBold(true);
	m_downloadTitle->setFont(titleFont);
	root->addWidget(m_downloadTitle);
	
	root->addSpacing(20);
	
	m_downloadDesc = new QLabel(
								tr("WebView2 Runtime 是微软提供的浏览器内核组件，用于在本程序中显示网页。\n"
								   "本程序未携带此组件，需要您单独安装（约 150 MB，仅需安装一次）。\n\n"
								   "点击下方按钮将打开微软官方下载页面："), page);
	m_downloadDesc->setAlignment(Qt::AlignCenter);
	m_downloadDesc->setWordWrap(true);
	root->addWidget(m_downloadDesc);
	
	root->addSpacing(30);
	
	auto *btnRow = new QHBoxLayout();
	btnRow->addStretch(1);
	
	m_btnDownload = new QPushButton(tr("下载并安装"), page);
	m_btnDownload->setMinimumSize(140, 40);
	m_btnDownload->setCursor(Qt::PointingHandCursor);
	m_btnDownload->setProperty("accent", true);
	
	m_btnRetry = new QPushButton(tr("我已安装，重试"), page);
	m_btnRetry->setMinimumSize(140, 40);
	m_btnRetry->setCursor(Qt::PointingHandCursor);
	
	btnRow->addWidget(m_btnDownload);
	btnRow->addSpacing(20);
	btnRow->addWidget(m_btnRetry);
	btnRow->addStretch(1);
	
	root->addLayout(btnRow);
	root->addSpacing(30);
	
	auto *tip = new QLabel(
						   tr("<i>提示：安装完成后请重启程序，或者点击「我已安装，重试」立即启用。</i>"),
						   page);
	tip->setAlignment(Qt::AlignCenter);
	tip->setProperty("muted", true);
	root->addWidget(tip);
	
	root->addStretch(2);
	
	connect(m_btnDownload, &QPushButton::clicked, this, &WebBrowserWidget::onDownloadClicked);
	connect(m_btnRetry,    &QPushButton::clicked, this, &WebBrowserWidget::onRetryClicked);
}

void WebBrowserWidget::showBrowserPage()  { if (m_stack) m_stack->setCurrentIndex(0); }
void WebBrowserWidget::showDownloadPage() { if (m_stack) m_stack->setCurrentIndex(1); }

// ============================================================
//                      下载 / 重试
// ============================================================
void WebBrowserWidget::onDownloadClicked()
{
	const QString localInstaller = QDir(QCoreApplication::applicationDirPath())
	.filePath("MicrosoftEdgeWebview2Setup.exe");
	
	if (QFileInfo::exists(localInstaller)) {
		QMessageBox::StandardButton ret = QMessageBox::question(
																this, tr("安装 WebView2 Runtime"),
																tr("检测到本地安装器：\n%1\n\n是否立即自动静默安装？")
																.arg(localInstaller),
																QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
		
		if (ret == QMessageBox::Yes) {
			if (m_btnDownload) m_btnDownload->setEnabled(false);
			if (m_btnRetry)    m_btnRetry->setEnabled(false);
			m_downloadDesc->setText(tr("正在安装 WebView2 Runtime，请稍候…"));
			
			auto &rt = WebView2Runtime::instance();
			disconnect(&rt, nullptr, this, nullptr);
			
			connect(&rt, &WebView2Runtime::installProgress,
					this, [this](const QString &t) { m_downloadDesc->setText(t); });
			
			connect(&rt, &WebView2Runtime::installFinished,
					this, [this](bool ok, const QString &msg) {
						if (m_btnDownload) m_btnDownload->setEnabled(true);
						if (m_btnRetry)    m_btnRetry->setEnabled(true);
						if (ok) {
							QMessageBox::information(this, tr("安装成功"), msg);
							showBrowserPage();
							if (!m_webViewInitialized) {
								m_webViewInitialized = true;
								initWebView();
							}
						} else {
							QMessageBox::warning(this, tr("安装失败"), msg);
						}
					});
			
			rt.installAsync(localInstaller);
			return;
		}
	}
	
	QDesktopServices::openUrl(QUrl(kWebView2DownloadUrl));
	QMessageBox::information(this, tr("手动安装提示"),
							 tr("已打开微软官方下载页面。安装后回到本程序点击「我已安装，重试」。"));
}

void WebBrowserWidget::onRetryClicked()
{
	if (!WebView2Runtime::instance().isInstalled()) {
		QMessageBox::information(this, tr("尚未检测到"), tr("未检测到 WebView2 Runtime。"));
		return;
	}
	showBrowserPage();
	if (!m_webViewInitialized) {
		m_webViewInitialized = true;
		initWebView();
	}
}

// ============================================================
//                      导航
// ============================================================
void WebBrowserWidget::navigateTo(const QString &url)
{
	auto *page = currentPage();
	if (!page) return;
	
	QString target = url.trimmed();
	if (target.isEmpty()) return;
	
	if (!target.startsWith("http://") && !target.startsWith("https://")
		&& !target.startsWith("file://") && !target.startsWith("about:")) {
		if (target.contains('.') && !target.contains(' ')) {
			target = "https://" + target;
		} else {
			const SearchEngine eng = SettingsManager::instance().defaultSearchEngine();
			if (!eng.url.isEmpty() && eng.url.contains("%1")) {
				target = eng.url;
				target.replace("%1",
							   QString::fromUtf8(QUrl::toPercentEncoding(url.trimmed())));
			} else {
				target = "https://www.baidu.com/s?ie=UTF-8&wd=" +
				QString::fromUtf8(QUrl::toPercentEncoding(url.trimmed()));
			}
		}
	}
	
	page->navigate(target);
	m_statusLabel->setText(tr("加载中…"));
}

void WebBrowserWidget::openUrlInNewTab(const QString &url)
{
	if (url.isEmpty()) return;
	if (!m_envReady) {
		auto retryFn = std::make_shared<std::function<void(int)>>();
		*retryFn = [this, url, retryFn](int attempt) {
			if (m_envReady) { openUrlInNewTab(url); return; }
			if (attempt >= 30) {
				qWarning() << "[Browser] WebView2 环境未就绪，放弃打开:" << url;
				return;
			}
			QTimer::singleShot(100, this, [retryFn, attempt]() {
				(*retryFn)(attempt + 1);
			});
		};
		(*retryFn)(0);
		return;
	}
	openInNewTab(url);
}

// ============================================================
//                      动作
// ============================================================
void WebBrowserWidget::onGoClicked()      { navigateTo(m_addressEdit->text()); }
void WebBrowserWidget::onBackClicked()    { if (auto *p = currentPage()) p->goBack(); }
void WebBrowserWidget::onForwardClicked() { if (auto *p = currentPage()) p->goForward(); }
void WebBrowserWidget::onRefreshClicked() { if (auto *p = currentPage()) p->reload(); }
void WebBrowserWidget::onStopClicked()    { if (auto *p = currentPage()) p->stop(); }

void WebBrowserWidget::onHomeClicked()
{
	m_homeUrl = SettingsManager::instance().browserHomeUrl();
	if (m_homeUrl.isEmpty()) m_homeUrl = kDefaultHomeUrl;
	if (auto *p = currentPage()) p->navigateToHome(m_homeUrl);
}

void WebBrowserWidget::onDevToolsClicked()
{
	if (auto *p = currentPage()) p->openDevTools();
}

void WebBrowserWidget::updateButtons()
{
	auto *page = currentPage();
	const bool ready = (page && page->isReady());
	m_btnBack->setEnabled(ready);
	m_btnForward->setEnabled(ready);
}

void WebBrowserWidget::applySettings()
{
	m_homeUrl = SettingsManager::instance().browserHomeUrl();
	if (m_homeUrl.isEmpty()) m_homeUrl = kDefaultHomeUrl;
}
