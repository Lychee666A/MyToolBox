#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
#include <QMutex>
#include <QIcon>

// 单条结果
struct EverythingResult
{
	QString   fullPath;
	quint64   size = 0;
	QDateTime modified;
	QDateTime dateCreated;
	QDateTime dateAccessed;
	QDateTime dateRecentlyChanged;
	quint32   attributes = 0;
	quint32   runCount = 0;
	bool      isFolder = false;
	QString   extension;    // "cpp"
};

// 排序字段（对应 Everything SDK 的 EVERYTHING_SORT_*）
enum class EverythingSortBy {
	Name,
	Path,
	Size,
	Extension,
	DateModified,
	DateCreated,
	DateAccessed,
	Attributes,
	RunCount,
	DateRecentlyChanged,
};

// 查询选项
struct EverythingQueryOptions
{
	bool matchCase       = false;
	bool matchWholeWord  = false;
	bool matchMatchPath  = false;
	bool regex           = false;
	bool matchDiacritics = false;
	bool filesOnly       = false;
	bool foldersOnly     = false;
	
	EverythingSortBy sortBy = EverythingSortBy::Name;
	bool sortAscending = true;
	
	int  offset      = 0;      // ★ 分页起始
	int  maxResults  = 1000;   // ★ 单页大小
};

// 结果统计
struct EverythingStats
{
	quint32 totalResults   = 0;   // Everything 里总命中数
	quint32 totalFiles     = 0;
	quint32 totalFolders   = 0;
	quint64 totalSizeBytes = 0;
};

class EverythingApi
{
public:
	static EverythingApi &instance();
	
	bool isAvailable() const { return m_available; }
	bool isEverythingRunning() const;
	bool isDbLoaded() const;
	
	// ★ 搜索并返回统计信息
	QList<EverythingResult> search(const QString &query,
								   const EverythingQueryOptions &opt,
								   EverythingStats *statsOut = nullptr);
	
	// ★ 兼容旧接口
	QList<EverythingResult> search(const QString &query,
								   const EverythingQueryOptions &opt = {})
	{
		return search(query, opt, nullptr);
	}
	
	QString lastError() const { return m_lastError; }
	EverythingStats lastStats() const { return m_lastStats; }
	
private:
	EverythingApi();
	~EverythingApi();
	Q_DISABLE_COPY(EverythingApi)
	
	bool loadDll();
	void resolveFunctions();
	
	bool    m_available = false;
	void   *m_dll = nullptr;
	QString m_lastError;
	QMutex  m_mutex;
	EverythingStats m_lastStats;
};
