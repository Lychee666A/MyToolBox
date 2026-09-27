#pragma once

#include <QWidget>
#include <QPixmap>
#include <QDateTime>
#include <QCapturableWindow>
#include <QList>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QScreen;
class QTimer;
class QMediaCaptureSession;
class QScreenCapture;
class QWindowCapture;
class QMediaRecorder;
class QAudioInput;
class QAudioDevice;
class QMediaDevices;
class QAudioBufferInput;
class WasapiLoopbackCapture;

class ScreenRecorderWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ScreenRecorderWidget(QWidget *parent = nullptr);
	~ScreenRecorderWidget() override;
	
	bool isRecording() const { return m_isRecording; }
	
public slots:
	void takeScreenshot();
	void toggleRecording();
	void stopAll();
	
private slots:
	void onDelayCapture();
	void updateScreenList();
	void updateWindowList();
	void updateAudioDeviceList();
	void onRecordTimerTick();
	
private:
	void buildUi();
	QString makeOutputPath(const QString &suffix) const;
	QScreen *currentScreen() const;
	QCapturableWindow currentWindow() const;
	
	// 采集管线
	QMediaCaptureSession *m_session = nullptr;
	QScreenCapture *m_screenCapture = nullptr;
	QWindowCapture *m_windowCapture = nullptr;
	QAudioInput *m_audioInput = nullptr;
	QMediaRecorder *m_recorder = nullptr;
	
	// WASAPI Loopback
	WasapiLoopbackCapture *m_wasapi = nullptr;
	QAudioBufferInput *m_bufferInput = nullptr;
	
	// 采集目标
	QComboBox *m_captureTypeCombo = nullptr;
	QComboBox *m_screenCombo = nullptr;
	QComboBox *m_windowCombo = nullptr;
	
	QList<QCapturableWindow> m_capturableWindows;
	
	// 音频
	QComboBox *m_audioSourceCombo = nullptr;
	QComboBox *m_audioDeviceCombo = nullptr;
	QList<QAudioDevice> m_audioDevices;
	
	// 编码
	QComboBox *m_codecCombo = nullptr;
	
	QSpinBox *m_delaySpin = nullptr;
	QCheckBox *m_chkIncludeCursor = nullptr;
	QComboBox *m_fpsCombo = nullptr;
	QPushButton *m_btnShot = nullptr;
	QPushButton *m_btnRecord = nullptr;
	QLabel *m_statusLabel = nullptr;
	QLabel *m_thumbLabel = nullptr;
	
	bool m_isRecording = false;
	QPixmap m_lastShot;
	QTimer *m_delayTimer = nullptr;
	QTimer *m_recordTimer = nullptr;
	QDateTime m_recordStart;
};
