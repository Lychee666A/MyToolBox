#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QFutureWatcher>

class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QLabel;
class QPushButton;

namespace bit7z {
	class Bit7zLibrary;
}

class ArchivePreviewWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ArchivePreviewWidget(QWidget *parent = nullptr);
	~ArchivePreviewWidget() override;
	void openArchiveDialog();
	
	// ★ 后台读取结果
	struct ArchiveEntry {
		QString  fullPath;
		quint64  size = 0;
		bool     isDir = false;
		uint32_t index = 0;
	};
	
private slots:
	void onOpenArchive();
	void onItemDoubleClicked(QTreeWidgetItem *item, int column);
	void onTreeContextMenu(const QPoint &pos);
	
private:
	void buildUi();
	void loadArchive(const QString &path);
	void onArchiveLoaded(const QList<ArchiveEntry> &entries);          // ★ 1 个参数
	void onEntryExtracted(const QByteArray &data, const QString &fullPath);  // ★ 新增
	void previewData(const QByteArray &data, const QString &fileName);
	
	QString formatSize(quint64 bytes) const;
	QString typeFromName(const QString &name) const;
	
	void extractItem(QTreeWidgetItem *item, const QString &destDir);
	void extractToDefault(QTreeWidgetItem *item);
	void extractToChosen(QTreeWidgetItem *item);
	
	QList<uint32_t> collectFileIndices(QTreeWidgetItem *item) const;
	
	QTreeWidget *m_tree = nullptr;
	QTextEdit *m_preview = nullptr;
	QLabel *m_infoLabel = nullptr;
	QPushButton *m_btnOpen = nullptr;
	
	QString m_currentArchive;
	bit7z::Bit7zLibrary *m_lib = nullptr;
	
	// ★ 异步 watcher
	QFutureWatcher<QList<ArchiveEntry>> m_loadWatcher;
	QFutureWatcher<QByteArray>          m_extractWatcher;
};
