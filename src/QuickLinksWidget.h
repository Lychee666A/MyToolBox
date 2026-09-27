#pragma once

#include <QWidget>
#include <QUrl>

class QListWidget;
class QPushButton;
class QLineEdit;

class QuickLinksWidget : public QWidget
{
	Q_OBJECT
public:
	explicit QuickLinksWidget(QWidget *parent = nullptr);
	
	signals:
	// ★ 请求在主程序内置浏览器里打开 URL
	void openInBrowser(const QUrl &url);
	
private slots:
	void onOpenSelected();
	void onAddLink();
	void reloadLinks();
	
private:
	void buildUi();
	void addItem(const QString &title, const QUrl &url);
	
	QListWidget *m_list = nullptr;
	QLineEdit *m_titleEdit = nullptr;
	QLineEdit *m_urlEdit = nullptr;
	QPushButton *m_btnOpen = nullptr;
	QPushButton *m_btnAdd = nullptr;
};
