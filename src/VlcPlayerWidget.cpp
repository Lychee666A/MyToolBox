#include "VlcPlayerWidget.h"

#include <vlc/vlc.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QSplitter>
#include <QFileDialog>
#include <QTimer>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>

// ============================================================
//                        构造 / 析构
// ============================================================
VlcPlayerWidget::VlcPlayerWidget(QWidget *parent)
: QWidget(parent)
{
	buildUi();
	
	// 强制 native window
	m_videoFrame->setAttribute(Qt::WA_NativeWindow);
	m_videoFrame->winId();
	
	// 显式指定 plugin path
	const QString pluginPath = QDir(QCoreApplication::applicationDirPath())
	.filePath("plugins");
	const QByteArray pluginPathUtf8 = pluginPath.toUtf8();
	
	const char *args[] = {
		"--plugin-path", pluginPathUtf8.constData(),
		nullptr
	};
	
	m_vlc = libvlc_new(2, args);
	if (!m_vlc) {
		qDebug() << "libvlc_new FAILED";
		m_timeLabel->setText(tr("libvlc 初始化失败"));
	} else {
		qDebug() << "libvlc_new OK, version:" << libvlc_get_version();
	}
	
	// 初始化音量
	if (m_mp) {
		libvlc_audio_set_volume(m_mp, m_lastVolume);
	}
	
	// ★ 先创建 timer 并连接，但不立即启动（等有媒体再启）
	m_timer = new QTimer(this);
	m_timer->setInterval(300);
	connect(m_timer, &QTimer::timeout, this, &VlcPlayerWidget::onTick);
	m_timer->start();
	
	// 支持拖拽
	setAcceptDrops(true);
	setFocusPolicy(Qt::StrongFocus);
}

VlcPlayerWidget::~VlcPlayerWidget()
{
	// ★ 先停 timer，避免后续操作触发 onTick
	if (m_timer) {
		m_timer->stop();
	}
	
	if (m_mp) {
		libvlc_media_player_stop(m_mp);
		libvlc_media_player_release(m_mp);
		m_mp = nullptr;
	}
	if (m_vlc) {
		libvlc_release(m_vlc);
		m_vlc = nullptr;
	}
}

// ============================================================
//                        界面
// ============================================================
void VlcPlayerWidget::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 4, 4, 4);
	root->setSpacing(4);
	
	// ---- 视频区 + 播放列表（可拖动分割） ----
	m_splitter = new QSplitter(Qt::Horizontal, this);
	
	m_videoFrame = new QWidget(m_splitter);
	m_videoFrame->setStyleSheet("background-color: black;");
	m_videoFrame->setMinimumSize(480, 320);
	
	m_playlist = new QListWidget(m_splitter);
	m_playlist->setMinimumWidth(180);
	m_playlist->setAlternatingRowColors(true);
	
	m_splitter->addWidget(m_videoFrame);
	m_splitter->addWidget(m_playlist);
	m_splitter->setStretchFactor(0, 3);
	m_splitter->setStretchFactor(1, 1);
	m_splitter->setSizes({ 700, 220 });
	
	root->addWidget(m_splitter, 1);
	
	// ---- 进度条 ----
	auto *progressRow = new QHBoxLayout();
	m_slider = new QSlider(Qt::Horizontal, this);
	m_slider->setRange(0, 1000);
	m_timeLabel = new QLabel(tr("00:00 / 00:00"), this);
	m_timeLabel->setMinimumWidth(110);
	
	progressRow->addWidget(m_slider, 1);
	progressRow->addWidget(m_timeLabel);
	root->addLayout(progressRow);
	
	// ---- 控制栏 ----
	auto *ctrl = new QHBoxLayout();
	
	m_btnOpen       = new QPushButton(tr("打开"), this);
	m_btnOpenFolder = new QPushButton(tr("打开文件夹"), this);
	m_btnPrev       = new QPushButton(tr("上一曲"), this);
	m_btnPlayPause  = new QPushButton(tr("播放"), this);
	m_btnStop       = new QPushButton(tr("停止"), this);
	m_btnNext       = new QPushButton(tr("下一曲"), this);
	m_btnMute       = new QPushButton(tr("静音"), this);
	m_btnFullscreen = new QPushButton(tr("全屏"), this);
	
	m_volume = new QSlider(Qt::Horizontal, this);
	m_volume->setRange(0, 100);
	m_volume->setValue(m_lastVolume);
	m_volume->setFixedWidth(100);
	
	m_volumeLabel = new QLabel(tr("80%"), this);
	m_volumeLabel->setMinimumWidth(40);
	
	m_speedCombo = new QComboBox(this);
	m_speedCombo->addItem("0.5x", 0.5);
	m_speedCombo->addItem("0.75x", 0.75);
	m_speedCombo->addItem("1.0x", 1.0);
	m_speedCombo->addItem("1.25x", 1.25);
	m_speedCombo->addItem("1.5x", 1.5);
	m_speedCombo->addItem("2.0x", 2.0);
	m_speedCombo->setCurrentIndex(2);
	
	ctrl->addWidget(m_btnOpen);
	ctrl->addWidget(m_btnOpenFolder);
	ctrl->addSpacing(10);
	ctrl->addWidget(m_btnPrev);
	ctrl->addWidget(m_btnPlayPause);
	ctrl->addWidget(m_btnStop);
	ctrl->addWidget(m_btnNext);
	ctrl->addSpacing(10);
	ctrl->addWidget(m_btnMute);
	ctrl->addWidget(m_volume);
	ctrl->addWidget(m_volumeLabel);
	ctrl->addSpacing(10);
	ctrl->addWidget(new QLabel(tr("倍速："), this));
	ctrl->addWidget(m_speedCombo);
	ctrl->addStretch();
	ctrl->addWidget(m_btnFullscreen);
	
	root->addLayout(ctrl);
	
	// ---- 信号连接 ----
	connect(m_btnOpen,       &QPushButton::clicked, this, &VlcPlayerWidget::onOpenFile);
	connect(m_btnOpenFolder, &QPushButton::clicked, this, &VlcPlayerWidget::onOpenFolder);
	connect(m_btnPrev,       &QPushButton::clicked, this, &VlcPlayerWidget::onPrev);
	connect(m_btnPlayPause,  &QPushButton::clicked, this, &VlcPlayerWidget::onPlayPause);
	connect(m_btnStop,       &QPushButton::clicked, this, &VlcPlayerWidget::onStop);
	connect(m_btnNext,       &QPushButton::clicked, this, &VlcPlayerWidget::onNext);
	connect(m_btnMute,       &QPushButton::clicked, this, &VlcPlayerWidget::onMuteToggled);
	connect(m_btnFullscreen, &QPushButton::clicked, this, &VlcPlayerWidget::onToggleFullscreen);
	
	connect(m_slider, &QSlider::sliderPressed,  this, &VlcPlayerWidget::onSliderPressed);
	connect(m_slider, &QSlider::sliderReleased, this, &VlcPlayerWidget::onSliderReleased);
	connect(m_slider, &QSlider::sliderMoved,    this, &VlcPlayerWidget::onSliderMoved);
	
	connect(m_volume, &QSlider::valueChanged, this, &VlcPlayerWidget::onVolumeChanged);
	
	connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &VlcPlayerWidget::onSpeedChanged);
	
	connect(m_playlist, &QListWidget::itemDoubleClicked,
			this, &VlcPlayerWidget::onPlaylistDoubleClicked);
	
	// 按钮初始状态
	updateButtons();
}

// ============================================================
//                        拖拽
// ============================================================
void VlcPlayerWidget::dragEnterEvent(QDragEnterEvent *e)
{
	if (e->mimeData()->hasUrls())
		e->acceptProposedAction();
}

void VlcPlayerWidget::dropEvent(QDropEvent *e)
{
	QStringList files;
	for (const QUrl &u : e->mimeData()->urls()) {
		if (u.isLocalFile())
			files.append(u.toLocalFile());
	}
	if (files.isEmpty()) return;
	
	setPlaylist(files, 0);
	playCurrent();
	e->acceptProposedAction();
}

// ============================================================
//                        键盘 / 鼠标
// ============================================================
void VlcPlayerWidget::keyPressEvent(QKeyEvent *e)
{
	switch (e->key()) {
	case Qt::Key_Space:
		onPlayPause();
		e->accept();
		break;
	case Qt::Key_Escape:
		if (m_fullscreen) onToggleFullscreen();
		e->accept();
		break;
	case Qt::Key_Left:
		if (m_mp) {
			libvlc_time_t t = libvlc_media_player_get_time(m_mp);
			libvlc_media_player_set_time(m_mp, qMax<libvlc_time_t>(0, t - 5000));
		}
		e->accept();
		break;
	case Qt::Key_Right:
		if (m_mp) {
			libvlc_time_t t = libvlc_media_player_get_time(m_mp);
			libvlc_media_player_set_time(m_mp, t + 5000);
		}
		e->accept();
		break;
	case Qt::Key_Up:
		m_volume->setValue(qMin(100, m_volume->value() + 5));
		e->accept();
		break;
	case Qt::Key_Down:
		m_volume->setValue(qMax(0, m_volume->value() - 5));
		e->accept();
		break;
	default:
		QWidget::keyPressEvent(e);
	}
}

void VlcPlayerWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
	Q_UNUSED(e)
	onToggleFullscreen();
}

// ============================================================
//                        打开
// ============================================================
void VlcPlayerWidget::onOpenFile()
{
	QStringList files = QFileDialog::getOpenFileNames(
													  this, tr("选择媒体文件"), QString(),
													  tr("媒体文件 (*.mp4 *.mkv *.avi *.mov *.wmv *.flv *.webm *.mp3 *.flac *.wav *.m4a *.aac *.ogg);;所有文件 (*.*)"));
	
	if (files.isEmpty()) return;
	
	setPlaylist(files, 0);
	playCurrent();
}

void VlcPlayerWidget::onOpenFolder()
{
	const QString dir = QFileDialog::getExistingDirectory(this, tr("选择文件夹"));
	if (dir.isEmpty()) return;
	
	QDir d(dir);
	const QStringList filters = {
		"*.mp4", "*.mkv", "*.avi", "*.mov", "*.wmv", "*.flv", "*.webm",
		"*.mp3", "*.flac", "*.wav", "*.m4a", "*.aac", "*.ogg"
	};
	QFileInfoList infos = d.entryInfoList(filters, QDir::Files, QDir::Name);
	if (infos.isEmpty()) return;
	
	QStringList files;
	for (const QFileInfo &fi : infos) files.append(fi.absoluteFilePath());
	
	setPlaylist(files, 0);
	playCurrent();
}

// ============================================================
//                        播放列表
// ============================================================
void VlcPlayerWidget::setPlaylist(const QStringList &files, int startIndex)
{
	m_files = files;
	m_currentIndex = (startIndex >= 0 && startIndex < files.size()) ? startIndex : 0;
	refreshPlaylistView();
}

void VlcPlayerWidget::refreshPlaylistView()
{
	m_playlist->clear();
	for (const QString &f : m_files) {
		m_playlist->addItem(QFileInfo(f).fileName());
	}
	if (m_currentIndex >= 0 && m_currentIndex < m_playlist->count()) {
		m_playlist->setCurrentRow(m_currentIndex);
	}
}

void VlcPlayerWidget::onPlaylistDoubleClicked()
{
	const int row = m_playlist->currentRow();
	if (row < 0 || row >= m_files.size()) return;
	m_currentIndex = row;
	playCurrent();
}

// ============================================================
//                        加载 / 播放
// ============================================================
bool VlcPlayerWidget::loadMedia(const QString &path)
{
	if (!m_vlc) return false;
	
	if (m_mp) {
		// ★ 先停再释放。VLC 的 stop 会等待内部线程，不能省。
		libvlc_media_player_stop(m_mp);
		libvlc_media_player_release(m_mp);
		m_mp = nullptr;
	}
	
	const QString fileUrl = QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString();
	qDebug() << "loadMedia:" << fileUrl;
	
	libvlc_media_t *media = libvlc_media_new_location(m_vlc, fileUrl.toUtf8().constData());
	if (!media) {
		qDebug() << "media null";
		return false;
	}
	
	m_mp = libvlc_media_player_new_from_media(media);
	libvlc_media_release(media);
	
	if (!m_mp) {
		qDebug() << "m_mp null";
		return false;
	}
	
	attachVlcToWindow();
	libvlc_audio_set_volume(m_mp, m_muted ? 0 : m_volume->value());
	
	return true;
}

// ★ 异步播放：延迟到事件循环
void VlcPlayerWidget::playCurrent()
{
	if (m_currentIndex < 0 || m_currentIndex >= m_files.size()) return;
	
	const QString path = m_files.at(m_currentIndex);
	const int index = m_currentIndex;
	
	// 延迟到下一次事件循环，避免在信号/槽里同步阻塞
	QTimer::singleShot(0, this, [this, path, index]() {
		if (!loadMedia(path)) return;
		
		const int ret = libvlc_media_player_play(m_mp);
		qDebug() << "play ret =" << ret;
		
		m_slider->setValue(0);
		m_timeLabel->setText(tr("加载中..."));
		updateButtons();
		
		if (index >= 0 && index < m_playlist->count())
			m_playlist->setCurrentRow(index);
	});
}

void VlcPlayerWidget::playFile(const QString &path)
{
	setPlaylist({ path }, 0);
	playCurrent();
}

// ============================================================
//                        控制
// ============================================================
void VlcPlayerWidget::onPlayPause()
{
	if (!m_mp) {
		if (!m_files.isEmpty()) playCurrent();
		return;
	}
	
	const auto state = libvlc_media_player_get_state(m_mp);
	if (state == libvlc_Playing) {
		libvlc_media_player_pause(m_mp);
	} else if (state == libvlc_Paused) {
		libvlc_media_player_play(m_mp);
	} else {
		// Stopped / Ended / Error
		playCurrent();
	}
	updateButtons();
}

void VlcPlayerWidget::togglePause()
{
	onPlayPause();
}

// ★ 停止：延迟到事件循环
void VlcPlayerWidget::onStop()
{
	if (!m_mp) return;
	
	QTimer::singleShot(0, this, [this]() {
		if (!m_mp) return;
		libvlc_media_player_stop(m_mp);
		m_slider->setValue(0);
		m_timeLabel->setText(tr("00:00 / 00:00"));
		updateButtons();
	});
}

void VlcPlayerWidget::stop()
{
	onStop();
}

void VlcPlayerWidget::onPrev()
{
	if (m_files.isEmpty()) return;
	m_currentIndex = (m_currentIndex - 1 + m_files.size()) % m_files.size();
	playCurrent();
}

void VlcPlayerWidget::onNext()
{
	if (m_files.isEmpty()) return;
	m_currentIndex = (m_currentIndex + 1) % m_files.size();
	playCurrent();
}

// ============================================================
//                        进度
// ============================================================
void VlcPlayerWidget::onSliderPressed()
{
	m_sliderDragging = true;
}

void VlcPlayerWidget::onSliderReleased()
{
	m_sliderDragging = false;
	if (!m_mp) return;
	const float pos = m_slider->value() / 1000.0f;
	libvlc_media_player_set_position(m_mp, pos);
}

void VlcPlayerWidget::onSliderMoved(int value)
{
	if (!m_sliderDragging) return;
	if (!m_mp) return;
	
	libvlc_time_t len = libvlc_media_player_get_length(m_mp);
	if (len > 0) {
		libvlc_time_t target = len * value / 1000;
		m_timeLabel->setText(QString("%1 / %2")
							 .arg([target]{
								 int s = int(target/1000);
								 int m = s/60; s%=60;
								 return QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
							 }())
							 .arg([len]{
								 int s = int(len/1000);
								 int m = s/60; s%=60;
								 return QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
							 }()));
	}
}

// ============================================================
//                        音量 / 静音
// ============================================================
void VlcPlayerWidget::onVolumeChanged(int value)
{
	m_volumeLabel->setText(QString("%1%").arg(value));
	if (m_mp) libvlc_audio_set_volume(m_mp, value);
	if (value > 0) m_muted = false;
	updateButtons();
}

void VlcPlayerWidget::onMuteToggled(bool /*muted*/)
{
	if (!m_mp) return;
	m_muted = !m_muted;
	libvlc_audio_set_volume(m_mp, m_muted ? 0 : m_volume->value());
	updateButtons();
}

// ============================================================
//                        倍速
// ============================================================
void VlcPlayerWidget::onSpeedChanged(int index)
{
	if (!m_mp) return;
	const float rate = m_speedCombo->itemData(index).toFloat();
	if (rate > 0) libvlc_media_player_set_rate(m_mp, rate);
}

// ============================================================
//                        全屏
// ============================================================
void VlcPlayerWidget::onToggleFullscreen()
{
	m_fullscreen = !m_fullscreen;
	
	if (m_fullscreen) {
		m_playlist->hide();
		m_btnFullscreen->setText(tr("退出全屏"));
		window()->showFullScreen();
	} else {
		m_playlist->show();
		m_btnFullscreen->setText(tr("全屏"));
		window()->showNormal();
	}
}

// ============================================================
//                        定时刷新
// ============================================================
void VlcPlayerWidget::updateTimeLabel()
{
	if (!m_mp) {
		m_timeLabel->setText(tr("00:00 / 00:00"));
		return;
	}
	
	libvlc_time_t len = libvlc_media_player_get_length(m_mp);
	libvlc_time_t cur = libvlc_media_player_get_time(m_mp);
	
	if (len <= 0) {
		m_timeLabel->setText(tr("--:-- / --:--"));
		return;
	}
	
	auto fmt = [](libvlc_time_t ms) {
		int s = int(ms / 1000);
		int m = s / 60; s %= 60;
		return QString("%1:%2").arg(m, 2, 10, QChar('0'))
		.arg(s, 2, 10, QChar('0'));
	};
	m_timeLabel->setText(fmt(cur) + " / " + fmt(len));
}

void VlcPlayerWidget::onTick()
{
	if (!m_mp) {
		updateButtons();
		return;
	}
	
	libvlc_time_t len = libvlc_media_player_get_length(m_mp);
	libvlc_time_t cur = libvlc_media_player_get_time(m_mp);
	
	if (!m_sliderDragging && len > 0) {
		int v = int((double)cur / (double)len * 1000.0);
		m_slider->blockSignals(true);
		m_slider->setValue(v);
		m_slider->blockSignals(false);
	}
	
	updateTimeLabel();
	
	// ★ 播放结束自动下一曲：延迟到事件循环，避免重入
	const auto state = libvlc_media_player_get_state(m_mp);
	if (state == libvlc_Ended) {
		if (!m_files.isEmpty()) {
			QTimer::singleShot(0, this, [this]() {
				onNext();
			});
		}
	}
	
	updateButtons();
}

// ============================================================
//                        状态刷新
// ============================================================
void VlcPlayerWidget::updateButtons()
{
	bool hasMedia = (m_mp != nullptr);
	bool playing  = false;
	
	if (m_mp) {
		auto state = libvlc_media_player_get_state(m_mp);
		playing = (state == libvlc_Playing);
	}
	
	m_btnPlayPause->setText(playing ? tr("暂停") : tr("播放"));
	m_btnPrev->setEnabled(!m_files.isEmpty());
	m_btnNext->setEnabled(!m_files.isEmpty());
	m_btnStop->setEnabled(hasMedia);
	m_btnMute->setText(m_muted ? tr("取消静音") : tr("静音"));
}

void VlcPlayerWidget::attachVlcToWindow()
{
	if (!m_mp) return;
#ifdef Q_OS_WIN
	libvlc_media_player_set_hwnd(m_mp, (void*)m_videoFrame->winId());
#elif defined(Q_OS_LINUX)
	libvlc_media_player_set_xwindow(m_mp, m_videoFrame->winId());
#elif defined(Q_OS_MAC)
	libvlc_media_player_set_nsobject(m_mp, (void*)m_videoFrame->winId());
#endif
}
