#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QUrl>

class QTabWidget;
class QLabel;
class QMenu;
class QAction;

class CameraRecorder;
class ScreenRecorderWidget;
class VlcPlayerWidget;
class ArchivePreviewWidget;
class ImageViewerWidget;
class QuickLinksWidget;
class FileSearchWidget;
class WebBrowserWidget;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;
	
protected:
	void showEvent(QShowEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void closeEvent(QCloseEvent *e) override;
	
private slots:
	void openSettings();
	void onSettingsChanged();
	void openAbout();
	void toggleDarkTheme(bool dark);
	void onTabChanged(int index);
	void updateStatusBar();
	
	void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void updateTrayActions();
	
	void trayOpenCamera();
	void trayOpenScreen();
	void trayOpenPlayer();
	void trayOpenArchive();
	void trayOpenImage();
	void trayOpenLinks();
	void trayOpenSearch();
	void trayOpenBrowser();
	
	void trayTakePhoto();
	void trayToggleRecord();
	void trayToggleAudioRecord();
	void trayTakeScreenshot();
	void trayToggleScreenRecord();
	
	// ★ 新增：快捷网页 → 在内部浏览器打开
	void onQuickLinkOpenInBrowser(const QUrl &url);
	
	void showWindow();
	void quitApp();
	
private:
	void buildMenus();
	void buildTray();
	void buildStatusBar();
	void restoreWindowGeometry();
	void saveWindowGeometry();
	
private:
	QTabWidget *m_tabs = nullptr;
	
	CameraRecorder       *m_cameraTab  = nullptr;
	ScreenRecorderWidget *m_screenTab  = nullptr;
	VlcPlayerWidget      *m_playerTab  = nullptr;
	ArchivePreviewWidget *m_archiveTab = nullptr;
	ImageViewerWidget    *m_imageTab   = nullptr;
	QuickLinksWidget     *m_linksTab   = nullptr;
	FileSearchWidget     *m_searchTab  = nullptr;
	WebBrowserWidget     *m_browserTab = nullptr;
	
	QLabel *m_statusTab       = nullptr;
	QLabel *m_statusRecording = nullptr;
	
	QSystemTrayIcon *m_tray     = nullptr;
	QMenu           *m_trayMenu = nullptr;
	
	QAction *m_actDarkTheme          = nullptr;
	QAction *m_actToggleRecord       = nullptr;
	QAction *m_actToggleAudio        = nullptr;
	QAction *m_actToggleScreenRecord = nullptr;
	
	bool m_reallyQuit    = false;
	bool m_firstShowDone = false;
};
