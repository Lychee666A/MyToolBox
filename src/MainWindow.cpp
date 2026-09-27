#include "MainWindow.h"
#include "CameraRecorder.h"
#include "ScreenRecorderWidget.h"
#include "ArchivePreviewWidget.h"
#include "ImageViewerWidget.h"
#include "QuickLinksWidget.h"
#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "VlcPlayerWidget.h"
#include "FileSearchWidget.h"
#include "WebBrowserWidget.h"
#include "AboutDialog.h"
#include "ThemeManager.h"
#include "AppInfo.h"

#include <QTabWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QTimer>
#include <QStatusBar>
#include <QLabel>
#include <QLayout>
#include <QSettings>
#include <QScreen>
#include <QStyle>
#include <QUrl>

// ============================================================
//                        构造 / 析构
// ============================================================
MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent) {
	setWindowTitle(AppInfo::DisplayName());
	setWindowIcon(QIcon(":/icon.ico"));
	setMinimumSize(900, 600);
	
	m_tabs = new QTabWidget(this);
	m_tabs->setObjectName("mainTabWidget");
	m_tabs->setMovable(true);
	m_tabs->setDocumentMode(true);
	setCentralWidget(m_tabs);
	
	m_cameraTab  = new CameraRecorder(this);
	m_screenTab  = new ScreenRecorderWidget(this);
	m_playerTab  = new VlcPlayerWidget(this);
	m_archiveTab = new ArchivePreviewWidget(this);
	m_imageTab   = new ImageViewerWidget(this);
	m_linksTab   = new QuickLinksWidget(this);
	m_searchTab  = new FileSearchWidget(this);
	m_browserTab = new WebBrowserWidget(this);
	
	auto stdIcon = [this](QStyle::StandardPixmap sp) {
		return style()->standardIcon(sp);
	};
	m_tabs->addTab(m_cameraTab,  stdIcon(QStyle::SP_DialogSaveButton),
				   tr("拍照 / 录像 / 录音"));
	m_tabs->addTab(m_screenTab,  stdIcon(QStyle::SP_ComputerIcon),
				   tr("录屏 / 截屏"));
	m_tabs->addTab(m_playerTab,  stdIcon(QStyle::SP_MediaPlay),
				   tr("音视频播放"));
	m_tabs->addTab(m_archiveTab, stdIcon(QStyle::SP_DirIcon),
				   tr("压缩包预览"));
	m_tabs->addTab(m_imageTab,   stdIcon(QStyle::SP_FileDialogContentsView),
				   tr("图片查看"));
	m_tabs->addTab(m_linksTab,   stdIcon(QStyle::SP_DriveNetIcon),
				   tr("快捷网页"));
	m_tabs->addTab(m_searchTab,  stdIcon(QStyle::SP_FileDialogDetailedView),
				   tr("文件搜索"));
	m_tabs->addTab(m_browserTab, stdIcon(QStyle::SP_DialogHelpButton),
				   tr("浏览器"));
	
	// ★ 快捷网页 → 内部浏览器打开
	connect(m_linksTab, &QuickLinksWidget::openInBrowser,
			this, &MainWindow::onQuickLinkOpenInBrowser);
	
	buildMenus();
	buildTray();
	buildStatusBar();
	
	if (SettingsManager::instance().restoreLastTab()) {
		int idx = SettingsManager::instance().lastTabIndex();
		if (idx >= 0 && idx < m_tabs->count()) m_tabs->setCurrentIndex(idx);
	}
	
	restoreWindowGeometry();
	
	connect(&SettingsManager::instance(), &SettingsManager::settingsChanged,
			this, &MainWindow::onSettingsChanged);
	
	connect(m_tabs, &QTabWidget::currentChanged,
			this, &MainWindow::onTabChanged);
	
	connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
		if (m_cameraTab) m_cameraTab->stopAll();
		if (m_screenTab) m_screenTab->stopAll();
	});
	
	updateStatusBar();
}

MainWindow::~MainWindow() = default;

// ============================================================
//                    showEvent：首帧强制重排
// ============================================================
void MainWindow::showEvent(QShowEvent *e)
{
	QMainWindow::showEvent(e);
	if (m_firstShowDone) return;
	m_firstShowDone = true;
	
	QTimer::singleShot(0, this, [this]() {
		style()->unpolish(this);
		style()->polish(this);
		updateGeometry();
		
		const auto children = findChildren<QWidget *>();
		for (QWidget *w : children) {
			w->updateGeometry();
			w->update();
		}
		
		if (m_tabs && m_tabs->currentWidget()) {
			if (auto *lay = m_tabs->currentWidget()->layout())
				lay->activate();
		}
		
		this->update();
	});
}

// ============================================================
//                        菜单栏
// ============================================================
void MainWindow::buildMenus() {
	// ---- 设置菜单 ----
	auto *settingsMenu = menuBar()->addMenu(tr("设置"));
	auto *actOptions = settingsMenu->addAction(tr("选项…"));
	actOptions->setShortcut(QKeySequence("Ctrl+,"));
	connect(actOptions, &QAction::triggered, this, &MainWindow::openSettings);
	
	settingsMenu->addSeparator();
	
	auto *actQuit = settingsMenu->addAction(tr("退出"));
	actQuit->setShortcut(QKeySequence::Quit);
	connect(actQuit, &QAction::triggered, this, &MainWindow::quitApp);
	
	// ---- 外观菜单 ----
	auto *themeMenu = menuBar()->addMenu(tr("外观"));
	m_actDarkTheme = themeMenu->addAction(tr("深色主题"));
	m_actDarkTheme->setCheckable(true);
	m_actDarkTheme->setChecked(ThemeManager::instance().isDark());
	connect(m_actDarkTheme, &QAction::triggered,
			this, &MainWindow::toggleDarkTheme);
	
	connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
			this, [this](bool dark) {
				if (m_actDarkTheme) m_actDarkTheme->setChecked(dark);
			});
	
	// ---- 帮助菜单 ----
	auto *helpMenu = menuBar()->addMenu(tr("帮助"));
	auto *actAbout = helpMenu->addAction(tr("关于 %1…").arg(AppInfo::DisplayName()));
	actAbout->setShortcut(QKeySequence::HelpContents);
	connect(actAbout, &QAction::triggered, this, &MainWindow::openAbout);
	
	// ---- Tab 切换触发 WebView2 同步 ----
	connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx) {
		QWidget *w = m_tabs->widget(idx);
		if (w == m_browserTab && m_browserTab) {
			QTimer::singleShot(0,   m_browserTab, &WebBrowserWidget::syncWebViewSize);
			QTimer::singleShot(80,  m_browserTab, &WebBrowserWidget::syncWebViewSize);
			QTimer::singleShot(250, m_browserTab, &WebBrowserWidget::syncWebViewSize);
		}
	});
}

// ============================================================
//                        状态栏
// ============================================================
void MainWindow::buildStatusBar() {
	m_statusTab = new QLabel(this);
	m_statusTab->setMinimumWidth(200);
	
	m_statusRecording = new QLabel(this);
	m_statusRecording->setStyleSheet("color:#c00; font-weight:bold;");
	
	statusBar()->addWidget(m_statusTab, 1);
	statusBar()->addPermanentWidget(m_statusRecording);
	statusBar()->setSizeGripEnabled(true);
}

// ============================================================
//                        托盘
// ============================================================
void MainWindow::buildTray() {
	if (!QSystemTrayIcon::isSystemTrayAvailable()) {
		return;
	}
	
	m_tray = new QSystemTrayIcon(QIcon(":/icon.ico"), this);
	m_tray->setToolTip(AppInfo::DisplayName());
	
	m_trayMenu = new QMenu(this);
	
	QAction *actShow = m_trayMenu->addAction(tr("显示主窗口"));
	connect(actShow, &QAction::triggered, this, &MainWindow::showWindow);
	
	m_trayMenu->addSeparator();
	
	QAction *actCamera  = m_trayMenu->addAction(tr("拍照 / 录像 / 录音"));
	QAction *actScreen  = m_trayMenu->addAction(tr("录屏 / 截屏"));
	QAction *actPlayer  = m_trayMenu->addAction(tr("音视频播放"));
	QAction *actArchive = m_trayMenu->addAction(tr("压缩包预览"));
	QAction *actImage   = m_trayMenu->addAction(tr("图片查看"));
	QAction *actLinks   = m_trayMenu->addAction(tr("快捷网页"));
	QAction *actSearch  = m_trayMenu->addAction(tr("文件搜索"));
	QAction *actBrowser = m_trayMenu->addAction(tr("浏览器"));
	
	connect(actCamera,  &QAction::triggered, this, &MainWindow::trayOpenCamera);
	connect(actScreen,  &QAction::triggered, this, &MainWindow::trayOpenScreen);
	connect(actPlayer,  &QAction::triggered, this, &MainWindow::trayOpenPlayer);
	connect(actArchive, &QAction::triggered, this, &MainWindow::trayOpenArchive);
	connect(actImage,   &QAction::triggered, this, &MainWindow::trayOpenImage);
	connect(actLinks,   &QAction::triggered, this, &MainWindow::trayOpenLinks);
	connect(actSearch,  &QAction::triggered, this, &MainWindow::trayOpenSearch);
	connect(actBrowser, &QAction::triggered, this, &MainWindow::trayOpenBrowser);
	
	m_trayMenu->addSeparator();
	
	QAction *actPhoto = m_trayMenu->addAction(tr("拍照"));
	m_actToggleRecord = m_trayMenu->addAction(tr("开始录像"));
	m_actToggleAudio  = m_trayMenu->addAction(tr("开始录音"));
	
	connect(actPhoto,          &QAction::triggered, this, &MainWindow::trayTakePhoto);
	connect(m_actToggleRecord, &QAction::triggered, this, &MainWindow::trayToggleRecord);
	connect(m_actToggleAudio,  &QAction::triggered, this, &MainWindow::trayToggleAudioRecord);
	
	m_trayMenu->addSeparator();
	
	QAction *actShot = m_trayMenu->addAction(tr("截屏"));
	m_actToggleScreenRecord = m_trayMenu->addAction(tr("开始录屏"));
	
	connect(actShot,                 &QAction::triggered, this, &MainWindow::trayTakeScreenshot);
	connect(m_actToggleScreenRecord, &QAction::triggered, this, &MainWindow::trayToggleScreenRecord);
	
	m_trayMenu->addSeparator();
	
	QAction *actOpenImg = m_trayMenu->addAction(tr("打开图片…"));
	QAction *actOpenArc = m_trayMenu->addAction(tr("打开压缩包…"));
	connect(actOpenImg, &QAction::triggered, this, [this]() {
		showWindow();
		m_tabs->setCurrentWidget(m_imageTab);
		m_imageTab->openImageDialog();
	});
	connect(actOpenArc, &QAction::triggered, this, [this]() {
		showWindow();
		m_tabs->setCurrentWidget(m_archiveTab);
		m_archiveTab->openArchiveDialog();
	});
	
	m_trayMenu->addSeparator();
	
	QAction *actAbout = m_trayMenu->addAction(tr("关于…"));
	connect(actAbout, &QAction::triggered, this, &MainWindow::openAbout);
	
	m_trayMenu->addSeparator();
	
	QAction *actOptions = m_trayMenu->addAction(tr("设置…"));
	connect(actOptions, &QAction::triggered, this, &MainWindow::openSettings);
	
	QAction *actQuit = m_trayMenu->addAction(tr("退出"));
	connect(actQuit, &QAction::triggered, this, &MainWindow::quitApp);
	
	m_tray->setContextMenu(m_trayMenu);
	
	connect(m_tray, &QSystemTrayIcon::activated,
			this, &MainWindow::onTrayActivated);
	
	connect(m_trayMenu, &QMenu::aboutToShow,
			this, &MainWindow::updateTrayActions);
	
	m_tray->show();
}

void MainWindow::updateTrayActions() {
	if (m_cameraTab) {
		if (m_actToggleRecord) {
			m_actToggleRecord->setText(m_cameraTab->isRecording()
									   ? tr("停止录像")
									   : tr("开始录像"));
		}
		if (m_actToggleAudio) {
			m_actToggleAudio->setText(m_cameraTab->isAudioRecording()
									  ? tr("停止录音")
									  : tr("开始录音"));
		}
	}
	
	if (m_screenTab && m_actToggleScreenRecord) {
		m_actToggleScreenRecord->setText(m_screenTab->isRecording()
										 ? tr("停止录屏")
										 : tr("开始录屏"));
	}
	
	updateStatusBar();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
	if (reason == QSystemTrayIcon::Trigger ||
		reason == QSystemTrayIcon::DoubleClick) {
		showWindow();
	}
}

// ============================================================
//                        托盘各动作
// ============================================================
void MainWindow::trayOpenCamera()  { showWindow(); m_tabs->setCurrentWidget(m_cameraTab); }
void MainWindow::trayOpenScreen()  { showWindow(); m_tabs->setCurrentWidget(m_screenTab); }
void MainWindow::trayOpenPlayer()  { showWindow(); m_tabs->setCurrentWidget(m_playerTab); }
void MainWindow::trayOpenArchive() { showWindow(); m_tabs->setCurrentWidget(m_archiveTab); }
void MainWindow::trayOpenImage()   { showWindow(); m_tabs->setCurrentWidget(m_imageTab); }
void MainWindow::trayOpenLinks()   { showWindow(); m_tabs->setCurrentWidget(m_linksTab); }
void MainWindow::trayOpenSearch()  { showWindow(); m_tabs->setCurrentWidget(m_searchTab); }

void MainWindow::trayOpenBrowser() {
	showWindow();
	m_tabs->setCurrentWidget(m_browserTab);
	QTimer::singleShot(0,   m_browserTab, &WebBrowserWidget::syncWebViewSize);
	QTimer::singleShot(80,  m_browserTab, &WebBrowserWidget::syncWebViewSize);
	QTimer::singleShot(250, m_browserTab, &WebBrowserWidget::syncWebViewSize);
}

void MainWindow::trayTakePhoto() {
	if (!m_cameraTab) return;
	m_cameraTab->onTakePhoto();
}

void MainWindow::trayToggleRecord() {
	if (!m_cameraTab) return;
	m_cameraTab->onToggleRecord();
	updateTrayActions();
}

void MainWindow::trayToggleAudioRecord() {
	if (!m_cameraTab) return;
	m_cameraTab->onToggleAudioRecord();
	updateTrayActions();
}

void MainWindow::trayTakeScreenshot() {
	if (!m_screenTab) return;
	m_screenTab->takeScreenshot();
}

void MainWindow::trayToggleScreenRecord() {
	if (!m_screenTab) return;
	m_screenTab->toggleRecording();
	updateTrayActions();
}

// ============================================================
//                        通用
// ============================================================
void MainWindow::showWindow() {
	show();
	setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
	raise();
	activateWindow();
}

void MainWindow::quitApp() {
	if (m_cameraTab) m_cameraTab->stopAll();
	if (m_screenTab) m_screenTab->stopAll();
	
	m_reallyQuit = true;
	if (m_tray) m_tray->hide();
	qApp->quit();
}

void MainWindow::openSettings() {
	SettingsDialog dlg(this);
	dlg.exec();
}

void MainWindow::onSettingsChanged() {
	// 各 Tab 自行响应
}

void MainWindow::openAbout() {
	AboutDialog dlg(this);
	dlg.exec();
}

void MainWindow::toggleDarkTheme(bool dark) {
	ThemeManager::instance().setDark(dark);
}

// ============================================================
//               ★ 快捷网页 → 内部浏览器打开
// ============================================================
void MainWindow::onQuickLinkOpenInBrowser(const QUrl &url)
{
	if (!url.isValid() || url.isEmpty()) return;
	if (!m_browserTab) return;
	
	// 1. 切到浏览器 Tab
	showWindow();
	m_tabs->setCurrentWidget(m_browserTab);
	
	// 2. 让浏览器新建标签页并打开
	m_browserTab->openUrlInNewTab(url.toString());
	
	// 3. 切 Tab 后同步 WebView2 尺寸
	QTimer::singleShot(0,   m_browserTab, &WebBrowserWidget::syncWebViewSize);
	QTimer::singleShot(80,  m_browserTab, &WebBrowserWidget::syncWebViewSize);
	QTimer::singleShot(250, m_browserTab, &WebBrowserWidget::syncWebViewSize);
}

// ============================================================
//                     Tab 变化 / 状态栏刷新
// ============================================================
void MainWindow::onTabChanged(int index) {
	Q_UNUSED(index)
	
	updateStatusBar();
	
	if (m_tabs && m_tabs->currentWidget()) {
		setWindowTitle(QString("%1 - %2")
					   .arg(m_tabs->tabText(m_tabs->currentIndex()),
							AppInfo::DisplayName()));
	}
}

void MainWindow::updateStatusBar() {
	if (m_statusTab && m_tabs) {
		m_statusTab->setText(tr("当前页面：%1")
							 .arg(m_tabs->tabText(m_tabs->currentIndex())));
	}
	
	if (m_statusRecording) {
		QStringList recs;
		if (m_cameraTab && m_cameraTab->isRecording())      recs << tr("录像中");
		if (m_cameraTab && m_cameraTab->isAudioRecording()) recs << tr("录音中");
		if (m_screenTab && m_screenTab->isRecording())      recs << tr("录屏中");
		m_statusRecording->setText(recs.isEmpty() ? QString()
								   : "● " + recs.join(" / "));
	}
}

// ============================================================
//                     窗口大小/位置
// ============================================================
void MainWindow::restoreWindowGeometry() {
	QSettings s;
	QByteArray geo = s.value("window/geometry").toByteArray();
	if (!geo.isEmpty()) {
		restoreGeometry(geo);
	} else {
		resize(1100, 720);
		if (QScreen *scr = QApplication::primaryScreen()) {
			move(scr->availableGeometry().center() - rect().center());
		}
	}
}

void MainWindow::saveWindowGeometry() {
	QSettings s;
	s.setValue("window/geometry", saveGeometry());
}

// ============================================================
//                        resize / close
// ============================================================
void MainWindow::resizeEvent(QResizeEvent *e) {
	QMainWindow::resizeEvent(e);
	if (m_browserTab && m_browserTab->isVisible()) {
		m_browserTab->syncWebViewSize();
	}
}

void MainWindow::closeEvent(QCloseEvent *e) {
	if (SettingsManager::instance().restoreLastTab()) {
		SettingsManager::instance().setLastTabIndex(m_tabs->currentIndex());
	}
	
	saveWindowGeometry();
	
	if (m_tray && !m_reallyQuit) {
		e->ignore();
		hide();
		m_tray->showMessage(tr("仍在后台运行"),
							tr("程序已最小化到托盘，录像/录音不会中断。"),
							QSystemTrayIcon::Information, 2000);
		return;
	}
	
	QMainWindow::closeEvent(e);
}
