#include "ArchivePreviewWidget.h"
#include "SettingsManager.h"

#include <bit7z/bit7z.hpp>

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QPixmap>
#include <QDebug>
#include <QMap>
#include <QSet>
#include <QStyle>
#include <QFileIconProvider>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QMenu>
#include <QDir>
#include <QMessageBox>
#include <QApplication>

// ====== 手写路径拆分，兼容 / 和 \ ======
namespace {
	
	QStringList splitArchivePath(const QString &path) {
		QStringList parts;
		QString cur;
		for (QChar c : path) {
			if (c == '/' || c == '\\') {
				if (!cur.isEmpty()) {
					parts.append(cur);
					cur.clear();
				}
			} else {
				cur.append(c);
			}
		}
		if (!cur.isEmpty()) parts.append(cur);
		return parts;
	}
	
	// 文件夹优先，然后按名称升序
	class ArchiveTreeItem : public QTreeWidgetItem {
	public:
		using QTreeWidgetItem::QTreeWidgetItem;
		
		bool operator<(const QTreeWidgetItem &other) const override {
			const int col = treeWidget() ? treeWidget()->sortColumn() : 0;
			if (col != 0) return QTreeWidgetItem::operator<(other);
			
			const bool thisDir  = data(0, Qt::UserRole + 2).toBool();
			const bool otherDir = other.data(0, Qt::UserRole + 2).toBool();
			if (thisDir != otherDir) return thisDir;
			
			return text(0).compare(other.text(0), Qt::CaseInsensitive) < 0;
		}
	};
	
} // namespace

void ArchivePreviewWidget::openArchiveDialog()
{
	onOpenArchive();
}

ArchivePreviewWidget::ArchivePreviewWidget(QWidget *parent)
: QWidget(parent)
{
	buildUi();
	
	try {
		m_lib = new bit7z::Bit7zLibrary("7z.dll");
	} catch (const bit7z::BitException &ex) {
		m_infoLabel->setText(tr("加载 7z.dll 失败: %1").arg(ex.what()));
		m_lib = nullptr;
	}
	
	// ★ 后台读取完成
	connect(&m_loadWatcher,
			&QFutureWatcher<QList<ArchiveEntry>>::finished,
			this, [this]() {
				const auto entries = m_loadWatcher.result();
				onArchiveLoaded(entries);
			});
	
	// ★ 后台提取完成
	connect(&m_extractWatcher,
			&QFutureWatcher<QByteArray>::finished,
			this, [this]() {
				const QByteArray data = m_extractWatcher.result();
				if (data.isEmpty()) {
					m_preview->setPlainText(tr("提取失败或文件为空"));
					return;
				}
				const QString fullPath = m_extractWatcher.property("path").toString();
				onEntryExtracted(data, fullPath);
			});
}

ArchivePreviewWidget::~ArchivePreviewWidget()
{
	// 等后台任务结束
	if (m_loadWatcher.isRunning())    m_loadWatcher.waitForFinished();
	if (m_extractWatcher.isRunning()) m_extractWatcher.waitForFinished();
	delete m_lib;
}

void ArchivePreviewWidget::buildUi()
{
	auto *root = new QVBoxLayout(this);
	
	auto *top = new QHBoxLayout();
	m_btnOpen = new QPushButton(tr("打开压缩包"), this);
	m_infoLabel = new QLabel(tr("支持 7z / zip / tar 等格式"), this);
	top->addWidget(m_btnOpen);
	top->addWidget(m_infoLabel, 1);
	root->addLayout(top);
	
	auto *mid = new QHBoxLayout();
	
	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(3);
	m_tree->setHeaderLabels({ tr("名称"), tr("大小"), tr("类型") });
	m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	
	m_tree->setRootIsDecorated(true);
	m_tree->setItemsExpandable(true);
	m_tree->setExpandsOnDoubleClick(true);
	m_tree->setAlternatingRowColors(true);
	m_tree->setUniformRowHeights(true);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_tree->setIndentation(20);
	m_tree->setAnimated(true);
	m_tree->setMouseTracking(true);
	m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	
	m_preview = new QTextEdit(this);
	m_preview->setReadOnly(true);
	
	mid->addWidget(m_tree, 1);
	mid->addWidget(m_preview, 2);
	root->addLayout(mid, 1);
	
	connect(m_btnOpen, &QPushButton::clicked,
			this, &ArchivePreviewWidget::onOpenArchive);
	connect(m_tree, &QTreeWidget::itemDoubleClicked,
			this, &ArchivePreviewWidget::onItemDoubleClicked);
	connect(m_tree, &QTreeWidget::customContextMenuRequested,
			this, &ArchivePreviewWidget::onTreeContextMenu);
}

void ArchivePreviewWidget::onOpenArchive()
{
	QString path = QFileDialog::getOpenFileName(
												this, tr("选择压缩包"), QString(),
												tr("压缩包 (*.7z *.zip *.tar *.gz *.bz2 *.xz);;所有文件 (*.*)"));
	if (path.isEmpty()) return;
	loadArchive(path);
}

// ============================================================
//                 异步：加载压缩包文件列表
// ============================================================
void ArchivePreviewWidget::loadArchive(const QString &path)
{
	m_currentArchive = path;
	m_tree->clear();
	m_preview->clear();
	m_tree->setSortingEnabled(false);
	
	if (!m_lib) {
		m_infoLabel->setText(tr("7z.dll 未加载"));
		return;
	}
	
	if (m_loadWatcher.isRunning()) m_loadWatcher.waitForFinished();
	
	m_infoLabel->setText(tr("正在读取压缩包…"));
	
	const QString lower = path.toLower();
	const bool isZip = lower.endsWith(".zip");
	const std::string archivePath = path.toStdString();
	bit7z::Bit7zLibrary *lib = m_lib;
	
	// ★ 在工作线程里读取列表
	QFuture<QList<ArchiveEntry>> future = QtConcurrent::run(
															[lib, archivePath, isZip]() -> QList<ArchiveEntry> {
																
																QList<ArchiveEntry> entries;
																try {
																	const bit7z::BitInFormat &format =
																	isZip ? bit7z::BitFormat::Zip : bit7z::BitFormat::SevenZip;
																	
																	bit7z::BitArchiveReader reader(*lib, archivePath, format);
																	
																	uint32_t idx = 0;
																	for (const auto &item : reader.items()) {
																		ArchiveEntry e;
																		e.fullPath = QString::fromUtf8(item.path().c_str());
																		e.size     = item.size();
																		e.isDir    = item.isDir();
																		e.index    = idx++;
																		if (!e.fullPath.isEmpty()) {
																			entries.append(e);
																		}
																	}
																} catch (const bit7z::BitException &) {
																	// 出错返回空列表
																}
																return entries;
															});
	
	m_loadWatcher.setFuture(future);
}

// ============================================================
//            后台读取完成：填充树（在 UI 线程执行）
// ============================================================
void ArchivePreviewWidget::onArchiveLoaded(const QList<ArchiveEntry> &entries)
{
	if (entries.isEmpty()) {
		m_infoLabel->setText(tr("无法打开压缩包或压缩包为空"));
		return;
	}
	
	QFileIconProvider iconProvider;
	QMap<QString, QTreeWidgetItem *> dirMap;
	int actualFileCount = 0;
	
	// 超大压缩包保护：超过 50000 项截断，避免 UI 卡死
	const int maxItems = 50000;
	int processed = 0;
	bool truncated = false;
	
	m_tree->setUpdatesEnabled(false);
	
	for (const auto &e : entries) {
		if (++processed > maxItems) { truncated = true; break; }
		
		QStringList parts = splitArchivePath(e.fullPath);
		if (parts.isEmpty()) continue;
		
		QString curPath;
		QTreeWidgetItem *parentItem = nullptr;
		
		for (int i = 0; i < parts.size(); ++i) {
			const QString &part = parts.at(i);
			const bool isLast = (i == parts.size() - 1);
			curPath += (curPath.isEmpty() ? "" : "/") + part;
			
			if (dirMap.contains(curPath)) {
				parentItem = dirMap.value(curPath);
				continue;
			}
			
			auto *node = new ArchiveTreeItem();
			node->setText(0, part);
			node->setData(0, Qt::UserRole, curPath);
			
			const bool nodeIsDir = !isLast || e.isDir;
			
			if (nodeIsDir) {
				node->setIcon(0, iconProvider.icon(QFileIconProvider::Folder));
				node->setText(2, tr("文件夹"));
				node->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
			} else {
				node->setIcon(0, iconProvider.icon(QFileIconProvider::File));
				node->setText(1, formatSize(e.size));
				node->setText(2, typeFromName(part));
				node->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
				node->setData(0, Qt::UserRole + 1, e.index);
			}
			
			node->setData(0, Qt::UserRole + 2, nodeIsDir);
			node->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
			
			if (parentItem)
				parentItem->addChild(node);
			else
				m_tree->addTopLevelItem(node);
			
			dirMap.insert(curPath, node);
			parentItem = node;
		}
		
		if (!e.isDir) ++actualFileCount;
	}
	
	m_tree->setUpdatesEnabled(true);
	
	if (SettingsManager::instance().archiveExpandAllOnOpen())
		m_tree->expandAll();
	else
		m_tree->expandToDepth(0);
	
	m_tree->setSortingEnabled(true);
	m_tree->sortByColumn(0, Qt::AscendingOrder);
	
	QString info = tr("%1  （%2 个文件）").arg(m_currentArchive).arg(actualFileCount);
	if (truncated) info += tr("  [仅显示前 %1 项]").arg(maxItems);
	m_infoLabel->setText(info);
}

// ============================================================
//              异步：提取单个文件用于预览
// ============================================================
void ArchivePreviewWidget::onItemDoubleClicked(QTreeWidgetItem *item, int)
{
	if (!item || m_currentArchive.isEmpty() || !m_lib) return;
	if (item->childCount() > 0) return;
	
	const QVariant idxVar = item->data(0, Qt::UserRole + 1);
	if (!idxVar.isValid()) return;
	const uint32_t index = idxVar.toUInt();
	const QString fullPath = item->data(0, Qt::UserRole).toString();
	
	if (m_extractWatcher.isRunning()) m_extractWatcher.waitForFinished();
	
	m_preview->setPlainText(tr("正在提取…"));
	
	const QString lower = m_currentArchive.toLower();
	const bool isZip = lower.endsWith(".zip");
	const std::string archivePath = m_currentArchive.toStdString();
	bit7z::Bit7zLibrary *lib = m_lib;
	
	QFuture<QByteArray> future = QtConcurrent::run(
												   [lib, archivePath, isZip, index]() -> QByteArray {
													   
													   try {
														   const bit7z::BitInFormat &format =
														   isZip ? bit7z::BitFormat::Zip : bit7z::BitFormat::SevenZip;
														   bit7z::BitArchiveReader reader(*lib, archivePath, format);
														   
														   std::vector<bit7z::byte_t> buffer;
														   reader.extractTo(buffer, index);
														   
														   return QByteArray(reinterpret_cast<const char*>(buffer.data()),
																			 static_cast<int>(buffer.size()));
													   } catch (const bit7z::BitException &) {
														   return {};
													   }
												   });
	
	m_extractWatcher.setProperty("path", fullPath);
	m_extractWatcher.setFuture(future);
}

void ArchivePreviewWidget::onEntryExtracted(const QByteArray &data, const QString &fullPath)
{
	previewData(data, fullPath);
}

// ============================================================
//                        右键菜单
// ============================================================
void ArchivePreviewWidget::onTreeContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *item = m_tree->itemAt(pos);
	if (!item) return;
	if (m_currentArchive.isEmpty() || !m_lib) return;
	
	const bool isFile = item->data(0, Qt::UserRole + 1).isValid();
	const bool isDir  = item->data(0, Qt::UserRole + 2).toBool();
	if (!isFile && !isDir) return;
	if (isDir && collectFileIndices(item).isEmpty()) return;
	
	QMenu menu(this);
	QAction *actDefault = menu.addAction(tr("解压到 Compressed 文件夹"));
	QAction *actChoose  = menu.addAction(tr("解压到..."));
	
	QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
	if (chosen == actDefault)      extractToDefault(item);
	else if (chosen == actChoose)  extractToChosen(item);
}

QList<uint32_t> ArchivePreviewWidget::collectFileIndices(QTreeWidgetItem *item) const
{
	QList<uint32_t> indices;
	if (!item) return indices;
	
	const QVariant idxVar = item->data(0, Qt::UserRole + 1);
	if (idxVar.isValid()) indices.append(idxVar.toUInt());
	
	for (int i = 0; i < item->childCount(); ++i)
		indices.append(collectFileIndices(item->child(i)));
	
	return indices;
}

void ArchivePreviewWidget::extractToDefault(QTreeWidgetItem *item)
{
	if (m_currentArchive.isEmpty()) return;
	
	const QString appDir = QCoreApplication::applicationDirPath();
	const QString folder = SettingsManager::instance().archiveDefaultExtractFolder();
	const QString destDir = QDir(appDir).filePath(
												  folder.isEmpty() ? "Compressed" : folder);
	
	QDir().mkpath(destDir);
	extractItem(item, destDir);
}

void ArchivePreviewWidget::extractToChosen(QTreeWidgetItem *item)
{
	QString destDir = QFileDialog::getExistingDirectory(
														this, tr("选择解压目标文件夹"),
														QFileInfo(m_currentArchive).absolutePath());
	if (destDir.isEmpty()) return;
	extractItem(item, destDir);
}

// 注意：解压本身还是同步的。因为要写文件+进度提示，
// 如果要完全异步，需要改成 QThread + 信号进度。
// 目前先保持同步（用户主动触发，有心理预期）。
void ArchivePreviewWidget::extractItem(QTreeWidgetItem *item, const QString &destDir)
{
	if (!item || m_currentArchive.isEmpty() || !m_lib) return;
	
	QList<uint32_t> indices = collectFileIndices(item);
	if (indices.isEmpty()) return;
	
	if (SettingsManager::instance().archiveConfirmBeforeExtract()) {
		auto ret = QMessageBox::question(
										 this, tr("确认解压"),
										 tr("确定要解压 %1 个文件吗？\n目标：%2").arg(indices.size()).arg(destDir));
		if (ret != QMessageBox::Yes) return;
	}
	
	const QString lower = m_currentArchive.toLower();
	const bit7z::BitInFormat &format = lower.endsWith(".zip")
	? bit7z::BitFormat::Zip : bit7z::BitFormat::SevenZip;
	
	try {
		bit7z::BitArchiveReader reader(*m_lib, m_currentArchive.toStdString(), format);
		
		uint32_t curIndex = 0;
		int successCount = 0;
		QStringList failed;
		
		QSet<uint32_t> wanted;
		for (uint32_t i : indices) wanted.insert(i);
		
		for (const auto &it : reader.items()) {
			if (!wanted.contains(curIndex)) { ++curIndex; continue; }
			
			const QString fullPath = QString::fromUtf8(it.path().c_str());
			if (fullPath.isEmpty()) { ++curIndex; continue; }
			
			std::vector<bit7z::byte_t> buffer;
			reader.extractTo(buffer, curIndex);
			
			QString outPath = QDir(destDir).filePath(fullPath);
			QFileInfo fi(outPath);
			QDir().mkpath(fi.absolutePath());
			
			QFile out(outPath);
			if (!out.open(QIODevice::WriteOnly)) {
				failed.append(outPath);
				++curIndex;
				continue;
			}
			out.write(reinterpret_cast<const char*>(buffer.data()),
					  static_cast<qint64>(buffer.size()));
			out.close();
			++successCount;
			
			++curIndex;
		}
		
		if (!failed.isEmpty()) {
			QMessageBox::warning(this, tr("解压完成（部分失败）"),
								 tr("成功：%1 个\n失败：%2 个\n\n失败文件：\n%3")
								 .arg(successCount).arg(failed.size()).arg(failed.join("\n")));
		} else {
			QMessageBox::information(this, tr("解压完成"),
									 tr("已解压 %1 个文件到：\n%2").arg(successCount).arg(destDir));
		}
		
	} catch (const bit7z::BitException &ex) {
		QMessageBox::warning(this, tr("解压失败"), ex.what());
	}
}

void ArchivePreviewWidget::previewData(const QByteArray &data, const QString &fileName)
{
	const QString lower = fileName.toLower();
	
	if (lower.endsWith(".png") || lower.endsWith(".jpg") || lower.endsWith(".jpeg")
		|| lower.endsWith(".bmp") || lower.endsWith(".gif")) {
		QPixmap pix;
		pix.loadFromData(data);
		if (pix.isNull()) {
			m_preview->setPlainText(tr("无法解析图片"));
		} else {
			QByteArray b64 = data.toBase64();
			QString html = QString("<img src=\"data:image/png;base64,%1\"/>")
			.arg(QString::fromLatin1(b64));
			m_preview->setHtml(html);
		}
		return;
	}
	
	if (lower.endsWith(".txt") || lower.endsWith(".log") || lower.endsWith(".md")
		|| lower.endsWith(".json") || lower.endsWith(".xml") || lower.endsWith(".csv")) {
		m_preview->setPlainText(QString::fromUtf8(data));
		return;
	}
	
	m_preview->setPlainText(tr("（不支持预览此类型文件，大小 %1 字节）").arg(data.size()));
}

QString ArchivePreviewWidget::formatSize(quint64 bytes) const
{
	const char *units[] = { "B", "KB", "MB", "GB", "TB" };
	int i = 0;
	double v = static_cast<double>(bytes);
	while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
	return QString("%1 %2").arg(v, 0, 'f', i == 0 ? 0 : 1).arg(units[i]);
}

QString ArchivePreviewWidget::typeFromName(const QString &name) const
{
	const int dot = name.lastIndexOf('.');
	if (dot < 0) return tr("文件");
	const QString ext = name.mid(dot + 1).toLower();
	
	if (ext == "txt" || ext == "log")                 return tr("文本文档");
	if (ext == "md")                                  return tr("Markdown 文档");
	if (ext == "json")                                return tr("JSON 文件");
	if (ext == "xml")                                 return tr("XML 文件");
	if (ext == "csv")                                 return tr("CSV 文件");
	if (ext == "png" || ext == "jpg" || ext == "jpeg"
		|| ext == "bmp" || ext == "gif" || ext == "webp") return tr("图片");
	if (ext == "mp4" || ext == "mkv" || ext == "avi"
		|| ext == "mov" || ext == "wmv")              return tr("视频");
	if (ext == "mp3" || ext == "flac" || ext == "wav"
		|| ext == "m4a" || ext == "aac")              return tr("音频");
	if (ext == "zip" || ext == "7z" || ext == "tar"
		|| ext == "gz" || ext == "bz2" || ext == "xz") return tr("压缩包");
	if (ext == "pdf")                                 return tr("PDF 文档");
	if (ext == "doc" || ext == "docx")                return tr("Word 文档");
	if (ext == "xls" || ext == "xlsx")                return tr("Excel 表格");
	if (ext == "ppt" || ext == "pptx")                return tr("PowerPoint 演示");
	if (ext == "exe" || ext == "msi")                 return tr("应用程序");
	if (ext == "dll")                                 return tr("动态链接库");
	
	return tr("%1 文件").arg(ext.toUpper());
}
