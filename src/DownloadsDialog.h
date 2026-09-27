#pragma once

#include <QDialog>

class QTreeWidget;
class QPushButton;
class QTreeWidgetItem;

class DownloadsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit DownloadsDialog(QWidget *parent = nullptr);
	
private slots:
	void refresh();
	void onItemActivated(QTreeWidgetItem *item, int column);
	void onClearClicked();
	
private:
	QTreeWidget *m_tree     = nullptr;
	QPushButton *m_btnClear = nullptr;
	QPushButton *m_btnClose = nullptr;
};
