#include "BrowserHistory.h"
#include <QSettings>

BrowserHistory &BrowserHistory::instance()
{
	static BrowserHistory s;
	return s;
}

BrowserHistory::BrowserHistory() : QObject(nullptr)
{
	load();
}

void BrowserHistory::recordVisit(const QString &url, const QString &title)
{
	if (url.isEmpty()) return;
	if (url.startsWith("about:") || url.startsWith("edge:")
		|| url.startsWith("data:") || url.startsWith("file:///"))
		return;
	
	for (auto &e : m_entries) {
		if (e.url == url) {
			e.visitTime  = QDateTime::currentDateTime();
			e.visitCount += 1;
			if (!title.isEmpty()) e.title = title;
			save();
			emit historyChanged();
			return;
		}
	}
	
	HistoryEntry e;
	e.url        = url;
	e.title      = title;
	e.visitTime  = QDateTime::currentDateTime();
	e.visitCount = 1;
	m_entries.prepend(e);
	
	while (m_entries.size() > kMaxEntries)
		m_entries.removeLast();
	
	save();
	emit historyChanged();
}

QList<HistoryEntry> BrowserHistory::all() const
{
	return m_entries;
}

QList<HistoryEntry> BrowserHistory::search(const QString &keyword) const
{
	if (keyword.isEmpty()) return m_entries;
	QList<HistoryEntry> r;
	for (const auto &e : m_entries) {
		if (e.url.contains(keyword, Qt::CaseInsensitive) ||
			e.title.contains(keyword, Qt::CaseInsensitive))
			r.append(e);
	}
	return r;
}

void BrowserHistory::clear()
{
	m_entries.clear();
	save();
	emit historyChanged();
}

void BrowserHistory::remove(const QString &url)
{
	for (int i = m_entries.size() - 1; i >= 0; --i) {
		if (m_entries[i].url == url)
			m_entries.removeAt(i);
	}
	save();
	emit historyChanged();
}

void BrowserHistory::pushClosedTab(const QString &url, const QString &title)
{
	if (url.isEmpty()) return;
	m_closedTabs.prepend(qMakePair(url, title));
	while (m_closedTabs.size() > kMaxClosedTab)
		m_closedTabs.removeLast();
	save();
}

QPair<QString, QString> BrowserHistory::popClosedTab()
{
	if (m_closedTabs.isEmpty()) return {};
	auto p = m_closedTabs.takeFirst();
	save();
	return p;
}

bool BrowserHistory::hasClosedTab() const
{
	return !m_closedTabs.isEmpty();
}

void BrowserHistory::save() const
{
	QSettings s;
	s.beginWriteArray("browser/history", m_entries.size());
	for (int i = 0; i < m_entries.size(); ++i) {
		s.setArrayIndex(i);
		s.setValue("url",        m_entries[i].url);
		s.setValue("title",      m_entries[i].title);
		s.setValue("time",       m_entries[i].visitTime);
		s.setValue("visitCount", m_entries[i].visitCount);
	}
	s.endArray();
	
	s.beginWriteArray("browser/closedTabs", m_closedTabs.size());
	for (int i = 0; i < m_closedTabs.size(); ++i) {
		s.setArrayIndex(i);
		s.setValue("url",   m_closedTabs[i].first);
		s.setValue("title", m_closedTabs[i].second);
	}
	s.endArray();
}

void BrowserHistory::load()
{
	QSettings s;
	int n = s.beginReadArray("browser/history");
	for (int i = 0; i < n; ++i) {
		s.setArrayIndex(i);
		HistoryEntry e;
		e.url        = s.value("url").toString();
		e.title      = s.value("title").toString();
		e.visitTime  = s.value("time").toDateTime();
		e.visitCount = s.value("visitCount", 1).toInt();
		if (!e.url.isEmpty()) m_entries.append(e);
	}
	s.endArray();
	
	n = s.beginReadArray("browser/closedTabs");
	for (int i = 0; i < n; ++i) {
		s.setArrayIndex(i);
		QString u = s.value("url").toString();
		QString t = s.value("title").toString();
		if (!u.isEmpty()) m_closedTabs.append(qMakePair(u, t));
	}
	s.endArray();
}
