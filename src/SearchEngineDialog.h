#pragma once

#include <QDialog>
#include "SettingsManager.h"

class QListWidget;
class QLineEdit;
class QPushButton;
class QLabel;

// 搜索引擎管理对话框（参照 Edge 的"管理搜索引擎"界面）
class SearchEngineDialog : public QDialog
{
	Q_OBJECT
public:
	explicit SearchEngineDialog(QWidget *parent = nullptr);
	
private slots:
	void onSelectionChanged();
	void onAdd();
	void onRemove();
	void onSetDefault();
	void onAccept();
	
private:
	void buildUi();
	void reloadList();
	void updateButtons();
	// 把当前编辑框内容写回选中的条目
	bool flushEditorToCurrent();
	
	QListWidget *m_list = nullptr;
	QLineEdit   *m_editName = nullptr;
	QLineEdit   *m_editKeyword = nullptr;
	QLineEdit   *m_editUrl = nullptr;
	QPushButton *m_btnAdd = nullptr;
	QPushButton *m_btnRemove = nullptr;
	QPushButton *m_btnSetDefault = nullptr;
	QLabel      *m_defaultLabel = nullptr;
	
	QList<SearchEngine> m_engines;   // 工作副本
	QString m_defaultId;
	bool m_loading = false;          // 防止 reloadList 时触发 selectionChanged 误写
};
