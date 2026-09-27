#include "FileSearchWidget.h"
#include "EverythingApi.h"

#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QProcess>
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QLocale>
#include <QTextStream>
#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <windows.h>
#include <shellapi.h>

// ============================================================
//                        构造 / 析构
// ============================================================
FileSearchWidget::FileSearchWidget(QWidget *parent)
: QWidget(parent)
{
	buildUi();
	
	m_debounceTimer = new QTimer(this);
	m_debounceTimer->setSingleShot(true);
	m_debounceTimer->setInterval(250);
	connect(m_debounceTimer, &QTimer::timeout, this, &FileSearchWidget::onSearch);
	
	// 异步搜索完成回调
	connect(&m_searchWatcher,
			&QFutureWatcher<QList<EverythingResult>>::finished,
			this, &FileSearchWidget::onSearchFinished);
	
	// DB 就绪检查定时器
	m_dbCheckTimer = new QTimer(this);
	m_dbCheckTimer->setInterval(500);
	connect(m_dbCheckTimer, &QTimer::timeout, this, &FileSearchWidget::checkDbReady);
	
	refreshStatus();
	
	// 加载搜索历史
	QSettings s;
	const QStringList hist = s.value("filesearch/history").toStringList();
	for (const auto &h : hist) {
		m_historyCombo->addItem(h.left(60), h);
	}
}

FileSearchWidget::~FileSearchWidget()
{
	if (m_debounceTimer) m_debounceTimer->stop();
	if (m_dbCheckTimer)  m_dbCheckTimer->stop();
	
	if (m_searchWatcher.isRunning()) {
		m_searchWatcher.cancel();
		m_searchWatcher.waitForFinished();
	}
}

// ============================================================
//                        UI
// ============================================================
void FileSearchWidget::buildUi()
{
	auto *root = new QVBoxLayout(this);
	
	// ---- 搜索行 ----
	auto *top = new QHBoxLayout();
	m_historyCombo = new QComboBox(this);
	m_historyCombo->setEditable(false);
	m_historyCombo->setMinimumWidth(160);
	m_historyCombo->addItem(tr("搜索历史…"));
	connect(m_historyCombo, QOverload<int>::of(&QComboBox::activated),
			this, [this](int) {
				const QString t = m_historyCombo->currentData().toString();
				if (!t.isEmpty()) { m_searchEdit->setText(t); onSearch(); }
			});
	
	m_searchEdit = new QLineEdit(this);
	m_searchEdit->setPlaceholderText(
									 tr("Everything 语法，如 *.cpp  ext:exe;ini  size:>100mb  dm:today"));
	m_searchEdit->setClearButtonEnabled(true);
	m_btnSearch = new QPushButton(tr("搜索"), this);
	
	top->addWidget(m_historyCombo);
	top->addWidget(m_searchEdit, 1);
	top->addWidget(m_btnSearch);
	root->addLayout(top);
	
	// ---- 选项行 ----
	auto *optBox = new QGroupBox(tr("搜索选项"), this);
	auto *optLay = new QHBoxLayout(optBox);
	
	m_chkMatchCase = new QCheckBox(tr("区分大小写"), optBox);
	m_chkWholeWord = new QCheckBox(tr("全字匹配"),   optBox);
	m_chkMatchPath = new QCheckBox(tr("匹配路径"),   optBox);
	m_chkRegex     = new QCheckBox(tr("正则表达式"), optBox);
	
	m_typeCombo = new QComboBox(optBox);
	m_typeCombo->addItem(tr("全部"),    0);
	m_typeCombo->addItem(tr("仅文件"),  1);
	m_typeCombo->addItem(tr("仅文件夹"), 2);
	
	m_sortCombo = new QComboBox(optBox);
	m_sortCombo->addItem(tr("按名称"),   int(EverythingSortBy::Name));
	m_sortCombo->addItem(tr("按路径"),   int(EverythingSortBy::Path));
	m_sortCombo->addItem(tr("按大小"),   int(EverythingSortBy::Size));
	m_sortCombo->addItem(tr("按扩展名"), int(EverythingSortBy::Extension));
	m_sortCombo->addItem(tr("按修改时间"), int(EverythingSortBy::DateModified));
	m_sortCombo->addItem(tr("按创建时间"), int(EverythingSortBy::DateCreated));
	
	optLay->addWidget(m_chkMatchCase);
	optLay->addWidget(m_chkWholeWord);
	optLay->addWidget(m_chkMatchPath);
	optLay->addWidget(m_chkRegex);
	optLay->addSpacing(12);
	optLay->addWidget(new QLabel(tr("类型："), optBox));
	optLay->addWidget(m_typeCombo);
	optLay->addWidget(new QLabel(tr("排序："), optBox));
	optLay->addWidget(m_sortCombo);
	optLay->addStretch();
	root->addWidget(optBox);
	
	// ---- 结果表（8 列） ----
	m_table = new QTableWidget(this);
	m_table->setColumnCount(8);
	m_table->setHorizontalHeaderLabels({
		tr("名称"), tr("路径"), tr("扩展名"), tr("大小"),
		tr("修改时间"), tr("创建时间"), tr("访问时间"), tr("属性")
	});
	m_table->horizontalHeader()->setSectionsClickable(true);
	m_table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	m_table->horizontalHeader()->setStretchLastSection(false);
	m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(ColPath, QHeaderView::Stretch);
	
	m_table->verticalHeader()->setVisible(false);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setAlternatingRowColors(true);
	m_table->setContextMenuPolicy(Qt::CustomContextMenu);
	m_table->setSortingEnabled(false);
	m_table->setWordWrap(false);
	root->addWidget(m_table, 1);
	
	// ---- 底部 ----
	auto *bottom = new QHBoxLayout();
	m_statusLabel = new QLabel(tr("就绪"), this);
	m_statsLabel  = new QLabel(tr(""), this);
	m_btnLoadMore = new QPushButton(tr("加载更多"), this);
	m_btnExportCsv = new QPushButton(tr("导出 CSV"), this);
	m_btnExportTxt = new QPushButton(tr("导出 TXT"), this);
	m_btnOpen     = new QPushButton(tr("打开"), this);
	m_btnOpenDir  = new QPushButton(tr("打开所在文件夹"), this);
	bottom->addWidget(m_statusLabel, 1);
	bottom->addWidget(m_statsLabel);
	bottom->addWidget(m_btnLoadMore);
	bottom->addWidget(m_btnExportCsv);
	bottom->addWidget(m_btnExportTxt);
	bottom->addWidget(m_btnOpen);
	bottom->addWidget(m_btnOpenDir);
	root->addLayout(bottom);
	
	// ---- 信号 ----
	connect(m_searchEdit, &QLineEdit::returnPressed, this, &FileSearchWidget::onSearch);
	connect(m_searchEdit, &QLineEdit::textChanged, this, &FileSearchWidget::onSearchTextChanged);
	connect(m_btnSearch, &QPushButton::clicked, this, &FileSearchWidget::onSearch);
	
	connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked,
			this, &FileSearchWidget::onHeaderClicked);
	connect(m_table->horizontalHeader(), &QHeaderView::customContextMenuRequested,
			this, &FileSearchWidget::onHeaderContextMenu);
	
	connect(m_table, &QTableWidget::cellDoubleClicked, this, &FileSearchWidget::onResultDoubleClicked);
	connect(m_table, &QTableWidget::customContextMenuRequested, this, &FileSearchWidget::onResultContextMenu);
	
	connect(m_btnOpen,     &QPushButton::clicked, this, &FileSearchWidget::onOpenFile);
	connect(m_btnOpenDir,  &QPushButton::clicked, this, &FileSearchWidget::onOpenFolder);
	connect(m_btnLoadMore, &QPushButton::clicked, this, &FileSearchWidget::onLoadMore);
	connect(m_btnExportCsv,&QPushButton::clicked, this, &FileSearchWidget::onExportCsv);
	connect(m_btnExportTxt,&QPushButton::clicked, this, &FileSearchWidget::onExportTxt);
	
	loadColumnVisibility();
}

void FileSearchWidget::refreshStatus()
{
	auto &api = EverythingApi::instance();
	if (!api.isAvailable()) {
		m_statusLabel->setText(tr("Everything64.dll 未加载：%1").arg(api.lastError()));
		setSearchEnabled(false);
	} else if (!api.isEverythingRunning()) {
		m_statusLabel->setText(tr("Everything.exe 未运行，请检查启动流程"));
		setSearchEnabled(false);
	} else if (!api.isDbLoaded()) {
		m_statusLabel->setText(tr("Everything 正在建立索引…（此期间搜索不可用）"));
		setSearchEnabled(false);
		if (m_dbCheckTimer && !m_dbCheckTimer->isActive())
			m_dbCheckTimer->start();
	} else {
		m_statusLabel->setText(tr("就绪（Everything 服务端已连接）"));
		setSearchEnabled(true);
		if (m_dbCheckTimer && m_dbCheckTimer->isActive())
			m_dbCheckTimer->stop();
	}
}

void FileSearchWidget::setSearchEnabled(bool enabled)
{
	m_searchEdit->setEnabled(enabled);
	m_btnSearch->setEnabled(enabled);
}

void FileSearchWidget::checkDbReady()
{
	auto &api = EverythingApi::instance();
	if (!api.isAvailable() || !api.isEverythingRunning()) {
		m_dbCheckTimer->stop();
		refreshStatus();
		return;
	}
	if (api.isDbLoaded()) {
		m_dbCheckTimer->stop();
		m_statusLabel->setText(tr("就绪（Everything 服务端已连接）"));
		setSearchEnabled(true);
	}
}

void FileSearchWidget::onHeaderClicked(int column)
{
	if (m_sortColumn == column) {
		m_sortAscending = !m_sortAscending;
	} else {
		m_sortColumn = column;
		m_sortAscending = true;
	}
	
	EverythingSortBy by = EverythingSortBy::Name;
	switch (column) {
		case ColName:          by = EverythingSortBy::Name; break;
		case ColPath:          by = EverythingSortBy::Path; break;
		case ColExt:           by = EverythingSortBy::Extension; break;
		case ColSize:          by = EverythingSortBy::Size; break;
		case ColDateModified:  by = EverythingSortBy::DateModified; break;
		case ColDateCreated:   by = EverythingSortBy::DateCreated; break;
		case ColDateAccessed:  by = EverythingSortBy::DateAccessed; break;
		case ColAttributes:    by = EverythingSortBy::Attributes; break;
	}
	m_sortCombo->setCurrentIndex(int(by));
	onSearch();
}

void FileSearchWidget::onHeaderContextMenu(const QPoint &pos)
{
	QMenu menu(this);
	const char *names[] = { "名称", "路径", "扩展名", "大小",
		"修改时间", "创建时间", "访问时间", "属性" };
	for (int i = 0; i < 8; ++i) {
		QAction *a = menu.addAction(tr(names[i]));
		a->setCheckable(true);
		a->setChecked(!m_table->isColumnHidden(i));
		connect(a, &QAction::toggled, this, [this, i](bool on) {
			m_table->setColumnHidden(i, !on);
			saveColumnVisibility();
		});
	}
	menu.exec(m_table->horizontalHeader()->mapToGlobal(pos));
}

void FileSearchWidget::saveColumnVisibility()
{
	QSettings s;
	s.beginWriteArray("filesearch/columns");
	for (int i = 0; i < 8; ++i) {
		s.setArrayIndex(i);
		s.setValue("hidden", m_table->isColumnHidden(i));
	}
	s.endArray();
}

void FileSearchWidget::loadColumnVisibility()
{
	QSettings s;
	int n = s.beginReadArray("filesearch/columns");
	for (int i = 0; i < n && i < 8; ++i) {
		s.setArrayIndex(i);
		m_table->setColumnHidden(i, s.value("hidden", false).toBool());
	}
	s.endArray();
}

// ============================================================
//                        搜索
// ============================================================
void FileSearchWidget::onSearchTextChanged()
{
	m_debounceTimer->start();
}

void FileSearchWidget::onSearch()
{
	auto &api = EverythingApi::instance();
	if (!api.isAvailable()) { refreshStatus(); return; }
	
	if (!api.isEverythingRunning() || !api.isDbLoaded()) {
		refreshStatus();
		return;
	}
	
	const QString q = m_searchEdit->text().trimmed();
	if (q.isEmpty()) {
		m_table->setRowCount(0);
		m_statusLabel->setText(tr("请输入关键字"));
		return;
	}
	
	// 上一次还在跑 → 丢弃
	if (m_searchWatcher.isRunning()) {
		qDebug() << "[FileSearchWidget] previous search still running, skipping";
		return;
	}
	
	EverythingQueryOptions opt;
	opt.matchCase      = m_chkMatchCase->isChecked();
	opt.matchWholeWord = m_chkWholeWord->isChecked();
	opt.matchMatchPath = m_chkMatchPath->isChecked();
	opt.regex          = m_chkRegex->isChecked();
	opt.maxResults     = m_pageSize;
	opt.offset         = 0;
	opt.sortBy         = EverythingSortBy(m_sortCombo->currentData().toInt());
	opt.sortAscending  = m_sortAscending;
	
	switch (m_typeCombo->currentData().toInt()) {
		case 1: opt.filesOnly   = true; break;
		case 2: opt.foldersOnly = true; break;
		default: break;
	}
	
	m_lastQuery = q;
	m_lastOptions = opt;
	m_currentOffset = 0;
	
	// 记录历史
	{
		QSettings s;
		QStringList hist = s.value("filesearch/history").toStringList();
		hist.removeAll(q);
		hist.prepend(q);
		while (hist.size() > 30) hist.removeLast();
		s.setValue("filesearch/history", hist);
		m_historyCombo->clear();
		m_historyCombo->addItem(tr("搜索历史…"));
		for (const auto &h : hist) m_historyCombo->addItem(h.left(60), h);
	}
	
	m_statusLabel->setText(tr("搜索中…"));
	m_btnSearch->setEnabled(false);
	
	if (!m_cursorActive) {
		QApplication::setOverrideCursor(Qt::WaitCursor);
		m_cursorActive = true;
	}
	
	QFuture<QList<EverythingResult>> future = QtConcurrent::run([q, opt]() {
		return EverythingApi::instance().search(q, opt, nullptr);
	});
	
	m_searchWatcher.setProperty("append", false);
	m_searchWatcher.setFuture(future);
}

void FileSearchWidget::onSearchFinished()
{
	if (m_cursorActive) {
		QApplication::restoreOverrideCursor();
		m_cursorActive = false;
	}
	m_btnSearch->setEnabled(true);
	
	auto &api = EverythingApi::instance();
	if (!api.lastError().isEmpty()) {
		m_statusLabel->setText(api.lastError());
		return;
	}
	
	const auto results = m_searchWatcher.result();
	const bool append = m_searchWatcher.property("append").toBool();
	
	fillTable(results, append);
	
	// ★ 从 api 拿统计
	m_lastStats = api.lastStats();
	
	m_statusLabel->setText(tr("找到 %1 个结果").arg(results.size()));
	m_statsLabel->setText(tr("显示 %1 / 共 %2 项，文件 %3，文件夹 %4")
						  .arg(append ? m_table->rowCount() : results.size())
						  .arg(m_lastStats.totalResults)
						  .arg(m_lastStats.totalFiles)
						  .arg(m_lastStats.totalFolders));
	
	m_btnLoadMore->setEnabled(int(results.size()) >= m_pageSize);
	if (append) m_currentOffset += results.size();
}

void FileSearchWidget::onLoadMore()
{
	if (m_lastQuery.isEmpty()) return;
	if (m_searchWatcher.isRunning()) return;
	
	EverythingQueryOptions opt = m_lastOptions;
	opt.offset = m_currentOffset;
	
	m_statusLabel->setText(tr("加载中…"));
	
	QFuture<QList<EverythingResult>> future = QtConcurrent::run([q = m_lastQuery, opt]() {
		return EverythingApi::instance().search(q, opt, nullptr);
	});
	m_searchWatcher.setProperty("append", true);
	m_searchWatcher.setFuture(future);
}

void FileSearchWidget::fillTable(const QList<EverythingResult> &results, bool append)
{
	m_table->setUpdatesEnabled(false);
	if (!append) m_table->setRowCount(0);
	const int startRow = m_table->rowCount();
	m_table->setRowCount(startRow + results.size());
	
	static const QIcon dirIcon  = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
	static const QIcon fileIcon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
	
	auto attrToString = [](quint32 a) -> QString {
		QStringList parts;
		if (a & FILE_ATTRIBUTE_READONLY)   parts << "R";
		if (a & FILE_ATTRIBUTE_HIDDEN)     parts << "H";
		if (a & FILE_ATTRIBUTE_SYSTEM)     parts << "S";
		if (a & FILE_ATTRIBUTE_ARCHIVE)    parts << "A";
		if (a & FILE_ATTRIBUTE_DIRECTORY)  parts << "D";
		if (a & FILE_ATTRIBUTE_COMPRESSED) parts << "C";
		if (a & FILE_ATTRIBUTE_ENCRYPTED)  parts << "E";
		return parts.join("");
	};
	
	for (int i = 0; i < results.size(); ++i) {
		const auto &r = results.at(i);
		const int row = startRow + i;
		
		const int slash = qMax(r.fullPath.lastIndexOf('/'), r.fullPath.lastIndexOf('\\'));
		const QString name = (slash >= 0) ? r.fullPath.mid(slash + 1) : r.fullPath;
		
		auto *itName = new QTableWidgetItem(name);
		itName->setIcon(r.isFolder ? dirIcon : fileIcon);
		auto *itPath = new QTableWidgetItem(r.fullPath);
		auto *itExt  = new QTableWidgetItem(r.extension);
		auto *itSize = new QTableWidgetItem(
											r.isFolder ? QString() : QString::number(r.size) + " B");
		auto *itDm   = new QTableWidgetItem(r.modified.isValid() ? r.modified.toString("yyyy-MM-dd hh:mm:ss") : "");
		auto *itDc   = new QTableWidgetItem(r.dateCreated.isValid() ? r.dateCreated.toString("yyyy-MM-dd hh:mm:ss") : "");
		auto *itDa   = new QTableWidgetItem(r.dateAccessed.isValid() ? r.dateAccessed.toString("yyyy-MM-dd hh:mm:ss") : "");
		auto *itAttr = new QTableWidgetItem(attrToString(r.attributes));
		
		itSize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
		itPath->setData(Qt::UserRole, r.fullPath);
		
		m_table->setItem(row, ColName, itName);
		m_table->setItem(row, ColPath, itPath);
		m_table->setItem(row, ColExt, itExt);
		m_table->setItem(row, ColSize, itSize);
		m_table->setItem(row, ColDateModified, itDm);
		m_table->setItem(row, ColDateCreated, itDc);
		m_table->setItem(row, ColDateAccessed, itDa);
		m_table->setItem(row, ColAttributes, itAttr);
	}
	
	m_table->setUpdatesEnabled(true);
}

// ============================================================
//                        结果操作
// ============================================================
void FileSearchWidget::onResultDoubleClicked(int row, int)
{
	auto *it = m_table->item(row, ColPath);
	if (!it) return;
	QDesktopServices::openUrl(QUrl::fromLocalFile(it->data(Qt::UserRole).toString()));
}

void FileSearchWidget::onOpenFile()
{
	const int row = m_table->currentRow();
	if (row < 0) return;
	onResultDoubleClicked(row, 0);
}

void FileSearchWidget::onOpenFolder()
{
	const int row = m_table->currentRow();
	if (row < 0) return;
	auto *it = m_table->item(row, ColPath);
	if (!it) return;
	
	const QString path = it->data(Qt::UserRole).toString();
	QFileInfo fi(path);
	const QString dir = fi.isDir() ? path : fi.absolutePath();
	QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void FileSearchWidget::onCopyPath()
{
	const int row = m_table->currentRow();
	if (row < 0) return;
	auto *it = m_table->item(row, ColPath);
	if (!it) return;
	QApplication::clipboard()->setText(it->data(Qt::UserRole).toString());
}

void FileSearchWidget::onLocateInExplorer()
{
	const int row = m_table->currentRow();
	if (row < 0) return;
	auto *it = m_table->item(row, ColPath);
	if (!it) return;
	
	const QString path = it->data(Qt::UserRole).toString();
	QProcess::startDetached("explorer.exe",
							{ "/select,", QDir::toNativeSeparators(path) });
}

void FileSearchWidget::onCopyFile()
{
	onCopyPath();
}

void FileSearchWidget::onCutFile()
{
	onCopyPath();
}

void FileSearchWidget::onDeleteFile()
{
	const int row = m_table->currentRow();
	if (row < 0) return;
	auto *it = m_table->item(row, ColPath);
	if (!it) return;
	
	if (QMessageBox::question(this, tr("删除"),
							  tr("确定要删除 %1 吗？（移到回收站）").arg(it->text()))
							  != QMessageBox::Yes) return;
	
	std::wstring path = QDir::toNativeSeparators(it->text()).toStdWString();
	path.push_back(L'\0');
	path.push_back(L'\0');
	
	SHFILEOPSTRUCTW op = {};
	op.wFunc  = FO_DELETE;
	op.pFrom  = path.c_str();
	op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
	
	if (SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted) {
		m_table->removeRow(row);
		m_statusLabel->setText(tr("已删除"));
	} else {
		QMessageBox::warning(this, tr("删除失败"), tr("无法删除该文件"));
	}
}

void FileSearchWidget::onRenameFile()
{
	const int row = m_table->currentRow();
	if (row < 0) return;
	auto *it = m_table->item(row, ColPath);
	if (!it) return;
	
	bool ok = false;
	const QString newName = QInputDialog::getText(
												  this, tr("重命名"), tr("新名称："), QLineEdit::Normal,
												  QFileInfo(it->text()).fileName(), &ok);
	if (!ok || newName.isEmpty()) return;
	
	QFileInfo fi(it->text());
	const QString newPath = fi.absolutePath() + "/" + newName;
	
	if (QFile::rename(it->text(), newPath)) {
		it->setText(newPath);
		m_statusLabel->setText(tr("已重命名"));
	} else {
		QMessageBox::warning(this, tr("重命名失败"), tr("无法重命名"));
	}
}

void FileSearchWidget::onShowProperties()
{
	const int row = m_table->currentRow();
	if (row < 0) return;
	auto *it = m_table->item(row, ColPath);
	if (!it) return;
	
	SHELLEXECUTEINFOW sei = { sizeof(sei) };
	sei.fMask  = SEE_MASK_INVOKEIDLIST;
	sei.lpVerb = L"properties";
	std::wstring p = it->text().toStdWString();
	sei.lpFile = p.c_str();
	sei.nShow  = SW_SHOW;
	ShellExecuteExW(&sei);
}

void FileSearchWidget::onExportCsv()
{
	const QString path = QFileDialog::getSaveFileName(
													  this, tr("导出 CSV"), "search.csv", "CSV (*.csv)");
	if (path.isEmpty()) return;
	
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
	QTextStream ts(&f);
	ts.setEncoding(QStringConverter::Utf8);
	
	ts << "名称,路径,扩展名,大小,修改时间,创建时间,访问时间\n";
	for (int i = 0; i < m_table->rowCount(); ++i) {
		auto *name = m_table->item(i, ColName);
		auto *path = m_table->item(i, ColPath);
		auto *ext  = m_table->item(i, ColExt);
		auto *size = m_table->item(i, ColSize);
		auto *dm   = m_table->item(i, ColDateModified);
		auto *dc   = m_table->item(i, ColDateCreated);
		auto *da   = m_table->item(i, ColDateAccessed);
		if (!name) continue;
		ts << '"' << name->text() << "\","
		<< '"' << path->text() << "\","
		<< '"' << ext->text()  << "\","
		<< '"' << size->text() << "\","
		<< '"' << dm->text()   << "\","
		<< '"' << dc->text()   << "\","
		<< '"' << da->text()   << "\"\n";
	}
	f.close();
	m_statusLabel->setText(tr("已导出到 %1").arg(path));
}

void FileSearchWidget::onExportTxt()
{
	const QString path = QFileDialog::getSaveFileName(
													  this, tr("导出 TXT"), "search.txt", "TXT (*.txt)");
	if (path.isEmpty()) return;
	
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
	QTextStream ts(&f);
	ts.setEncoding(QStringConverter::Utf8);
	
	for (int i = 0; i < m_table->rowCount(); ++i) {
		auto *it = m_table->item(i, ColPath);
		if (it) ts << it->text() << "\n";
	}
	f.close();
	m_statusLabel->setText(tr("已导出到 %1").arg(path));
}

void FileSearchWidget::onResultContextMenu(const QPoint &pos)
{
	const int row = m_table->rowAt(pos.y());
	if (row < 0) return;
	m_table->selectRow(row);
	
	QMenu menu(this);
	QAction *aOpen   = menu.addAction(tr("打开"));
	QAction *aFolder = menu.addAction(tr("打开所在文件夹"));
	menu.addSeparator();
	QAction *aCopy   = menu.addAction(tr("复制完整路径"));
	QAction *aCopyF  = menu.addAction(tr("复制文件"));
	QAction *aCut    = menu.addAction(tr("剪切文件"));
	QAction *aRename = menu.addAction(tr("重命名…"));
	QAction *aDel    = menu.addAction(tr("删除（移到回收站）"));
	menu.addSeparator();
	QAction *aLocate = menu.addAction(tr("在资源管理器中定位"));
	QAction *aProp   = menu.addAction(tr("属性"));
	
	QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
	if      (chosen == aOpen)   onOpenFile();
	else if (chosen == aFolder) onOpenFolder();
	else if (chosen == aCopy)   onCopyPath();
	else if (chosen == aCopyF)  onCopyFile();
	else if (chosen == aCut)    onCutFile();
	else if (chosen == aRename) onRenameFile();
	else if (chosen == aDel)    onDeleteFile();
	else if (chosen == aLocate) onLocateInExplorer();
	else if (chosen == aProp)   onShowProperties();
}
