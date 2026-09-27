#pragma once

#include <QDialog>

class QLineEdit;
class QTreeWidget;
class QPushButton;
class QTreeWidgetItem;

class HistoryDialog : public QDialog
{
	Q_OBJECT
public:
	explicit HistoryDialog(QWidget *parent = nullptr);
	
	signals:
	void openUrl(const QString &url);
	
private slots:
	void refresh();
	void onFilterChanged(const QString &kw);
	void onItemActivated(QTreeWidgetItem *item, int column);
	void onClearClicked();
	void onDeleteClicked();
	
private:
	QLineEdit    *m_filter   = nullptr;
	QTreeWidget  *m_tree     = nullptr;
	QPushButton  *m_btnClear = nullptr;
	QPushButton  *m_btnDel   = nullptr;
	QPushButton  *m_btnClose = nullptr;
};
