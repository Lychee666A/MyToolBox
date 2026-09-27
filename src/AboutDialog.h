#pragma once

#include <QDialog>
#include <QVector>

class QListWidget;
class QTextBrowser;
class QPushButton;
class QTimer;

class AboutDialog : public QDialog
{
	Q_OBJECT
public:
	explicit AboutDialog(QWidget *parent = nullptr);
	~AboutDialog() override;
	
protected:
	void keyPressEvent(QKeyEvent *e) override;
	void closeEvent(QCloseEvent *e) override;
	
private slots:
	void onCategoryChanged(int row);
	void onToggleAutoScroll();
	void onScrollTick();
	void onOpenLicense();
	void onOpenQtLicense();
	void onCopyVersion();
	void onThemeChanged(bool dark);
	
private:
	void buildUi();
	void addCategory(const QString &title, const QString &resourceName);
	
	QString loadHtml(const QString &resourceName) const;
	QString wrapWithStyle(const QString &html) const;
	
	void stopAutoScroll();
	void reloadAllPages();
	
	// ★ 新增：用 Helper 打开 URL
	bool openUrlInHelper(const QString &url);
	
	QListWidget  *m_categories = nullptr;
	QTextBrowser *m_viewer = nullptr;
	QPushButton  *m_btnAutoScroll = nullptr;
	QPushButton  *m_btnClose = nullptr;
	QPushButton  *m_btnCopyVer = nullptr;
	QTimer       *m_scrollTimer = nullptr;
	
	QVector<QString> m_htmlPages;
	QVector<QString> m_htmlPaths;
	
	int  m_scrollStep = 0;
	bool m_autoScrolling = false;
};
