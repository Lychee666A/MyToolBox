#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <QDateTime>

struct HistoryEntry {
	QString   url;
	QString   title;
	QDateTime visitTime;
	int       visitCount = 1;
};

// 历史记录管理（QSettings 持久化）
class BrowserHistory : public QObject
{
	Q_OBJECT
public:
	static BrowserHistory &instance();
	
	// 记录访问（同 URL 会更新时间/次数）
	void recordVisit(const QString &url, const QString &title);
	
	// 全部（按时间倒序）
	QList<HistoryEntry> all() const;
	
	// 搜索（url 或 title 包含 keyword）
	QList<HistoryEntry> search(const QString &keyword) const;
	
	void clear();
	void remove(const QString &url);
	
	// ---- 最近关闭的标签（用于 Ctrl+Shift+T）----
	void pushClosedTab(const QString &url, const QString &title);
	QPair<QString, QString> popClosedTab();   // <url, title>；空表示没有
	bool hasClosedTab() const;
	
	signals:
	void historyChanged();
	
private:
	BrowserHistory();
	Q_DISABLE_COPY(BrowserHistory)
	
	void save() const;
	void load();
	
	QList<HistoryEntry>             m_entries;
	QList<QPair<QString, QString>>  m_closedTabs;
	
	static constexpr int kMaxEntries   = 5000;
	static constexpr int kMaxClosedTab = 30;
};
