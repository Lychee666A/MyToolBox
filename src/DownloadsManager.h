#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

struct DownloadEntry {
	QString   url;
	QString   filePath;
	QString   fileName;
	qint64    totalBytes    = 0;
	qint64    receivedBytes = 0;
	QString   state;   // "delegated" / "in_progress" / "completed" / "interrupted"
	QDateTime startTime;
	QDateTime endTime;
};

// 下载记录管理（下载动作交给系统默认下载器，如 Edge）
class DownloadsManager : public QObject
{
	Q_OBJECT
public:
	static DownloadsManager &instance();
	
	// 通过系统默认下载器下载 URL
	bool downloadViaSystem(const QString &url);
	
	void addDownload(const DownloadEntry &e);
	void updateDownload(const QString &url, qint64 received, qint64 total,
						const QString &state);
	
	QList<DownloadEntry> all() const;
	void clearRecords();
	
	signals:
	void downloadsChanged();
	
private:
	DownloadsManager();
	Q_DISABLE_COPY(DownloadsManager)
	
	void save();
	void load();
	
	QList<DownloadEntry> m_entries;
};
