#include "CameraRecorder.h"
#include "SettingsManager.h"

#include <QCamera>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QTimer>
#include <QMediaCaptureSession>
#include <QImageCapture>
#include <QMediaRecorder>
#include <QVideoWidget>
#include <QAudioInput>
#include <QMediaFormat>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QMediaDevices>
#include <QDebug>
#include <QStyle>

// ============================================================
//                        构造 / 析构
// ============================================================
CameraRecorder::CameraRecorder(QWidget *parent)
	: QWidget(parent) {
	buildUi();

	m_session = new QMediaCaptureSession(this);

	// 摄像头（先创建空对象，具体设备在 startCamera() 里设）
	m_camera = new QCamera(this);
	m_session->setCamera(m_camera);

	// ★ 摄像头错误监听
	connect(m_camera, &QCamera::errorOccurred,
	this, [this](QCamera::Error err, const QString & msg) {
		if (err == QCamera::NoError) return;

		qWarning() << "[Camera] errorOccurred:" << int(err) << msg;

		m_cameraActive = false;
		if (m_btnToggleCamera) {
			m_btnToggleCamera->setText(tr("开启摄像头"));
			m_btnToggleCamera->setProperty("accent", true);
			m_btnToggleCamera->style()->unpolish(m_btnToggleCamera);
			m_btnToggleCamera->style()->polish(m_btnToggleCamera);
		}

		if (m_statusLabel) {
			m_statusLabel->setText(tr("摄像头错误：%1").arg(msg));
		}
	});

	// ★ 视频预览：必须 Expanding，且布局给 stretch
	m_videoWidget = new QVideoWidget(this);
	m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_videoWidget->setMinimumHeight(200);
	m_session->setVideoOutput(m_videoWidget);
	if (auto *lay = qobject_cast<QVBoxLayout *>(layout())) {
		lay->addWidget(m_videoWidget, 1);   // ★ stretch=1，吃掉剩余空间
	} else {
		layout()->addWidget(m_videoWidget);
	}

	// 拍照
	m_imageCapture = new QImageCapture(this);
	m_session->setImageCapture(m_imageCapture);
	connect(m_imageCapture, &QImageCapture::imageSaved,
	this, [this](int, const QString & fileName) {
		m_statusLabel->setText(tr("照片已保存: %1").arg(fileName));
	});

	// 录像
	m_recorder = new QMediaRecorder(this);
	m_session->setRecorder(m_recorder);

	// 录音（先不激活，切换时使用）
	m_audioInput = new QAudioInput(this);

	// ★ 枚举摄像头
	refreshCameraList();

	// ★ 按设置决定是否自动开启
	QTimer::singleShot(0, this, [this]() {
		if (!m_cameraCombo || m_cameraCombo->count() == 0) {
			m_statusLabel->setText(tr("未检测到摄像头"));
			if (m_btnToggleCamera) m_btnToggleCamera->setEnabled(false);
			return;
		}

		// 根据设置决定
		if (SettingsManager::instance().cameraAutoStart()) {
			startCamera();
		} else {
			// 不自动开启：按钮文字应为"开启摄像头"，状态提示"未开启"
			if (m_btnToggleCamera) {
				m_btnToggleCamera->setText(tr("开启摄像头"));
				m_btnToggleCamera->setProperty("accent", true);
				m_btnToggleCamera->style()->unpolish(m_btnToggleCamera);
				m_btnToggleCamera->style()->polish(m_btnToggleCamera);
			}
			m_statusLabel->setText(tr("摄像头未开启，点击「开启摄像头」开始"));
		}
	});
}

CameraRecorder::~CameraRecorder() = default;

// ============================================================
//                        UI 构建
// ============================================================
void CameraRecorder::buildUi() {
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(6, 6, 6, 6);   // ★ 紧凑边距
	root->setSpacing(6);                     // ★ 统一间距

	// ---- 第一行：摄像头选择 + 开关 ----
	auto *camRow = new QHBoxLayout();
	camRow->setContentsMargins(0, 0, 0, 0);
	camRow->setSpacing(6);

	QLabel *camLabel = new QLabel(tr("摄像头："), this);
	m_cameraCombo = new QComboBox(this);
	m_cameraCombo->setMinimumWidth(220);
	m_cameraCombo->setMaximumWidth(420);                                       // ★ 上限
	m_cameraCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);  // ★ 不 Expanding

	m_btnToggleCamera = new QPushButton(tr("关闭摄像头"), this);
	m_btnToggleCamera->setMinimumWidth(110);
	m_btnToggleCamera->setProperty("accent", true);

	camRow->addWidget(camLabel);
	camRow->addWidget(m_cameraCombo);    // ★ 不给 stretch
	camRow->addStretch(1);                // ★ 用 stretch 顶开
	camRow->addWidget(m_btnToggleCamera);
	root->addLayout(camRow);

	// ---- 第二行：拍照 / 录像 / 录音 ----
	auto *btnRow = new QHBoxLayout();
	btnRow->setContentsMargins(0, 0, 0, 0);
	btnRow->setSpacing(6);

	m_btnPhoto  = new QPushButton(tr("拍照"), this);
	m_btnRecord = new QPushButton(tr("开始录像"), this);
	m_btnAudio  = new QPushButton(tr("开始录音"), this);
	m_btnPhoto->setProperty("accent", true);

	btnRow->addWidget(m_btnPhoto);
	btnRow->addWidget(m_btnRecord);
	btnRow->addWidget(m_btnAudio);
	btnRow->addStretch();
	root->addLayout(btnRow);

	// ---- 状态标签 ----
	m_statusLabel = new QLabel(tr("就绪"), this);
	m_statusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	root->addWidget(m_statusLabel);

	// ---- 信号 ----
	connect(m_cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &CameraRecorder::onCameraSelectionChanged);
	connect(m_btnToggleCamera, &QPushButton::clicked,
	        this, &CameraRecorder::onToggleCamera);
	connect(m_btnPhoto,  &QPushButton::clicked, this, &CameraRecorder::onTakePhoto);
	connect(m_btnRecord, &QPushButton::clicked, this, &CameraRecorder::onToggleRecord);
	connect(m_btnAudio,  &QPushButton::clicked, this, &CameraRecorder::onToggleAudioRecord);
}

// ============================================================
//                        摄像头枚举
// ============================================================
void CameraRecorder::refreshCameraList() {
	if (!m_cameraCombo) return;

	const QByteArray prevId = m_cameraCombo->currentData().toByteArray();

	m_cameraCombo->blockSignals(true);
	m_cameraCombo->clear();

	const auto devices = QMediaDevices::videoInputs();
	for (const QCameraDevice &dev : devices) {
		m_cameraCombo->addItem(dev.description(), QVariant(dev.id()));
	}

	if (!prevId.isEmpty()) {
		int idx = m_cameraCombo->findData(prevId);
		if (idx >= 0) m_cameraCombo->setCurrentIndex(idx);
	}
	m_cameraCombo->blockSignals(false);

	const bool hasDevice = (m_cameraCombo->count() > 0);
	if (m_btnToggleCamera) m_btnToggleCamera->setEnabled(hasDevice);
}

// ============================================================
//                        启动 / 停止
// ============================================================
void CameraRecorder::applySelectedCamera() {
	if (!m_cameraCombo || !m_camera) return;

	const QByteArray id = m_cameraCombo->currentData().toByteArray();
	if (id.isEmpty()) return;

	const auto devices = QMediaDevices::videoInputs();
	for (const QCameraDevice &dev : devices) {
		if (dev.id() == id) {
			m_camera->setCameraDevice(dev);
			qDebug() << "[Camera] selected:" << dev.description();
			return;
		}
	}
}

void CameraRecorder::startCamera() {
	if (!m_camera || m_cameraActive) return;
	if (m_cameraCombo && m_cameraCombo->count() == 0) {
		m_statusLabel->setText(tr("未检测到摄像头"));
		return;
	}

	if (m_isAudioRecording) {
		m_statusLabel->setText(tr("录音中，暂不能开启摄像头"));
		return;
	}

	applySelectedCamera();

	m_camera->start();
	m_cameraActive = true;

	if (m_btnToggleCamera) {
		m_btnToggleCamera->setText(tr("关闭摄像头"));
		m_btnToggleCamera->setProperty("accent", true);
		m_btnToggleCamera->style()->unpolish(m_btnToggleCamera);
		m_btnToggleCamera->style()->polish(m_btnToggleCamera);
	}
	if (m_cameraCombo) m_cameraCombo->setEnabled(true);

	if (m_statusLabel) {
		m_statusLabel->setText(tr("摄像头已开启：%1")
		                       .arg(m_cameraCombo ? m_cameraCombo->currentText() : QString()));
	}

	QTimer::singleShot(500, this, [this]() {
		if (!m_camera) return;
		if (!m_camera->isActive() && m_cameraActive) {
			m_cameraActive = false;
			if (m_btnToggleCamera) {
				m_btnToggleCamera->setText(tr("开启摄像头"));
				m_btnToggleCamera->setProperty("accent", true);
				m_btnToggleCamera->style()->unpolish(m_btnToggleCamera);
				m_btnToggleCamera->style()->polish(m_btnToggleCamera);
			}
			if (m_statusLabel) {
				m_statusLabel->setText(tr("摄像头启动失败，可能被其他程序占用"));
			}
		}
	});
}

void CameraRecorder::stopCamera() {
	if (!m_camera || !m_cameraActive) return;

	m_camera->stop();
	m_cameraActive = false;

	if (m_btnToggleCamera) {
		m_btnToggleCamera->setText(tr("开启摄像头"));
		m_btnToggleCamera->setProperty("accent", true);
		m_btnToggleCamera->style()->unpolish(m_btnToggleCamera);
		m_btnToggleCamera->style()->polish(m_btnToggleCamera);
	}
	if (m_statusLabel) m_statusLabel->setText(tr("摄像头已关闭"));
}

void CameraRecorder::onToggleCamera() {
	if (m_cameraActive) stopCamera();
	else                startCamera();
}

void CameraRecorder::onCameraSelectionChanged(int /*index*/) {
	if (m_cameraActive) {
		m_camera->stop();
		m_cameraActive = false;
		startCamera();
	}
}

// ============================================================
//                        停止全部
// ============================================================
void CameraRecorder::stopAll() {
	if (m_isRecording) {
		m_recorder->stop();
		m_isRecording = false;
		if (m_btnRecord) m_btnRecord->setText(tr("开始录像"));
	}
	if (m_isAudioRecording) {
		m_recorder->stop();
		m_isAudioRecording = false;
		if (m_btnAudio) m_btnAudio->setText(tr("开始录音"));
	}
	m_cameraWasActiveBeforeAudio = false;
}

// ============================================================
//                        输出路径
// ============================================================
QString CameraRecorder::makeOutputPath(const QString &suffix) const {
	QString dir = SettingsManager::instance().cameraOutputDir();
	if (dir.isEmpty())
		dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
	if (dir.isEmpty())
		dir = QDir::homePath();
	QDir().mkpath(dir);
	QString name = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + suffix;
	return QDir(dir).filePath(name);
}

// ============================================================
//                        拍照
// ============================================================
void CameraRecorder::onTakePhoto() {
	if (!m_cameraActive) {
		QMessageBox::information(this, tr("提示"),
		                         tr("摄像头未开启。请先点击「开启摄像头」。"));
		return;
	}

	QString path = makeOutputPath(".jpg");
	m_imageCapture->captureToFile(path);
	m_statusLabel->setText(tr("正在拍照..."));
}

// ============================================================
//                        录像
// ============================================================
void CameraRecorder::onToggleRecord() {
	if (!m_isRecording) {
		if (!m_cameraActive) {
			QMessageBox::information(this, tr("提示"),
			                         tr("摄像头未开启。请先点击「开启摄像头」。"));
			return;
		}

		QString path = makeOutputPath(".mp4");

		QMediaFormat fmt;
		fmt.setFileFormat(QMediaFormat::MPEG4);
		fmt.setVideoCodec(QMediaFormat::VideoCodec::H264);
		fmt.setAudioCodec(QMediaFormat::AudioCodec::AAC);
		m_recorder->setMediaFormat(fmt);
		m_recorder->setQuality(QMediaRecorder::HighQuality);
		m_recorder->setOutputLocation(QUrl::fromLocalFile(path));

		m_recorder->record();
		m_isRecording = true;
		m_btnRecord->setText(tr("停止录像"));
		m_statusLabel->setText(tr("录像中 -> %1").arg(path));
	} else {
		m_recorder->stop();
		m_isRecording = false;
		m_btnRecord->setText(tr("开始录像"));
		m_statusLabel->setText(tr("录像已停止"));
	}
}

// ============================================================
//                        录音
// ============================================================
void CameraRecorder::onToggleAudioRecord() {
	if (!m_isAudioRecording) {
		m_cameraWasActiveBeforeAudio = m_cameraActive;

		m_camera->stop();
		m_cameraActive = false;
		m_session->setCamera(nullptr);

		if (m_btnToggleCamera) {
			m_btnToggleCamera->setText(tr("开启摄像头"));
			m_btnToggleCamera->setProperty("accent", true);
			m_btnToggleCamera->style()->unpolish(m_btnToggleCamera);
			m_btnToggleCamera->style()->polish(m_btnToggleCamera);
		}
		if (m_cameraCombo) m_cameraCombo->setEnabled(false);

		m_session->setAudioInput(m_audioInput);

		QString path = makeOutputPath(".m4a");
		QMediaFormat fmt;
		fmt.setFileFormat(QMediaFormat::MPEG4);
		fmt.setAudioCodec(QMediaFormat::AudioCodec::AAC);
		m_recorder->setMediaFormat(fmt);
		m_recorder->setOutputLocation(QUrl::fromLocalFile(path));

		m_recorder->record();
		m_isAudioRecording = true;
		m_btnAudio->setText(tr("停止录音"));
		m_statusLabel->setText(tr("录音中 -> %1").arg(path));
	} else {
		m_recorder->stop();
		m_session->setAudioInput(nullptr);
		m_session->setCamera(m_camera);

		m_isAudioRecording = false;
		m_btnAudio->setText(tr("开始录音"));

		if (m_cameraCombo) m_cameraCombo->setEnabled(true);
		m_statusLabel->setText(tr("录音已停止"));

		if (m_cameraWasActiveBeforeAudio) {
			m_cameraWasActiveBeforeAudio = false;
			QTimer::singleShot(200, this, [this]() {
				startCamera();
			});
		}
	}
}
