#include "SearchEngineDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QUuid>

SearchEngineDialog::SearchEngineDialog(QWidget *parent)
	: QDialog(parent) {
	setWindowTitle(tr("管理搜索引擎"));
	setMinimumSize(680, 480);
	buildUi();

	// 拷贝一份
	m_engines = SettingsManager::instance().searchEngines();
	m_defaultId = SettingsManager::instance().defaultSearchEngineId();
	reloadList();
}

void SearchEngineDialog::buildUi() {
	auto *root = new QVBoxLayout(this);

	auto *tip = new QLabel(this);
	tip->setTextFormat(Qt::RichText);
	tip->setText(
	    tr("搜索引擎用于在地址栏输入非网址内容时进行搜索。<br>"
	   "URL 中的 <b>%1</b> 会被替换为搜索词（例如 "
	   "<code>https://www.bing.com/search?q=%1</code>）。"));
	tip->setWordWrap(true);
	root->addWidget(tip);

	auto *mid = new QHBoxLayout();
	root->addLayout(mid, 1);

	// ---- 左侧列表 ----
	auto *leftBox = new QGroupBox(tr("已添加的搜索引擎"), this);
	auto *leftLay = new QVBoxLayout(leftBox);
	m_list = new QListWidget(leftBox);
	leftLay->addWidget(m_list, 1);

	auto *btnRow = new QHBoxLayout();
	m_btnAdd    = new QPushButton(tr("添加"), leftBox);
	m_btnRemove = new QPushButton(tr("删除"), leftBox);
	btnRow->addWidget(m_btnAdd);
	btnRow->addWidget(m_btnRemove);
	btnRow->addStretch(1);
	leftLay->addLayout(btnRow);

	mid->addWidget(leftBox, 1);

	// ---- 右侧详情 ----
	auto *rightBox = new QGroupBox(tr("详细信息"), this);
	auto *form = new QFormLayout(rightBox);

	m_editName    = new QLineEdit(rightBox);
	m_editKeyword = new QLineEdit(rightBox);
	m_editKeyword->setPlaceholderText(tr("可选，例如 bing"));
	m_editUrl     = new QLineEdit(rightBox);
	m_editUrl->setPlaceholderText("https://www.example.com/search?q=%1");

	form->addRow(tr("名称："),   m_editName);
	form->addRow(tr("关键字："), m_editKeyword);
	form->addRow(tr("URL："),    m_editUrl);

	m_defaultLabel = new QLabel(rightBox);
	m_defaultLabel->setWordWrap(true);
	form->addRow(QString(), m_defaultLabel);

	m_btnSetDefault = new QPushButton(tr("设为默认"), rightBox);
	form->addRow(QString(), m_btnSetDefault);

	mid->addWidget(rightBox, 1);

	// ---- 底部按钮 ----
	auto *buttons = new QDialogButtonBox(
	    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	root->addWidget(buttons);

	// ---- 信号 ----
	connect(m_list, &QListWidget::currentRowChanged,
	        this, &SearchEngineDialog::onSelectionChanged);
	connect(m_btnAdd,        &QPushButton::clicked, this, &SearchEngineDialog::onAdd);
	connect(m_btnRemove,     &QPushButton::clicked, this, &SearchEngineDialog::onRemove);
	connect(m_btnSetDefault, &QPushButton::clicked, this, &SearchEngineDialog::onSetDefault);

	connect(buttons, &QDialogButtonBox::accepted, this, &SearchEngineDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	// 编辑框实时写回
	auto flush = [this]() {
		if (m_loading) return;
		int row = m_list->currentRow();
		if (row < 0 || row >= m_engines.size()) return;
		m_engines[row].name    = m_editName->text().trimmed();
		m_engines[row].keyword = m_editKeyword->text().trimmed();
		m_engines[row].url     = m_editUrl->text().trimmed();
		// 同步列表显示
		QString disp = m_engines[row].name.isEmpty()
		               ? m_engines[row].url : m_engines[row].name;
		if (m_engines[row].id == m_defaultId) disp += tr("  [默认]");
		m_list->item(row)->setText(disp);
	};
	connect(m_editName,    &QLineEdit::textChanged, this, flush);
	connect(m_editKeyword, &QLineEdit::textChanged, this, flush);
	connect(m_editUrl,     &QLineEdit::textChanged, this, flush);
}

void SearchEngineDialog::reloadList() {
	m_loading = true;
	m_list->clear();
	for (const auto &e : m_engines) {
		QString disp = e.name.isEmpty() ? e.url : e.name;
		if (e.id == m_defaultId) disp += tr("  [默认]");
		auto *it = new QListWidgetItem(disp, m_list);
		it->setData(Qt::UserRole, e.id);
	}
	m_loading = false;

	if (m_list->count() > 0) {
		if (m_list->currentRow() < 0) m_list->setCurrentRow(0);
		else onSelectionChanged();
	} else {
		m_editName->clear();
		m_editKeyword->clear();
		m_editUrl->clear();
	}
	updateButtons();
}

void SearchEngineDialog::onSelectionChanged() {
	int row = m_list->currentRow();
	if (row < 0 || row >= m_engines.size()) {
		updateButtons();
		return;
	}
	m_loading = true;
	const auto &e = m_engines[row];
	m_editName->setText(e.name);
	m_editKeyword->setText(e.keyword);
	m_editUrl->setText(e.url);
	m_defaultLabel->setText(e.id == m_defaultId
	                        ? tr("<i>当前为默认搜索引擎</i>")
	                        : QString());
	m_loading = false;
	updateButtons();
}

void SearchEngineDialog::updateButtons() {
	const bool has = m_list->currentRow() >= 0;
	m_btnRemove->setEnabled(has);
	m_btnSetDefault->setEnabled(has);
	const int row = m_list->currentRow();
	const bool isDefault = has && row < m_engines.size()
	                       && m_engines[row].id == m_defaultId;
	m_btnSetDefault->setEnabled(has && !isDefault);
}

bool SearchEngineDialog::flushEditorToCurrent() {
	int row = m_list->currentRow();
	if (row < 0 || row >= m_engines.size()) return false;
	m_engines[row].name    = m_editName->text().trimmed();
	m_engines[row].keyword = m_editKeyword->text().trimmed();
	m_engines[row].url     = m_editUrl->text().trimmed();
	return true;
}

void SearchEngineDialog::onAdd() {
	flushEditorToCurrent();

	SearchEngine e;
	e.id      = QUuid::createUuid().toString(QUuid::WithoutBraces);
	e.name    = tr("新搜索引擎");
	e.keyword = QString();
	e.url     = "https://www.example.com/search?q=%1";
	m_engines.append(e);

	reloadList();
	m_list->setCurrentRow(m_engines.size() - 1);
	m_editName->setFocus();
	m_editName->selectAll();
}

void SearchEngineDialog::onRemove() {
	int row = m_list->currentRow();
	if (row < 0 || row >= m_engines.size()) return;

	const QString id = m_engines[row].id;
	if (id == m_defaultId) {
		QMessageBox::warning(this, tr("提示"),
		                     tr("不能删除默认搜索引擎。请先把其他引擎设为默认。"));
		return;
	}
	if (QMessageBox::question(this, tr("删除"),
	                          tr("确定要删除「%1」吗？").arg(m_engines[row].name))
	    != QMessageBox::Yes) return;

	m_engines.removeAt(row);
	reloadList();
}

void SearchEngineDialog::onSetDefault() {
	int row = m_list->currentRow();
	if (row < 0 || row >= m_engines.size()) return;
	m_defaultId = m_engines[row].id;
	reloadList();
	onSelectionChanged();
}

void SearchEngineDialog::onAccept() {
	flushEditorToCurrent();

	// 校验
	if (m_engines.isEmpty()) {
		QMessageBox::warning(this, tr("提示"), tr("至少保留一个搜索引擎。"));
		return;
	}
	for (const auto &e : m_engines) {
		if (e.url.isEmpty()) {
			QMessageBox::warning(this, tr("提示"),
			                     tr("搜索引擎「%1」的 URL 不能为空。").arg(e.name));
			return;
		}
		if (!e.url.contains("%1")) {
			QMessageBox::warning(this, tr("提示"),
			                     tr("搜索引擎「%1」的 URL 必须包含 %1 占位符。").arg(e.name));
			return;
		}
	}
	// 确保默认 id 有效
	bool ok = false;
	for (const auto &e : m_engines) if (e.id == m_defaultId) {
			ok = true;
			break;
		}
	if (!ok) m_defaultId = m_engines.first().id;

	SettingsManager::instance().setSearchEngines(m_engines);
	SettingsManager::instance().setDefaultSearchEngineId(m_defaultId);
	accept();
}
