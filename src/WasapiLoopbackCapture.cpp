#include "WasapiLoopbackCapture.h"

#include <QDebug>
#include <QAudioBuffer>
#include <QAudioBufferInput>
#include <QAudioFormat>
#include <QDateTime>
#include <QMutexLocker>

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <comdef.h>

// 链接 WASAPI 需要的库（在 CMakeLists 里也要加）
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

// ============================================================
//                        构造 / 析构
// ============================================================
WasapiLoopbackCapture::WasapiLoopbackCapture(QObject *parent)
	: QObject(parent) {
}

WasapiLoopbackCapture::~WasapiLoopbackCapture() {
	stop();
}

// ============================================================
//                        启动 / 停止
// ============================================================
bool WasapiLoopbackCapture::start() {
	if (m_running.load()) return true;

	m_running = true;
	m_thread.setObjectName("WasapiLoopback");
	QObject::connect(&m_thread, &QThread::started,
	                 this, &WasapiLoopbackCapture::captureThreadFunc);
	m_thread.start();
	return true;
}

void WasapiLoopbackCapture::stop() {
	if (!m_running.load()) {
		// 即便没运行，也确保线程收尾
		if (m_thread.isRunning()) {
			m_thread.quit();
			m_thread.wait(2000);
		}
		return;
	}

	m_running = false;
	m_wait.wakeAll();

	if (m_thread.isRunning()) {
		m_thread.quit();
		m_thread.wait(2000);
	}
}

void WasapiLoopbackCapture::setAudioBufferInput(QAudioBufferInput *input) {
	QMutexLocker lock(&m_mutex);
	m_bufferInput = input;
}

// ============================================================
//                        采集线程
// ============================================================
void WasapiLoopbackCapture::captureThreadFunc() {
	HRESULT hr = S_OK;
	bool comInitialized = false;
	
	// ---- 1. COM 初始化 ----
	hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (SUCCEEDED(hr)) {
		comInitialized = true;
	} else if (hr == RPC_E_CHANGED_MODE) {
		comInitialized = false;
	} else {
		emit errorOccurred(QStringLiteral("CoInitializeEx failed: 0x%1")
						   .arg(static_cast<quint32>(hr), 8, 16, QChar('0')));
		m_running = false;
		return;
	}
	
	// ---- 2. 创建设备枚举器 ----
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
						  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
						  reinterpret_cast<void **>(&m_enum));
	if (FAILED(hr)) {
		emit errorOccurred(QStringLiteral("MMDeviceEnumerator failed: 0x%1")
						   .arg(hr, 0, 16));
		if (comInitialized) CoUninitialize();
		m_running = false;
		return;
	}
	
	// ---- 3. 拿默认播放设备 ----
	IMMDevice *device = nullptr;
	hr = m_enum->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	if (FAILED(hr)) {
		emit errorOccurred(QStringLiteral("GetDefaultAudioEndpoint failed: 0x%1")
						   .arg(hr, 0, 16));
		m_enum->Release(); m_enum = nullptr;
		if (comInitialized) CoUninitialize();
		m_running = false;
		return;
	}
	
	// ---- 4. 激活 IAudioClient ----
	hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
						  nullptr, reinterpret_cast<void **>(&m_audioClient));
	device->Release();
	device = nullptr;
	if (FAILED(hr)) {
		emit errorOccurred(QStringLiteral("Activate IAudioClient failed: 0x%1")
						   .arg(hr, 0, 16));
		m_enum->Release(); m_enum = nullptr;
		if (comInitialized) CoUninitialize();
		m_running = false;
		return;
	}
	
	// ---- 5. 获取混音格式 ----
	WAVEFORMATEX *mixFormat = nullptr;
	hr = m_audioClient->GetMixFormat(&mixFormat);
	if (FAILED(hr)) {
		emit errorOccurred(QStringLiteral("GetMixFormat failed: 0x%1")
						   .arg(hr, 0, 16));
		m_audioClient->Release(); m_audioClient = nullptr;
		m_enum->Release(); m_enum = nullptr;
		if (comInitialized) CoUninitialize();
		m_running = false;
		return;
	}
	
	// ---- 6. 初始化：LOOPBACK 标志 ----
	const REFERENCE_TIME bufferDuration = 10000000;
	hr = m_audioClient->Initialize(
								   AUDCLNT_SHAREMODE_SHARED,
								   AUDCLNT_STREAMFLAGS_LOOPBACK,
								   bufferDuration,
								   0,
								   mixFormat,
								   nullptr);
	if (FAILED(hr)) {
		emit errorOccurred(QStringLiteral("IAudioClient::Initialize failed: 0x%1")
						   .arg(hr, 0, 16));
		CoTaskMemFree(mixFormat);
		m_audioClient->Release(); m_audioClient = nullptr;
		m_enum->Release(); m_enum = nullptr;
		if (comInitialized) CoUninitialize();
		m_running = false;
		return;
	}
	
	// ---- 7. 拿 IAudioCaptureClient ----
	hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient),
								   reinterpret_cast<void **>(&m_captureClient));
	if (FAILED(hr)) {
		emit errorOccurred(QStringLiteral("GetService IAudioCaptureClient failed: 0x%1")
						   .arg(hr, 0, 16));
		CoTaskMemFree(mixFormat);
		m_audioClient->Release(); m_audioClient = nullptr;
		m_enum->Release(); m_enum = nullptr;
		if (comInitialized) CoUninitialize();
		m_running = false;
		return;
	}
	
	// ---- 8. 构造 QAudioFormat ----
	QAudioFormat fmt;
	fmt.setSampleRate(mixFormat->nSamplesPerSec);
	fmt.setChannelCount(mixFormat->nChannels);
	
	if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
		fmt.setSampleFormat(QAudioFormat::Float);
	} else if (mixFormat->wFormatTag == WAVE_FORMAT_PCM) {
		fmt.setSampleFormat(
							mixFormat->wBitsPerSample == 16 ? QAudioFormat::Int16
							: QAudioFormat::Int32);
	} else if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		auto *ext = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mixFormat);
		if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
			fmt.setSampleFormat(QAudioFormat::Float);
		else
			fmt.setSampleFormat(QAudioFormat::Int16);
	} else {
		fmt.setSampleFormat(QAudioFormat::Float);
	}
	
	const int bytesPerFrame = mixFormat->nBlockAlign;
	const int bytesPerSecond = mixFormat->nAvgBytesPerSec;
	
	qDebug() << "WASAPI loopback format:"
	<< "sampleRate =" << fmt.sampleRate()
	<< "channels =" << fmt.channelCount()
	<< "sampleFormat =" << int(fmt.sampleFormat())
	<< "bytesPerFrame =" << bytesPerFrame
	<< "bytesPerSecond =" << bytesPerSecond;
	
	// ---- 9. 启动 ----
	hr = m_audioClient->Start();
	if (FAILED(hr)) {
		emit errorOccurred(QStringLiteral("IAudioClient::Start failed: 0x%1")
						   .arg(hr, 0, 16));
		CoTaskMemFree(mixFormat);
		m_captureClient->Release(); m_captureClient = nullptr;
		m_audioClient->Release(); m_audioClient = nullptr;
		m_enum->Release(); m_enum = nullptr;
		if (comInitialized) CoUninitialize();
		m_running = false;
		return;
	}
	
	qDebug() << "WASAPI loopback started";
	
	// ---- 10. 采集循环 ----
	qint64 totalBytes = 0;
	qint64 lastReportMs = QDateTime::currentMSecsSinceEpoch();
	int packetCount = 0;
	int silentCount = 0;
	
	while (m_running.load()) {
		QAudioBufferInput *input = nullptr;
		{
			QMutexLocker lock(&m_mutex);
			input = m_bufferInput;
		}
		
		if (!input) {
			QThread::msleep(10);
			continue;
		}
		
		UINT32 packetLength = 0;
		hr = m_captureClient->GetNextPacketSize(&packetLength);
		if (FAILED(hr)) {
			QThread::msleep(10);
			continue;
		}
		
		if (packetLength == 0) {
			QThread::msleep(5);
			
			// 每 2 秒汇报一次吞吐
			const qint64 now = QDateTime::currentMSecsSinceEpoch();
			if (now - lastReportMs >= 2000) {
				qDebug() << "WASAPI stats:"
				<< "packets =" << packetCount
				<< "silent =" << silentCount
				<< "totalBytes =" << totalBytes
				<< "bytesPerSec(actual) ="
				<< (totalBytes * 1000 / qMax<qint64>(1, now - lastReportMs));
				packetCount = 0;
				silentCount = 0;
				totalBytes = 0;
				lastReportMs = now;
			}
			continue;
		}
		
		QByteArray pcm;
		while (packetLength > 0) {
			BYTE  *data      = nullptr;
			UINT32 numFrames = 0;
			DWORD  flags     = 0;
			
			hr = m_captureClient->GetBuffer(&data, &numFrames, &flags,
											nullptr, nullptr);
			if (FAILED(hr)) break;
			
			const UINT32 numBytes = numFrames * bytesPerFrame;
			
			if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || data == nullptr) {
				pcm.append(QByteArray(numBytes, 0));
				++silentCount;
			} else {
				pcm.append(reinterpret_cast<const char *>(data), numBytes);
			}
			++packetCount;
			
			m_captureClient->ReleaseBuffer(numFrames);
			
			hr = m_captureClient->GetNextPacketSize(&packetLength);
			if (FAILED(hr)) break;
		}
		
		if (pcm.isEmpty()) continue;
		
		totalBytes += pcm.size();
		
		// ---- 通过 QAudioBufferInput 送出 ----
		QAudioBuffer buffer(pcm, fmt, 0);
		QMetaObject::invokeMethod(input, [input, buffer]() {
			input->sendAudioBuffer(buffer);
		}, Qt::QueuedConnection);
	}
	
	// ---- 11. 清理 ----
	qDebug() << "WASAPI loopback stopping...";
	
	if (m_audioClient) m_audioClient->Stop();
	
	CoTaskMemFree(mixFormat);
	
	if (m_captureClient) { m_captureClient->Release(); m_captureClient = nullptr; }
	if (m_audioClient)   { m_audioClient->Release();   m_audioClient   = nullptr; }
	if (m_enum)          { m_enum->Release();          m_enum          = nullptr; }
	
	if (comInitialized) CoUninitialize();
	
	qDebug() << "WASAPI loopback stopped";
}
