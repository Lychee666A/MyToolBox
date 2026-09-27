#include "WebPageView.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QDebug>

#include <windows.h>
#include <wrl/event.h>

using namespace Microsoft::WRL;

// ============================================================
//                      构造 / 析构
// ============================================================
WebPageView::WebPageView(QWidget *parent)
: QWidget(parent)
{
	buildUi();
	
	m_sizeTimer = new QTimer(this);
	m_sizeTimer->setInterval(100);
	connect(m_sizeTimer, &QTimer::timeout, this, &WebPageView::onSizeTimerTick);
}

WebPageView::~WebPageView()
{
	if (m_controller) {
		m_controller->Close();
		m_controller = nullptr;
	}
	m_webView = nullptr;
	m_env = nullptr;
}

// ============================================================
//                      UI
// ============================================================
void WebPageView::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);
	
	m_webContainer = new QWidget(this);
	m_webContainer->setStyleSheet("background: white;");
	m_webContainer->setAttribute(Qt::WA_NativeWindow);
	m_webContainer->winId();
	root->addWidget(m_webContainer, 1);
	
	m_hintLabel = new QLabel(tr("正在初始化…"), this);
	m_hintLabel->setAlignment(Qt::AlignCenter);
	m_hintLabel->setStyleSheet("color:#888; font-size:13px;");
	root->addWidget(m_hintLabel);
}

// ============================================================
//                      初始化
// ============================================================
void WebPageView::initialize(Microsoft::WRL::ComPtr<ICoreWebView2Environment> env)
{
	if (m_initialized) return;
	m_initialized = true;
	
	if (!env) {
		m_hintLabel->setText(tr("WebView2 环境无效"));
		return;
	}
	m_env = env;
	
	HWND hwnd = reinterpret_cast<HWND>(m_webContainer->winId());
	
	m_env->CreateCoreWebView2Controller(
										hwnd,
										Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
																											[this](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
																												if (FAILED(result) || !controller) {
																													qWarning() << "[WebPageView] 控制器创建失败:"
																													<< Qt::hex << quint32(result);
																													m_hintLabel->setText(
																																		 tr("WebView2 控制器创建失败（0x%1）")
																																		 .arg(quint32(result), 8, 16, QChar('0')));
																													return S_OK;
																												}
																												
																												m_controller = controller;
																												m_controller->get_CoreWebView2(&m_webView);
																												
																												RECT bounds;
																												::GetClientRect(
																																reinterpret_cast<HWND>(m_webContainer->winId()),
																																&bounds);
																												m_controller->put_Bounds(bounds);
																												m_controller->put_IsVisible(TRUE);
																												
																												m_webViewReady = true;
																												m_hintLabel->hide();
																												
																												EventRegistrationToken token;
																												
																												// ---- 导航完成 ----
																												m_webView->add_NavigationCompleted(
																																				   Callback<ICoreWebView2NavigationCompletedEventHandler>(
																																																		  [this](ICoreWebView2*,
																																																				 ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
																																																			  BOOL ok = FALSE;
																																																			  args->get_IsSuccess(&ok);
																																																			  updateBackForward();
																																																			  emit navigationCompleted(ok != FALSE);
																																																			  return S_OK;
																																																		  }).Get(), &token);
																												
																												// ---- URL 变化 ----
																												m_webView->add_SourceChanged(
																																			 Callback<ICoreWebView2SourceChangedEventHandler>(
																																															  [this](ICoreWebView2* sender,
																																																	 ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
																																																  LPWSTR url = nullptr;
																																																  sender->get_Source(&url);
																																																  if (url) {
																																																	  m_currentUrl = QString::fromWCharArray(url);
																																																	  CoTaskMemFree(url);
																																																	  emit urlChanged(m_currentUrl);
																																																  }
																																																  updateBackForward();
																																																  return S_OK;
																																															  }).Get(), &token);
																												
																												// ---- 标题变化 ----
																												m_webView->add_DocumentTitleChanged(
																																					Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
																																																			[this](ICoreWebView2* sender, IUnknown*) -> HRESULT {
																																																				LPWSTR title = nullptr;
																																																				sender->get_DocumentTitle(&title);
																																																				if (title) {
																																																					m_currentTitle = QString::fromWCharArray(title);
																																																					CoTaskMemFree(title);
																																																					emit titleChanged(m_currentTitle);
																																																				}
																																																				return S_OK;
																																																			}).Get(), &token);
																												
																												// ---- 历史变化 ----
																												m_webView->add_HistoryChanged(
																																			  Callback<ICoreWebView2HistoryChangedEventHandler>(
																																																[this](ICoreWebView2*, IUnknown*) -> HRESULT {
																																																	updateBackForward();
																																																	return S_OK;
																																																}).Get(), &token);
																												
																												// ---- NewWindowRequested：window.open / target=_blank ----
																												m_webView->add_NewWindowRequested(
																																				  Callback<ICoreWebView2NewWindowRequestedEventHandler>(
																																																		[this](ICoreWebView2*,
																																																			   ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
																																																			LPWSTR uri = nullptr;
																																																			args->get_Uri(&uri);
																																																			QString url;
																																																			if (uri) {
																																																				url = QString::fromWCharArray(uri);
																																																				CoTaskMemFree(uri);
																																																			}
																																																			
																																																			qDebug() << "[WebPageView] NewWindowRequested:" << url;
																																																			args->put_Handled(TRUE);
																																																			
																																																			if (!url.isEmpty())
																																																				emit newWindowRequested(url);
																																																			return S_OK;
																																																		}).Get(), &token);
																												
																												return S_OK;
																											}).Get());
}

// ============================================================
//                      导航
// ============================================================
void WebPageView::navigate(const QString &url)
{
	if (!m_webViewReady || !m_webView) return;
	m_webView->Navigate(url.toStdWString().c_str());
}

void WebPageView::navigateToHome(const QString &homeUrl) { navigate(homeUrl); }

void WebPageView::goBack()
{
	if (m_webViewReady && m_webView && m_canGoBack) m_webView->GoBack();
}

void WebPageView::goForward()
{
	if (m_webViewReady && m_webView && m_canGoForward) m_webView->GoForward();
}

void WebPageView::reload()
{
	if (m_webViewReady && m_webView) m_webView->Reload();
}

void WebPageView::stop()
{
	if (m_webViewReady && m_webView) m_webView->Stop();
}

void WebPageView::openDevTools()
{
	if (m_webViewReady && m_webView) m_webView->OpenDevToolsWindow();
}

// ============================================================
//                      缩放
// ============================================================
void WebPageView::zoomIn()
{
	if (!m_controller) return;
	double z = 1.0;
	m_controller->get_ZoomFactor(&z);
	z = qMin(z + 0.1, 5.0);
	m_controller->put_ZoomFactor(z);
}

void WebPageView::zoomOut()
{
	if (!m_controller) return;
	double z = 1.0;
	m_controller->get_ZoomFactor(&z);
	z = qMax(z - 0.1, 0.25);
	m_controller->put_ZoomFactor(z);
}

void WebPageView::resetZoom()
{
	if (m_controller) m_controller->put_ZoomFactor(1.0);
}

double WebPageView::currentZoomFactor() const
{
	if (!m_controller) return 1.0;
	double z = 1.0;
	m_controller->get_ZoomFactor(&z);
	return z;
}

// ============================================================
//                      查找
//  注意：WebView2 SDK 的 Find API 可能需要较新版本。如果编译
//  提示 ICoreWebView2::Find 不存在，请把这两个函数体改成空实现
//  （只保留 Ctrl+F 显示查找栏，实际不执行查找）。
// ============================================================
void WebPageView::findText(const QString &text, bool forward)
{
	if (!m_webViewReady || !m_webView || text.isEmpty()) return;
	// 若 SDK 有 Find：m_webView->Find(...)
	// 不同版本 API 名称不同，此处留空占位，避免编译失败。
	Q_UNUSED(forward)
}

void WebPageView::stopFind()
{
	// 同上，SDK 支持时补实现
}

// ============================================================
//                      back/forward 状态
// ============================================================
void WebPageView::updateBackForward()
{
	if (!m_webViewReady || !m_webView) {
		m_canGoBack = m_canGoForward = false;
		emit backForwardChanged(false, false);
		return;
	}
	BOOL back = FALSE, forward = FALSE;
	m_webView->get_CanGoBack(&back);
	m_webView->get_CanGoForward(&forward);
	m_canGoBack = (back != FALSE);
	m_canGoForward = (forward != FALSE);
	emit backForwardChanged(m_canGoBack, m_canGoForward);
}

// ============================================================
//                      尺寸同步
// ============================================================
void WebPageView::resizeWebView()
{
	if (!m_controller || !m_webContainer) return;
	HWND hwnd = reinterpret_cast<HWND>(m_webContainer->winId());
	if (!hwnd) return;
	RECT bounds;
	::GetClientRect(hwnd, &bounds);
	m_controller->put_Bounds(bounds);
}

void WebPageView::syncSize() { resizeWebView(); }

void WebPageView::onSizeTimerTick()
{
	resizeWebView();
	
	if (!m_controller || !m_webContainer) return;
	HWND hwnd = reinterpret_cast<HWND>(m_webContainer->winId());
	if (!hwnd) return;
	RECT r;
	::GetClientRect(hwnd, &r);
	static RECT last{};
	static int stable = 0;
	if (r.left == last.left && r.top == last.top &&
		r.right == last.right && r.bottom == last.bottom) {
		if (++stable >= 3 && m_sizeTimer) m_sizeTimer->stop();
	} else {
		stable = 0;
		last = r;
	}
}

// ============================================================
//                      事件
// ============================================================
void WebPageView::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	resizeWebView();
	if (m_sizeTimer && isVisible()) m_sizeTimer->start();
}

void WebPageView::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	if (m_sizeTimer) m_sizeTimer->start();
	QTimer::singleShot(0,   this, &WebPageView::syncSize);
	QTimer::singleShot(50,  this, &WebPageView::syncSize);
	QTimer::singleShot(200, this, &WebPageView::syncSize);
}

void WebPageView::hideEvent(QHideEvent *e)
{
	QWidget::hideEvent(e);
	if (m_sizeTimer) m_sizeTimer->stop();
}
