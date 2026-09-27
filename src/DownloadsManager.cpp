#include "DownloadsManager.h"
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>

DownloadsManager &DownloadsManager::instance()
{
	static DownloadsManager s;
	return s;
}

DownloadsManager::DownloadsManager() : QObject(nullptr)
{
	load();
}

bool DownloadsManager::downloadViaSystem(const QString &url)
{
	if (url.isEmpty()) return false;
	
	// QDesktopServices::openUrl 让系统决定（Edge 会接手下载）
	bool ok = QDesktopServices::openUrl(QUrl(url));
	
	if (ok) {
		DownloadEntry e;
		e.url       = url;
		e.fileName  = url.section('/', -1);
		e.state     = "delegated";
		e.startTime = QDateTime::currentDateTime();
		addDownload(e);
	}
	return ok;
}

void DownloadsManager::addDownload(const DownloadEntry &e)
{
	m_entries.prepend(e);
	while (m_entries.size() > 500)
		m_entries.removeLast();
	save();
	emit downloadsChanged();
}

void DownloadsManager::updateDownload(const QString &url, qint64 received,
									  qint64 total, const QString &state)
{
	for (auto &e : m_entries) {
		if (e.url == url) {
			e.receivedBytes = received;
			e.totalBytes    = total;
			e.state         = state;
			if (state == "completed" || state == "interrupted")
				e.endTime = QDateTime::currentDateTime();
			save();
			emit downloadsChanged();
			return;
		}
	}
}

QList<DownloadEntry> DownloadsManager::all() const
{
	return m_entries;
}

void DownloadsManager::clearRecords()
{
	m_entries.clear();
	save();
	emit downloadsChanged();
}

void DownloadsManager::save()
{
	QSettings s;
	s.beginWriteArray("browser/downloads", m_entries.size());
	for (int i = 0; i < m_entries.size(); ++i) {
		s.setArrayIndex(i);
		s.setValue("url",        m_entries[i].url);
		s.setValue("filePath",   m_entries[i].filePath);
		s.setValue("fileName",   m_entries[i].fileName);
		s.setValue("totalBytes", m_entries[i].totalBytes);
		s.setValue("received",   m_entries[i].receivedBytes);
		s.setValue("state",      m_entries[i].state);
		s.setValue("startTime",  m_entries[i].startTime);
		s.setValue("endTime",    m_entries[i].endTime);
	}
	s.endArray();
}

void DownloadsManager::load()
{
	QSettings s;
	int n = s.beginReadArray("browser/downloads");
	for (int i = 0; i < n; ++i) {
		s.setArrayIndex(i);
		DownloadEntry e;
		e.url           = s.value("url").toString();
		e.filePath      = s.value("filePath").toString();
		e.fileName      = s.value("fileName").toString();
		e.totalBytes    = s.value("totalBytes").toLongLong();
		e.receivedBytes = s.value("received").toLongLong();
		e.state         = s.value("state").toString();
		e.startTime     = s.value("startTime").toDateTime();
		e.endTime       = s.value("endTime").toDateTime();
		if (!e.url.isEmpty()) m_entries.append(e);
	}
	s.endArray();
}
