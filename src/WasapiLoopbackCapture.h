#pragma once

#include <QObject>
#include <QAudioFormat>
#include <QAudioBuffer>
#include <QAudioBufferInput>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

// 前向声明 WASAPI 类型，避免头文件污染
struct IAudioClient;
struct IAudioCaptureClient;
struct IMMDeviceEnumerator;

class WasapiLoopbackCapture : public QObject
{
	Q_OBJECT
public:
	explicit WasapiLoopbackCapture(QObject *parent = nullptr);
	~WasapiLoopbackCapture() override;
	
	// 启动/停止 WASAPI 采集线程
	bool start();
	void stop();
	
	// 把采集到的 PCM 数据通过 QAudioBufferInput 送给 recorder
	void setAudioBufferInput(QAudioBufferInput *input);
	
	bool isRunning() const { return m_running.load(); }
	
	signals:
	void errorOccurred(const QString &msg);
	
private:
	void captureThreadFunc();
	
	// WASAPI 对象
	IMMDeviceEnumerator *m_enum       = nullptr;
	IAudioClient        *m_audioClient = nullptr;
	IAudioCaptureClient *m_captureClient = nullptr;
	
	QAudioBufferInput *m_bufferInput = nullptr;
	
	QThread m_thread;
	std::atomic<bool> m_running{false};
	QMutex m_mutex;
	QWaitCondition m_wait;
};
