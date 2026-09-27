#include "ScreenRecorderWidget.h"
#include "SettingsManager.h"
#include "WasapiLoopbackCapture.h"

#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTimer>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QMessageBox>
#include <QPixmap>
#include <QMediaCaptureSession>
#include <QScreenCapture>
#include <QWindowCapture>
#include <QMediaRecorder>
#include <QMediaFormat>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioInput>
#include <QAudioBufferInput>
#include <QAudioBuffer>
#include <QUrl>
#include <QDebug>

// ============================================================
//                        构造 / 析构
// ============================================================
ScreenRecorderWidget::ScreenRecorderWidget(QWidget *parent)
: QWidget(parent)
{
	buildUi();
	
	// 录屏管线
	m_session = new QMediaCaptureSession(this);
	
	m_recorder = new QMediaRecorder(this);
	m_session->setRecorder(m_recorder);
	
	// WASAPI Loopback
	m_wasapi = new WasapiLoopbackCapture(this);
	m_bufferInput = new QAudioBufferInput(this);
	m_wasapi->setAudioBufferInput(m_bufferInput);
	
	connect(m_wasapi, &WasapiLoopbackCapture::errorOccurred,
			this, [this](const QString &msg) {
				qWarning() << "wasapi error:" << msg;
				if (m_statusLabel)
					m_statusLabel->setText(tr("系统音频错误: %1").arg(msg));
			});
	
	// 延迟截图定时器
	m_delayTimer = new QTimer(this);
	m_delayTimer->setSingleShot(true);
	connect(m_delayTimer, &QTimer::timeout,
			this, &ScreenRecorderWidget::onDelayCapture);
	
	// 录屏时长定时器
	m_recordTimer = new QTimer(this);
	m_recordTimer->setInterval(1000);
	connect(m_recordTimer, &QTimer::timeout,
			this, &ScreenRecorderWidget::onRecordTimerTick);
	
	// 录制错误信息
	connect(m_recorder, &QMediaRecorder::errorOccurred,
			this, [this](QMediaRecorder::Error err, const QString &msg) {
				qWarning() << "recorder error:" << int(err) << msg;
				if (m_statusLabel)
					m_statusLabel->setText(tr("录制错误: %1").arg(msg));
			});
	
	connect(m_recorder, &QMediaRecorder::recorderStateChanged,
			this, [this](QMediaRecorder::RecorderState state) {
				qDebug() << "recorder state changed:" << int(state);
			});
}

ScreenRecorderWidget::~ScreenRecorderWidget() = default;

// ============================================================
//                        UI 构建
// ============================================================
void ScreenRecorderWidget::buildUi()
{
	auto *root = new QVBoxLayout(this);
	
	auto *settingsBox = new QGroupBox(tr("录屏设置"), this);
	auto *form = new QFormLayout(settingsBox);
	
	// ---- 采集目标类型 ----
	m_captureTypeCombo = new QComboBox(settingsBox);
	m_captureTypeCombo->addItem(tr("显示器"), 0);
	m_captureTypeCombo->addItem(tr("窗口"),   1);
	connect(m_captureTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [this](int idx) {
				const bool isWindow = (idx == 1);
				if (m_screenCombo) m_screenCombo->setVisible(!isWindow);
				if (m_windowCombo) m_windowCombo->setVisible(isWindow);
				if (isWindow) updateWindowList();
				else          updateScreenList();
			});
	form->addRow(tr("采集目标："), m_captureTypeCombo);
	
	// ---- 显示器列表 ----
	m_screenCombo = new QComboBox(settingsBox);
	updateScreenList();
	connect(qApp, &QGuiApplication::screenAdded,
			this, &ScreenRecorderWidget::updateScreenList);
	connect(qApp, &QGuiApplication::screenRemoved,
			this, &ScreenRecorderWidget::updateScreenList);
	form->addRow(tr("显示器："), m_screenCombo);
	
	// ---- 窗口列表 ----
	m_windowCombo = new QComboBox(settingsBox);
	m_windowCombo->setVisible(false);
	form->addRow(tr("窗口："), m_windowCombo);
	
	// ---- 帧率 ----
	m_fpsCombo = new QComboBox(settingsBox);
	m_fpsCombo->addItem(tr("15 fps"), 15);
	m_fpsCombo->addItem(tr("24 fps"), 24);
	m_fpsCombo->addItem(tr("30 fps"), 30);
	m_fpsCombo->addItem(tr("60 fps"), 60);
	m_fpsCombo->setCurrentIndex(2);
	form->addRow(tr("帧率："), m_fpsCombo);
	
	// ---- 编码器 ----
	m_codecCombo = new QComboBox(settingsBox);
	m_codecCombo->addItem(tr("H.264"),        int(QMediaFormat::VideoCodec::H264));
	m_codecCombo->addItem(tr("H.265 / HEVC"), int(QMediaFormat::VideoCodec::H265));
	m_codecCombo->addItem(tr("AV1"),          int(QMediaFormat::VideoCodec::AV1));
	m_codecCombo->addItem(tr("自动选择"),      int(QMediaFormat::VideoCodec::Unspecified));
	m_codecCombo->setCurrentIndex(0);
	form->addRow(tr("编码器："), m_codecCombo);
	
	// ---- 音频源 ----
	m_audioSourceCombo = new QComboBox(settingsBox);
	m_audioSourceCombo->addItem(tr("无音频"),       0);
	m_audioSourceCombo->addItem(tr("系统内部声音"), 1);
	m_audioSourceCombo->addItem(tr("麦克风"),       2);
	connect(m_audioSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [this](int) {
				updateAudioDeviceList();
			});
	form->addRow(tr("音频来源："), m_audioSourceCombo);
	
	// ---- 音频设备 ----
	m_audioDeviceCombo = new QComboBox(settingsBox);
	updateAudioDeviceList();
	form->addRow(tr("音频设备："), m_audioDeviceCombo);
	
	// ---- 包含鼠标 ----
	m_chkIncludeCursor = new QCheckBox(tr("录制时包含鼠标指针"), settingsBox);
	m_chkIncludeCursor->setChecked(true);
	form->addRow(QString(), m_chkIncludeCursor);
	
	// ---- 延迟截图 ----
	m_delaySpin = new QSpinBox(settingsBox);
	m_delaySpin->setRange(0, 60);
	m_delaySpin->setSuffix(tr(" 秒"));
	form->addRow(tr("延迟截图："), m_delaySpin);
	
	root->addWidget(settingsBox);
	
	// ---- 操作按钮 ----
	auto *btnRow = new QHBoxLayout();
	m_btnShot   = new QPushButton(tr("截屏"), this);
	m_btnRecord = new QPushButton(tr("开始录屏"), this);
	btnRow->addWidget(m_btnShot);
	btnRow->addWidget(m_btnRecord);
	btnRow->addStretch();
	root->addLayout(btnRow);
	
	connect(m_btnShot, &QPushButton::clicked, this, [this]() {
		const int delay = m_delaySpin->value();
		if (delay > 0) {
			m_statusLabel->setText(tr("将在 %1 秒后截屏…").arg(delay));
			m_delayTimer->start(delay * 1000);
		} else {
			takeScreenshot();
		}
	});
	connect(m_btnRecord, &QPushButton::clicked,
			this, &ScreenRecorderWidget::toggleRecording);
	
	m_statusLabel = new QLabel(tr("就绪"), this);
	root->addWidget(m_statusLabel);
	
	m_thumbLabel = new QLabel(tr("截屏预览"), this);
	m_thumbLabel->setAlignment(Qt::AlignCenter);
	m_thumbLabel->setMinimumHeight(240);
	m_thumbLabel->setStyleSheet("background:#222;color:#aaa;");
	root->addWidget(m_thumbLabel, 1);
}

// ============================================================
//                        列表刷新
// ============================================================
void ScreenRecorderWidget::updateScreenList()
{
	if (!m_screenCombo) return;
	
	const QString prev = m_screenCombo->currentData().toString();
	m_screenCombo->clear();
	
	const auto screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); ++i) {
		QScreen *s = screens.at(i);
		const QString label = tr("显示器 %1 (%2x%3)")
		.arg(i + 1)
		.arg(s->geometry().width())
		.arg(s->geometry().height());
		m_screenCombo->addItem(label, s->name());
	}
	
	if (!prev.isEmpty()) {
		int idx = m_screenCombo->findData(prev);
		if (idx >= 0) m_screenCombo->setCurrentIndex(idx);
	}
}

void ScreenRecorderWidget::updateWindowList()
{
	if (!m_windowCombo) return;
	
	m_windowCombo->clear();
	m_capturableWindows.clear();
	
	const auto windows = QWindowCapture::capturableWindows();
	for (const QCapturableWindow &w : windows) {
		if (!w.isValid()) continue;
		m_capturableWindows.append(w);
		m_windowCombo->addItem(w.description(),
							   m_capturableWindows.size() - 1);
	}
}

void ScreenRecorderWidget::updateAudioDeviceList()
{
	if (!m_audioDeviceCombo) return;
	
	m_audioDeviceCombo->clear();
	m_audioDevices.clear();
	
	const int srcType = m_audioSourceCombo->currentData().toInt();
	
	if (srcType == 0) {
		m_audioDeviceCombo->setEnabled(false);
		return;
	}
	m_audioDeviceCombo->setEnabled(true);
	
	if (srcType == 1) {
		// 系统内部声音：不再枚举设备，WASAPI 环回直接采默认输出
		m_audioDeviceCombo->addItem(tr("系统默认输出（WASAPI 环回）"), 0);
		return;
	}
	
	// 麦克风
	const auto inputs = QMediaDevices::audioInputs();
	for (const QAudioDevice &dev : inputs) {
		const QString name = dev.description();
		const bool isLoopback =
		name.contains("Loopback", Qt::CaseInsensitive) ||
		name.contains("Stereo Mix", Qt::CaseInsensitive) ||
		name.contains("立体声混音", Qt::CaseInsensitive) ||
		name.contains("What U Hear", Qt::CaseInsensitive) ||
		name.contains("波输出", Qt::CaseInsensitive);
		if (isLoopback) continue;
		
		m_audioDevices.append(dev);
		m_audioDeviceCombo->addItem(name, m_audioDevices.size() - 1);
	}
}

// ============================================================
//                        当前选择
// ============================================================
QScreen *ScreenRecorderWidget::currentScreen() const
{
	if (!m_screenCombo) return QGuiApplication::primaryScreen();
	
	const QString name = m_screenCombo->currentData().toString();
	for (QScreen *s : QGuiApplication::screens()) {
		if (s->name() == name) return s;
	}
	return QGuiApplication::primaryScreen();
}

QCapturableWindow ScreenRecorderWidget::currentWindow() const
{
	if (!m_windowCombo || m_windowCombo->count() == 0)
		return QCapturableWindow();
	
	const int idx = m_windowCombo->currentData().toInt();
	if (idx < 0 || idx >= m_capturableWindows.size())
		return QCapturableWindow();
	
	return m_capturableWindows.at(idx);
}

// ============================================================
//                        输出路径
// ============================================================
QString ScreenRecorderWidget::makeOutputPath(const QString &suffix) const
{
	QString dir = SettingsManager::instance().cameraOutputDir();
	if (dir.isEmpty())
		dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
	if (dir.isEmpty())
		dir = QDir::homePath();
	QDir().mkpath(dir);
	const QString name =
	QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + suffix;
	return QDir(dir).filePath(name);
}

// ============================================================
//                        截屏
// ============================================================
void ScreenRecorderWidget::onDelayCapture()
{
	takeScreenshot();
}

void ScreenRecorderWidget::takeScreenshot()
{
	QScreen *screen = currentScreen();
	if (!screen) {
		m_statusLabel->setText(tr("未找到显示器"));
		return;
	}
	
	const QPixmap shot = screen->grabWindow(0);
	if (shot.isNull()) {
		m_statusLabel->setText(tr("截屏失败"));
		return;
	}
	
	m_lastShot = shot;
	
	m_thumbLabel->setPixmap(
							shot.scaled(m_thumbLabel->size(),
										Qt::KeepAspectRatio,
										Qt::SmoothTransformation));
	
	const QString path = makeOutputPath(".png");
	if (shot.save(path, "PNG")) {
		m_statusLabel->setText(tr("已保存：%1").arg(path));
	} else {
		m_statusLabel->setText(tr("保存失败：%1").arg(path));
	}
}

// ============================================================
//                        录屏
// ============================================================
void ScreenRecorderWidget::toggleRecording()
{
	if (!m_isRecording) {
		// ============ 开始录制 ============
		
		// ---- 重建 session ----
		m_session->setRecorder(nullptr);
		delete m_session;
		m_session = new QMediaCaptureSession(this);
		m_session->setRecorder(m_recorder);
		
		if (m_audioInput) {
			delete m_audioInput;
			m_audioInput = nullptr;
		}
		
		const QString path = makeOutputPath(".mp4");
		const int captureType = m_captureTypeCombo->currentData().toInt();
		
		// ---- 媒体格式 ----
		QMediaFormat fmt;
		fmt.setFileFormat(QMediaFormat::MPEG4);
		
		const auto codec = QMediaFormat::VideoCodec(
													m_codecCombo->currentData().toInt());
		if (codec != QMediaFormat::VideoCodec::Unspecified) {
			fmt.setVideoCodec(codec);
		} else {
			const auto supported = fmt.supportedVideoCodecs(QMediaFormat::Encode);
			if (supported.contains(QMediaFormat::VideoCodec::H264))
				fmt.setVideoCodec(QMediaFormat::VideoCodec::H264);
			else if (!supported.isEmpty())
				fmt.setVideoCodec(supported.first());
		}
		
		// ---- 音频 ----
		const int audioType = m_audioSourceCombo->currentData().toInt();
		
		if (audioType == 1) {
			// ==== 系统内部声音：WASAPI Loopback ====
			m_session->setAudioBufferInput(m_bufferInput);
			fmt.setAudioCodec(QMediaFormat::AudioCodec::AAC);
			
			QTimer::singleShot(0, this, [this]() {
				if (m_isRecording && !m_wasapi->start()) {
					qWarning() << "WASAPI loopback start failed";
					m_statusLabel->setText(tr("系统音频采集启动失败"));
				}
			});
		} else if (audioType == 2 && m_audioDevices.size() > 0) {
			// ==== 麦克风：QAudioInput ====
			const int devIdx = m_audioDeviceCombo->currentData().toInt();
			if (devIdx >= 0 && devIdx < m_audioDevices.size()) {
				const QAudioDevice &dev = m_audioDevices.at(devIdx);
				if (!dev.isNull()) {
					m_audioInput = new QAudioInput(dev, this);
					m_session->setAudioInput(m_audioInput);
					fmt.setAudioCodec(QMediaFormat::AudioCodec::AAC);
				}
			}
		}
		// audioType == 0：不设置任何音频源
		
		m_recorder->setMediaFormat(fmt);
		m_recorder->setQuality(QMediaRecorder::HighQuality);
		m_recorder->setOutputLocation(QUrl::fromLocalFile(path));
		
		// ---- 视频源 ----
		bool sourceReady = false;
		
		if (captureType == 0) {
			QScreen *screen = currentScreen();
			if (!screen) {
				m_statusLabel->setText(tr("未找到显示器"));
				return;
			}
			if (!m_screenCapture) {
				m_screenCapture = new QScreenCapture(this);
			}
			m_screenCapture->setScreen(screen);
			m_session->setScreenCapture(m_screenCapture);
			m_screenCapture->start();
			sourceReady = true;
		} else {
			if (m_capturableWindows.isEmpty()) {
				updateWindowList();
			}
			QCapturableWindow win = currentWindow();
			if (!win.isValid()) {
				m_statusLabel->setText(tr("未找到窗口"));
				return;
			}
			if (!m_windowCapture) {
				m_windowCapture = new QWindowCapture(this);
			}
			m_windowCapture->setWindow(win);
			m_session->setWindowCapture(m_windowCapture);
			m_windowCapture->start();
			sourceReady = true;
		}
		
		if (!sourceReady) {
			m_statusLabel->setText(tr("视频源未就绪"));
			return;
		}
		
		// ---- 延后启动 recorder ----
		m_isRecording = true;
		m_recordStart = QDateTime::currentDateTime();
		m_recordTimer->start();
		m_btnRecord->setText(tr("停止录屏"));
		m_statusLabel->setText(tr("录屏中 -> %1").arg(path));
		
		QTimer::singleShot(300, this, [this]() {
			if (!m_isRecording) return;
			m_recorder->record();
			qDebug() << "after record: state =" << int(m_recorder->recorderState())
			<< "error =" << int(m_recorder->error())
			<< "errorString =" << m_recorder->errorString();
		});
		
	} else {
		// ============ 停止 ============
		m_recordTimer->stop();
		m_recorder->stop();
		if (m_screenCapture) m_screenCapture->stop();
		if (m_windowCapture) m_windowCapture->stop();
		if (m_wasapi) m_wasapi->stop();           // 新增
		m_isRecording = false;
		m_btnRecord->setText(tr("开始录屏"));
		m_statusLabel->setText(tr("录屏已停止"));
		
		if (m_audioInput) {
			delete m_audioInput;
			m_audioInput = nullptr;
		}
	}
}

void ScreenRecorderWidget::onRecordTimerTick()
{
	if (!m_isRecording) return;
	
	const qint64 secs = m_recordStart.secsTo(QDateTime::currentDateTime());
	const int mm = int(secs / 60);
	const int ss = int(secs % 60);
	m_statusLabel->setText(tr("录屏中 %1:%2")
						   .arg(mm, 2, 10, QChar('0'))
						   .arg(ss, 2, 10, QChar('0')));
}

void ScreenRecorderWidget::stopAll()
{
	if (m_delayTimer) m_delayTimer->stop();
	if (m_isRecording) {
		m_recordTimer->stop();
		m_recorder->stop();
		m_isRecording = false;
		if (m_btnRecord) m_btnRecord->setText(tr("开始录屏"));
		if (m_audioInput) {
			delete m_audioInput;
			m_audioInput = nullptr;
		}
	}
	if (m_screenCapture) m_screenCapture->stop();
	if (m_windowCapture) m_windowCapture->stop();
	if (m_wasapi) m_wasapi->stop();              // 新增
}
