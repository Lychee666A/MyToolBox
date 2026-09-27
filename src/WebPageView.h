#pragma once

#include <QWidget>
#include <QString>

#include <wrl/client.h>
#include <WebView2.h>

class QLabel;
class QTimer;
class QResizeEvent;
class QShowEvent;
class QHideEvent;

class WebPageView : public QWidget
{
	Q_OBJECT
public:
	explicit WebPageView(QWidget *parent = nullptr);
	~WebPageView() override;
	
	void initialize(Microsoft::WRL::ComPtr<ICoreWebView2Environment> env);
	
	void navigate(const QString &url);
	void navigateToHome(const QString &homeUrl);
	void goBack();
	void goForward();
	void reload();
	void stop();
	void openDevTools();
	
	// ★ 缩放
	void zoomIn();
	void zoomOut();
	void resetZoom();
	double currentZoomFactor() const;
	
	// ★ 页面内查找（SDK 版本可能不支持，若报错请删掉并相应取消 Ctrl+F 连接）
	void findText(const QString &text, bool forward = true);
	void stopFind();
	
	bool isReady() const         { return m_webViewReady; }
	bool canGoBack() const       { return m_canGoBack; }
	bool canGoForward() const    { return m_canGoForward; }
	QString currentUrl() const   { return m_currentUrl; }
	QString currentTitle() const { return m_currentTitle; }
	
	void syncSize();
	
	signals:
	void urlChanged(const QString &url);
	void titleChanged(const QString &title);
	void navigationCompleted(bool ok);
	void backForwardChanged(bool canBack, bool canForward);
	
	// ★ 请求在新标签页打开（window.open / target=_blank）
	void newWindowRequested(const QString &url);
	
private slots:
	void onSizeTimerTick();
	
protected:
	void resizeEvent(QResizeEvent *e) override;
	void showEvent(QShowEvent *e) override;
	void hideEvent(QHideEvent *e) override;
	
private:
	void buildUi();
	void resizeWebView();
	void updateBackForward();
	
	Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_env;
	Microsoft::WRL::ComPtr<ICoreWebView2Controller>  m_controller;
	Microsoft::WRL::ComPtr<ICoreWebView2>            m_webView;
	
	bool m_webViewReady = false;
	bool m_initialized = false;
	bool m_canGoBack = false;
	bool m_canGoForward = false;
	
	QString m_currentUrl;
	QString m_currentTitle;
	
	QWidget *m_webContainer = nullptr;
	QLabel  *m_hintLabel = nullptr;
	QTimer  *m_sizeTimer = nullptr;
};
