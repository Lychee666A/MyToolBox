#include "QuickLinksWidget.h"
#include "SettingsManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QDesktopServices>
#include <QListWidgetItem>

QuickLinksWidget::QuickLinksWidget(QWidget *parent)
: QWidget(parent)
{
	buildUi();
	reloadLinks();
	
	connect(&SettingsManager::instance(), &SettingsManager::settingsChanged,
			this, &QuickLinksWidget::reloadLinks);
}

void QuickLinksWidget::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);
	
	m_list = new QListWidget(this);
	root->addWidget(m_list, 1);
	
	auto *row = new QHBoxLayout();
	row->setSpacing(6);
	m_titleEdit = new QLineEdit(this);
	m_titleEdit->setPlaceholderText(tr("名称"));
	m_urlEdit = new QLineEdit(this);
	m_urlEdit->setPlaceholderText(tr("URL, 例如 https://example.com"));
	m_btnAdd = new QPushButton(tr("添加"), this);
	row->addWidget(m_titleEdit);
	row->addWidget(m_urlEdit, 1);
	row->addWidget(m_btnAdd);
	root->addLayout(row);
	
	m_btnOpen = new QPushButton(tr("打开选中链接"), this);
	m_btnOpen->setProperty("accent", true);
	root->addWidget(m_btnOpen);
	
	connect(m_btnOpen, &QPushButton::clicked, this, &QuickLinksWidget::onOpenSelected);
	connect(m_btnAdd, &QPushButton::clicked, this, &QuickLinksWidget::onAddLink);
	connect(m_list, &QListWidget::itemDoubleClicked,
			this, [this](QListWidgetItem *) { onOpenSelected(); });
}

void QuickLinksWidget::reloadLinks()
{
	m_list->clear();
	for (const auto &p : SettingsManager::instance().quickLinks()) {
		addItem(p.first, QUrl(p.second));
	}
}

void QuickLinksWidget::addItem(const QString &title, const QUrl &url)
{
	auto *it = new QListWidgetItem(QString("%1  -  %2").arg(title, url.toString()), m_list);
	it->setData(Qt::UserRole, url);
}

// ★ 打开选中链接：改为发出信号，由 MainWindow 在内部浏览器打开
void QuickLinksWidget::onOpenSelected()
{
	auto *it = m_list->currentItem();
	if (!it) return;
	
	QUrl url = it->data(Qt::UserRole).toUrl();
	if (!url.isValid() || url.isEmpty()) return;
	
	// 补全 scheme：用户可能只存了 "www.baidu.com"
	if (url.scheme().isEmpty()) {
		QString s = url.toString();
		if (s.startsWith("//")) s.remove(0, 2);
		url = QUrl("https://" + s);
	}
	
	// ★ 交给 MainWindow 在内部浏览器打开
	emit openInBrowser(url);
}

void QuickLinksWidget::onAddLink()
{
	QString title = m_titleEdit->text().trimmed();
	QString urlStr = m_urlEdit->text().trimmed();
	if (urlStr.isEmpty()) return;
	
	// 容错：允许用户不写 https://
	if (!urlStr.contains("://"))
		urlStr = "https://" + urlStr;
	
	QUrl url(urlStr);
	if (!url.isValid()) return;
	if (title.isEmpty()) title = url.host();
	
	auto links = SettingsManager::instance().quickLinks();
	links.append(qMakePair(title, urlStr));
	SettingsManager::instance().setQuickLinks(links);
	
	m_titleEdit->clear();
	m_urlEdit->clear();
}
