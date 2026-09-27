#pragma once

#include <QWidget>
#include <QString>
#include <QPoint>
#include <functional>

#include <wrl/client.h>
#include <WebView2.h>

class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QMenu;
class QShortcut;
class WebPageView;
class BrowserHistory;
class DownloadsManager;

class WebBrowserWidget : public QWidget
{
	Q_OBJECT
public:
	explicit WebBrowserWidget(QWidget *parent = nullptr);
	~WebBrowserWidget() override;
	
	void openUrlInNewTab(const QString &url);
	void navigateTo(const QString &url);
	
	void setKioskMode(bool enabled);
	bool isKioskMode() const { return m_kioskMode; }
	
	void setInitialUrl(const QString &url);
	
	void closeCurrentTab();
	void restoreLastClosedTab();
	void openHistoryDialog();
	void openDownloadsDialog();
	void zoomInCurrent();
	void zoomOutCurrent();
	void resetZoomCurrent();
	void focusAddressBar();
	void showFindBar();
	
public slots:
	void syncWebViewSize();
	
private slots:
	void onGoClicked();
	void onBackClicked();
	void onForwardClicked();
	void onRefreshClicked();
	void onStopClicked();
	void onHomeClicked();
	void onDevToolsClicked();
	void updateButtons();
	void applySettings();
	
	void onDownloadClicked();
	void onRetryClicked();
	
	void onNewTab();
	void onCloseTab(int index);
	void onTabChanged(int index);
	void onTabCloseRequested(int index);
	
private:
	void buildUi();
	void buildBrowserPage(QWidget *page);
	void buildDownloadPage(QWidget *page);
	void initWebView();
	void showBrowserPage();
	void showDownloadPage();
	
	void setupShortcuts();
	void showMenuPopup();
	
	void refreshPlusTab();
	
	WebPageView *currentPage() const;
	void updateAddressFromCurrent();
	void updateTabTitle(int index);
	
	void openInNewTab(const QString &url);
	void navigateToHomeWhenReady(WebPageView *page, int attempt = 0);
	void wirePage(WebPageView *page);
	
	// ---- 顶层 ----
	QStackedWidget *m_stack = nullptr;
	
	// ---- 浏览器页 ----
	QWidget     *m_browserPage = nullptr;
	QTabWidget  *m_tabWidget = nullptr;
	QLineEdit   *m_addressEdit = nullptr;
	QPushButton *m_btnBack = nullptr;
	QPushButton *m_btnForward = nullptr;
	QPushButton *m_btnRefresh = nullptr;
	QPushButton *m_btnStop = nullptr;
	QPushButton *m_btnHome = nullptr;
	QPushButton *m_btnGo = nullptr;
	QPushButton *m_btnDevTools = nullptr;
	QPushButton *m_btnNewTab = nullptr;
	QPushButton *m_btnMenu = nullptr;
	QLabel      *m_statusLabel = nullptr;
	
	QWidget     *m_findBar   = nullptr;
	QLineEdit   *m_findEdit  = nullptr;
	QPushButton *m_findNext  = nullptr;
	QPushButton *m_findPrev  = nullptr;
	QPushButton *m_findClose = nullptr;
	
	QWidget     *m_downloadPage = nullptr;
	QLabel      *m_downloadTitle = nullptr;
	QLabel      *m_downloadDesc = nullptr;
	QPushButton *m_btnDownload = nullptr;
	QPushButton *m_btnRetry = nullptr;
	
	BrowserHistory   *m_history   = nullptr;
	DownloadsManager *m_downloads = nullptr;
	
	Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_env;
	bool m_envReady = false;
	bool m_webViewInitialized = false;
	
	bool m_kioskMode = false;
	QString m_pendingUrl;
	
	// ★ 用指针记录 + tab 的 widget（不用索引，索引会变）
	QWidget *m_plusTabWidget = nullptr;
	
	QString m_homeUrl;
	
	QTimer *m_sizeTimer = nullptr;
};
