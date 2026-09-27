#include "HistoryDialog.h"
#include "BrowserHistory.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QHeaderView>
#include <QMessageBox>

HistoryDialog::HistoryDialog(QWidget *parent)
: QDialog(parent)
{
	setWindowTitle(tr("历史记录"));
	resize(900, 600);
	
	auto *root = new QVBoxLayout(this);
	
	auto *filterRow = new QHBoxLayout();
	m_filter = new QLineEdit(this);
	m_filter->setPlaceholderText(tr("搜索历史记录…"));
	filterRow->addWidget(new QLabel(tr("搜索:"), this));
	filterRow->addWidget(m_filter, 1);
	root->addLayout(filterRow);
	
	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(3);
	m_tree->setHeaderLabels({ tr("标题"), tr("网址"), tr("访问时间") });
	m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_tree->setRootIsDecorated(false);
	m_tree->setAlternatingRowColors(true);
	root->addWidget(m_tree, 1);
	
	auto *btnRow = new QHBoxLayout();
	m_btnClear = new QPushButton(tr("清空全部"), this);
	m_btnDel   = new QPushButton(tr("删除选中"), this);
	m_btnClose = new QPushButton(tr("关闭"), this);
	btnRow->addWidget(m_btnClear);
	btnRow->addWidget(m_btnDel);
	btnRow->addStretch();
	btnRow->addWidget(m_btnClose);
	root->addLayout(btnRow);
	
	connect(m_filter, &QLineEdit::textChanged, this, &HistoryDialog::onFilterChanged);
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &HistoryDialog::onItemActivated);
	connect(m_btnClear, &QPushButton::clicked, this, &HistoryDialog::onClearClicked);
	connect(m_btnDel,   &QPushButton::clicked, this, &HistoryDialog::onDeleteClicked);
	connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
	
	refresh();
}

void HistoryDialog::refresh()
{
	m_tree->clear();
	const QString kw = m_filter->text().trimmed();
	const auto list = kw.isEmpty()
	? BrowserHistory::instance().all()
	: BrowserHistory::instance().search(kw);
	
	for (const auto &e : list) {
		auto *it = new QTreeWidgetItem(m_tree);
		it->setText(0, e.title.isEmpty() ? tr("(无标题)") : e.title);
		it->setText(1, e.url);
		it->setText(2, e.visitTime.toString("yyyy-MM-dd hh:mm:ss"));
		it->setData(0, Qt::UserRole, e.url);
	}
}

void HistoryDialog::onFilterChanged(const QString &) { refresh(); }

void HistoryDialog::onItemActivated(QTreeWidgetItem *item, int)
{
	if (!item) return;
	QString url = item->data(0, Qt::UserRole).toString();
	if (!url.isEmpty()) {
		emit openUrl(url);
		accept();
	}
}

void HistoryDialog::onClearClicked()
{
	if (QMessageBox::question(this, tr("确认"),
							  tr("确定要清空所有历史记录吗？")) == QMessageBox::Yes) {
		BrowserHistory::instance().clear();
		refresh();
	}
}

void HistoryDialog::onDeleteClicked()
{
	auto items = m_tree->selectedItems();
	if (items.isEmpty()) return;
	for (auto *it : items) {
		QString url = it->data(0, Qt::UserRole).toString();
		if (!url.isEmpty()) BrowserHistory::instance().remove(url);
	}
	refresh();
}
