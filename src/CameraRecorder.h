#pragma once

#include <QWidget>

class QCamera;
class QMediaCaptureSession;
class QImageCapture;
class QMediaRecorder;
class QVideoWidget;
class QAudioInput;
class QLabel;
class QPushButton;
class QComboBox;
class QMediaDevices;

class CameraRecorder : public QWidget
{
	Q_OBJECT
public:
	explicit CameraRecorder(QWidget *parent = nullptr);
	~CameraRecorder() override;
	
	bool isRecording() const      { return m_isRecording; }
	bool isAudioRecording() const { return m_isAudioRecording; }
	bool isCameraActive() const   { return m_cameraActive; }
	
public slots:
	void onTakePhoto();
	void onToggleRecord();
	void onToggleAudioRecord();
	void stopAll();
	
	void refreshCameraList();
	void onCameraSelectionChanged(int index);
	void onToggleCamera();
	
private:
	void buildUi();
	QString makeOutputPath(const QString &suffix) const;
	
	void startCamera();
	void stopCamera();
	void applySelectedCamera();
	
	QMediaCaptureSession *m_session = nullptr;
	QCamera *m_camera = nullptr;
	QImageCapture *m_imageCapture = nullptr;
	QMediaRecorder *m_recorder = nullptr;
	QAudioInput *m_audioInput = nullptr;
	QVideoWidget *m_videoWidget = nullptr;
	
	QComboBox   *m_cameraCombo = nullptr;
	QPushButton *m_btnToggleCamera = nullptr;
	
	QPushButton *m_btnPhoto = nullptr;
	QPushButton *m_btnRecord = nullptr;
	QPushButton *m_btnAudio = nullptr;
	QLabel *m_statusLabel = nullptr;
	
	bool m_isRecording = false;
	bool m_isAudioRecording = false;
	bool m_cameraActive = false;
	
	// ★ 新增：进录音前摄像头是否开着，用于录音结束后恢复
	bool m_cameraWasActiveBeforeAudio = false;
};
