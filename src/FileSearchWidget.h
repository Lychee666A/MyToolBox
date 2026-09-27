#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QFutureWatcher>
#include <QSettings>

#include "EverythingApi.h"

class QLineEdit;
class QComboBox;
class QPushButton;
class QTableWidget;
class QLabel;
class QCheckBox;
class QTimer;
class QMenu;
class QAction;

class FileSearchWidget : public QWidget
{
	Q_OBJECT
public:
	explicit FileSearchWidget(QWidget *parent = nullptr);
	~FileSearchWidget() override;
	
private slots:
	void onSearch();
	void onSearchTextChanged();
	void onSearchFinished();
	void onHeaderClicked(int column);
	void onHeaderContextMenu(const QPoint &pos);
	void onResultDoubleClicked(int row, int col);
	void onResultContextMenu(const QPoint &pos);
	void onOpenFile();
	void onOpenFolder();
	void onCopyPath();
	void onLocateInExplorer();
	void onCopyFile();
	void onCutFile();
	void onDeleteFile();
	void onRenameFile();
	void onShowProperties();
	void onExportCsv();
	void onExportTxt();
	void onLoadMore();
	void checkDbReady();
	
private:
	void buildUi();
	void refreshStatus();
	void fillTable(const QList<EverythingResult> &results, bool append);
	void setSearchEnabled(bool enabled);
	void saveColumnVisibility();
	void loadColumnVisibility();
	
	// UI
	QLineEdit    *m_searchEdit = nullptr;
	QComboBox    *m_historyCombo = nullptr;
	QPushButton  *m_btnSearch = nullptr;
	QPushButton  *m_btnExportCsv = nullptr;
	QPushButton  *m_btnExportTxt = nullptr;
	QPushButton  *m_btnLoadMore = nullptr;
	QPushButton  *m_btnOpen = nullptr;
	QPushButton  *m_btnOpenDir = nullptr;
	QCheckBox    *m_chkMatchCase = nullptr;
	QCheckBox    *m_chkWholeWord = nullptr;
	QCheckBox    *m_chkMatchPath = nullptr;
	QCheckBox    *m_chkRegex = nullptr;
	QComboBox    *m_typeCombo = nullptr;
	QComboBox    *m_sortCombo = nullptr;
	QTableWidget *m_table = nullptr;
	QLabel       *m_statusLabel = nullptr;
	QLabel       *m_statsLabel = nullptr;
	QTimer       *m_debounceTimer = nullptr;
	QTimer       *m_dbCheckTimer = nullptr;
	
	QFutureWatcher<QList<EverythingResult>> m_searchWatcher;
	
	// 状态
	bool m_cursorActive = false;
	bool m_sortAscending = true;
	int  m_sortColumn = 0;
	int  m_currentOffset = 0;
	int  m_pageSize = 1000;
	EverythingStats m_lastStats;
	QString m_lastQuery;
	EverythingQueryOptions m_lastOptions;
	
	// 列
	enum Column {
		ColName = 0, ColPath, ColExt, ColSize,
		ColDateModified, ColDateCreated, ColDateAccessed, ColAttributes
	};
};
