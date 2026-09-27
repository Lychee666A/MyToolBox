#include "DownloadsDialog.h"
#include "DownloadsManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTreeWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

DownloadsDialog::DownloadsDialog(QWidget *parent)
: QDialog(parent)
{
	setWindowTitle(tr("下载记录"));
	resize(800, 500);
	
	auto *root = new QVBoxLayout(this);
	
	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(4);
	m_tree->setHeaderLabels({ tr("文件名"), tr("来源"), tr("状态"), tr("时间") });
	m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	m_tree->setRootIsDecorated(false);
	m_tree->setAlternatingRowColors(true);
	root->addWidget(m_tree, 1);
	
	auto *btnRow = new QHBoxLayout();
	m_btnClear = new QPushButton(tr("清空记录"), this);
	m_btnClose = new QPushButton(tr("关闭"), this);
	btnRow->addWidget(m_btnClear);
	btnRow->addStretch();
	btnRow->addWidget(m_btnClose);
	root->addLayout(btnRow);
	
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &DownloadsDialog::onItemActivated);
	connect(m_btnClear, &QPushButton::clicked, this, &DownloadsDialog::onClearClicked);
	connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
	
	refresh();
}

void DownloadsDialog::refresh()
{
	m_tree->clear();
	for (const auto &e : DownloadsManager::instance().all()) {
		auto *it = new QTreeWidgetItem(m_tree);
		it->setText(0, e.fileName.isEmpty() ? e.url.section('/', -1) : e.fileName);
		it->setText(1, e.url);
		it->setText(2, e.state);
		it->setText(3, e.startTime.toString("yyyy-MM-dd hh:mm"));
		it->setData(0, Qt::UserRole, e.filePath);
	}
}

void DownloadsDialog::onItemActivated(QTreeWidgetItem *item, int)
{
	if (!item) return;
	QString path = item->data(0, Qt::UserRole).toString();
	if (!path.isEmpty())
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void DownloadsDialog::onClearClicked()
{
	if (QMessageBox::question(this, tr("确认"),
							  tr("清空下载记录（不会删除已下载的文件）？")) == QMessageBox::Yes) {
		DownloadsManager::instance().clearRecords();
		refresh();
	}
}
