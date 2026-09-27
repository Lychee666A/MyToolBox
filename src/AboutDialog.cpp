#include "AboutDialog.h"
#include "ThemeManager.h"
#include "AppInfo.h"

#include <QListWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollBar>
#include <QDateTime>
#include <QSysInfo>
#include <QFile>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// ============================================================
//                      构造 / 析构
// ============================================================
AboutDialog::AboutDialog(QWidget *parent)
: QDialog(parent) {
	setWindowTitle(tr("关于 %1").arg(AppInfo::DisplayName()));
	setMinimumSize(760, 560);
	resize(820, 600);
	
	buildUi();
	
	m_scrollTimer = new QTimer(this);
	m_scrollTimer->setInterval(30);
	connect(m_scrollTimer, &QTimer::timeout, this, &AboutDialog::onScrollTick);
	
	connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
			this, &AboutDialog::onThemeChanged);
}

AboutDialog::~AboutDialog() = default;

// ============================================================
//                      构建 UI
// ============================================================
void AboutDialog::buildUi() {
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(10, 10, 10, 10);
	
	// ---- 顶部：图标 + 程序名 + 版本 ----
	auto *header = new QHBoxLayout();
	
	auto *iconLabel = new QLabel(this);
	iconLabel->setFixedSize(80, 80);
	iconLabel->setAlignment(Qt::AlignCenter);
	iconLabel->setObjectName("aboutIcon");
	iconLabel->setStyleSheet(
							 "background:#ffffff; border:1px solid #dadce0; border-radius:8px;");
	
	QPixmap pm(":/icon.jpg");
	if (pm.isNull()) pm.load(":/icon.ico");
	if (!pm.isNull()) {
		iconLabel->setPixmap(pm.scaled(72, 72,
									   Qt::KeepAspectRatio,
									   Qt::SmoothTransformation));
	}
	header->addWidget(iconLabel);
	
	auto *titleBox = new QVBoxLayout();
	auto *title = new QLabel(
							 QString("<h2>%1</h2>").arg(AppInfo::DisplayName()), this);
	title->setTextFormat(Qt::RichText);
	
	auto *sub = new QLabel(
						   tr("版本 %1 &nbsp;|&nbsp; Qt %2 &nbsp;|&nbsp; %3")
						   .arg(AppInfo::Version(), QT_VERSION_STR, QSysInfo::prettyProductName()),
						   this);
	sub->setTextFormat(Qt::RichText);
	sub->setObjectName("aboutSub");
	
	titleBox->addWidget(title);
	titleBox->addWidget(sub);
	titleBox->addStretch();
	header->addLayout(titleBox, 1);
	
	m_btnCopyVer = new QPushButton(tr("复制版本信息"), this);
	connect(m_btnCopyVer, &QPushButton::clicked,
			this, &AboutDialog::onCopyVersion);
	header->addWidget(m_btnCopyVer);
	
	root->addLayout(header);
	
	// ---- 中部：左侧分类列表 + 右侧内容 ----
	auto *splitter = new QSplitter(Qt::Horizontal, this);
	
	m_categories = new QListWidget(splitter);
	m_categories->setFixedWidth(160);
	
	m_viewer = new QTextBrowser(splitter);
	// ★ 不自动打开链接，由 anchorClicked 拦截，交给 Helper
	m_viewer->setOpenExternalLinks(false);
	m_viewer->setOpenLinks(false);
	
	connect(m_viewer, &QTextBrowser::anchorClicked,
			this, [this](const QUrl &url) {
				if (url.isEmpty()) return;
				
				// 内部锚点（#xxx）交回给 QTextBrowser
				const QString s = url.toString();
				if ((url.scheme().isEmpty()) && s.startsWith('#')) {
					m_viewer->scrollToAnchor(s.mid(1));
					return;
				}
				
				qDebug() << "[About] anchorClicked:" << url.toString();
				openUrlInHelper(url.toString());
			});
	
	splitter->addWidget(m_categories);
	splitter->addWidget(m_viewer);
	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);
	
	root->addWidget(splitter, 1);
	
	// ---- 加入分类 ----
	addCategory(tr("关于"),        ":/about/about.html");
	addCategory(tr("许可证"),      ":/about/license.html");
	addCategory(tr("开发者名单"),  ":/about/authors.html");
	addCategory(tr("隐私政策"),    ":/about/privacy.html");
	addCategory(tr("捐赠者名单"),  ":/about/donors.html");
	addCategory(tr("关于 Qt"),     ":/about/qt.html");
	
	// ---- 底部按钮 ----
	auto *bottom = new QHBoxLayout();
	m_btnAutoScroll = new QPushButton(tr("自动滚动 (空格)"), this);
	m_btnAutoScroll->setCheckable(true);
	connect(m_btnAutoScroll, &QPushButton::clicked,
			this, &AboutDialog::onToggleAutoScroll);
	bottom->addWidget(m_btnAutoScroll);
	
	bottom->addStretch(1);
	
	auto *btnLicense = new QPushButton(tr("查看完整许可证…"), this);
	connect(btnLicense, &QPushButton::clicked,
			this, &AboutDialog::onOpenLicense);
	bottom->addWidget(btnLicense);
	
	auto *btnQt = new QPushButton(tr("Qt 开源许可…"), this);
	connect(btnQt, &QPushButton::clicked,
			this, &AboutDialog::onOpenQtLicense);
	bottom->addWidget(btnQt);
	
	m_btnClose = new QPushButton(tr("关闭"), this);
	m_btnClose->setDefault(true);
	connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
	bottom->addWidget(m_btnClose);
	
	root->addLayout(bottom);
	
	// ---- 信号连接 + 默认选中第一项 ----
	connect(m_categories, &QListWidget::currentRowChanged,
			this, &AboutDialog::onCategoryChanged);
	
	m_categories->setCurrentRow(0);
	onCategoryChanged(0);
}

void AboutDialog::addCategory(const QString &title, const QString &resourceName) {
	m_categories->addItem(title);
	m_htmlPaths.append(resourceName);
	m_htmlPages.append(loadHtml(resourceName));
}

QString AboutDialog::loadHtml(const QString &resourceName) const {
	QFile f(resourceName);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return wrapWithStyle(
							 QString("<p style='color:red'>无法加载资源：%1</p>")
							 .arg(resourceName));
	}
	
	QString html = QString::fromUtf8(f.readAll());
	f.close();
	
	html.replace("__APP_NAME__",    AppInfo::DisplayName());
	html.replace("__APP_VERSION__", AppInfo::Version());
	html.replace("__QT_VERSION__",  QT_VERSION_STR);
	html.replace("__OS__",          QSysInfo::prettyProductName());
	html.replace("__ARCH__",        QSysInfo::currentCpuArchitecture());
	html.replace("__BUILD_TIME__",  AppInfo::BuildDateTime());
	html.replace("__PUBLISHER__",   AppInfo::Publisher());
	html.replace("__COPYRIGHT__",   AppInfo::Copyright());
	html.replace("__LICENSE__",     AppInfo::License());
	html.replace("__HOMEPAGE__",    AppInfo::Homepage());
	html.replace("__EMAIL__",       AppInfo::Email());
	html.replace("__DESCRIPTION__", AppInfo::Description());
	
	return wrapWithStyle(html);
}

QString AboutDialog::wrapWithStyle(const QString &html) const {
	const bool dark = ThemeManager::instance().isDark();
	
	const char *kStyle = dark
	? R"(
<style>
body {
font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
font-size: 13px;
line-height: 1.7;
color: #e8eaed;
background: #2d2e31;
margin: 12px 16px;
}
h2   { color: #8ab4f8; margin-top: 4px; }
h3   { color: #aecbfa; margin-bottom: 4px; }
code {
background: #3c4043;
color: #e8eaed;
padding: 1px 4px;
border-radius: 3px;
}
ul   { margin-top: 2px; }
hr   { border: none; border-top: 1px solid #3c4043; }
a    { color: #8ab4f8; text-decoration: none; }
a:hover { text-decoration: underline; }
.muted { color: #9aa0a6; }
b    { color: #e8eaed; }
</style>)"
	: R"(
<style>
body {
font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
font-size: 13px;
line-height: 1.7;
color: #202124;
background: #ffffff;
margin: 12px 16px;
}
h2   { color: #1a73e8; margin-top: 4px; }
h3   { color: #1967d2; margin-bottom: 4px; }
code {
background: #f1f3f4;
color: #202124;
padding: 1px 4px;
border-radius: 3px;
}
ul   { margin-top: 2px; }
hr   { border: none; border-top: 1px solid #dadce0; }
a    { color: #1a73e8; text-decoration: none; }
a:hover { text-decoration: underline; }
.muted { color: #888; }
b    { color: #202124; }
</style>)";
	
	return QString(kStyle) + html;
}

// ============================================================
//                      分类切换
// ============================================================
void AboutDialog::onCategoryChanged(int row) {
	if (row < 0 || row >= m_htmlPages.size()) return;
	stopAutoScroll();
	m_viewer->setHtml(m_htmlPages[row]);
	m_viewer->verticalScrollBar()->setValue(0);
}

// ============================================================
//                  ★ 主题变化 / 重新加载
// ============================================================
void AboutDialog::onThemeChanged(bool /*dark*/) {
	reloadAllPages();
}

void AboutDialog::reloadAllPages() {
	const int cur = m_categories->currentRow();
	
	m_htmlPages.clear();
	for (const QString &path : m_htmlPaths) {
		m_htmlPages.append(loadHtml(path));
	}
	
	if (cur >= 0 && cur < m_htmlPages.size()) {
		const int scroll = m_viewer->verticalScrollBar()->value();
		m_viewer->setHtml(m_htmlPages[cur]);
		m_viewer->verticalScrollBar()->setValue(scroll);
	}
}

// ============================================================
//                      自动滚动
// ============================================================
void AboutDialog::onToggleAutoScroll() {
	if (m_autoScrolling) {
		stopAutoScroll();
		return;
	}
	
	auto *sb = m_viewer->verticalScrollBar();
	if (sb->value() >= sb->maximum()) sb->setValue(0);
	
	m_autoScrolling = true;
	m_scrollStep = 1;
	m_btnAutoScroll->setChecked(true);
	m_btnAutoScroll->setText(tr("停止滚动 (空格)"));
	m_scrollTimer->start();
}

void AboutDialog::onScrollTick() {
	auto *sb = m_viewer->verticalScrollBar();
	const int next = sb->value() + m_scrollStep;
	if (next >= sb->maximum()) {
		sb->setValue(sb->maximum());
		stopAutoScroll();
		return;
	}
	sb->setValue(next);
}

void AboutDialog::stopAutoScroll() {
	m_autoScrolling = false;
	m_scrollStep = 0;
	if (m_scrollTimer) m_scrollTimer->stop();
	if (m_btnAutoScroll) {
		m_btnAutoScroll->setChecked(false);
		m_btnAutoScroll->setText(tr("自动滚动 (空格)"));
	}
}

// ============================================================
//                      键盘 / 关闭
// ============================================================
void AboutDialog::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
		if (!m_autoScrolling) {
			onToggleAutoScroll();
		} else {
			if (m_scrollStep < 24) m_scrollStep *= 2;
			else m_scrollStep = 24;
		}
		e->accept();
		return;
	}
	if (e->key() == Qt::Key_Escape) {
		stopAutoScroll();
		reject();
		return;
	}
	QDialog::keyPressEvent(e);
}

void AboutDialog::closeEvent(QCloseEvent *e) {
	stopAutoScroll();
	QDialog::closeEvent(e);
}

// ============================================================
//  ★ 用 Helper 打开 URL（Kiosk 模式）
// ============================================================
bool AboutDialog::openUrlInHelper(const QString &url)
{
	if (url.isEmpty()) return false;
	
	const QUrl u(url);
	if (u.scheme() != "http" && u.scheme() != "https" && u.scheme() != "file") {
		QDesktopServices::openUrl(u);
		return false;
	}
	
	const QString appDir = QCoreApplication::applicationDirPath();
	QString helper = QDir(appDir).filePath("MyToolBoxBrowser.exe");
	
	qDebug() << "[About] Helper path:" << helper;
	qDebug() << "[About] Exists?" << QFileInfo::exists(helper);
	
	if (!QFileInfo::exists(helper)) {
		qWarning() << "[About] 找不到 MyToolBoxBrowser.exe，回退到系统浏览器";
		QDesktopServices::openUrl(u);
		return false;
	}
	
	QStringList args;
	args << "--kiosk"
	<< "--new-window"
	<< "--title" << QString("%1 - %2").arg(AppInfo::DisplayName(), windowTitle())
	<< url;
	
	qDebug() << "[About] Launching:" << helper << args;
	bool ok = QProcess::startDetached(helper, args);
	qDebug() << "[About] Launch result:" << ok;
	
	if (!ok) {
		qWarning() << "[About] 启动 Helper 失败，回退到系统浏览器";
		QDesktopServices::openUrl(u);
	}
	return ok;
}

// ============================================================
//                      外部链接
// ============================================================
void AboutDialog::onOpenLicense() {
	openUrlInHelper("https://www.gnu.org/licenses/gpl-3.0.html");
}

void AboutDialog::onOpenQtLicense() {
	openUrlInHelper("https://www.qt.io/licensing/open-source-lgpl-obligations");
}

void AboutDialog::onCopyVersion() {
	QApplication::clipboard()->setText(AppInfo::FullInfoText());
	m_btnCopyVer->setText(tr("已复制 ✓"));
	QTimer::singleShot(1500, this, [this]() {
		if (m_btnCopyVer) m_btnCopyVer->setText(tr("复制版本信息"));
	});
}
