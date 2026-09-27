#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "EverythingApi.h"
#include "SearchEngineDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QFileDialog>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QComboBox>

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent) {
	setWindowTitle(tr("设置"));
	setMinimumSize(600, 560);
	buildUi();
	loadFromSettings();
}

void SettingsDialog::buildUi() {
	auto *root = new QVBoxLayout(this);
	auto *tabs = new QTabWidget(this);
	root->addWidget(tabs);

	// ================= 通用 =================
	{
		auto *page = new QWidget(this);
		auto *lay = new QVBoxLayout(page);
		m_chkRestoreLastTab = new QCheckBox(tr("启动时恢复上次打开的标签页"), page);
		lay->addWidget(m_chkRestoreLastTab);
		lay->addStretch();
		tabs->addTab(page, tr("通用"));
	}

	// ================= 压缩包 =================
	{
		auto *page = new QWidget(this);
		auto *form = new QFormLayout(page);
		m_editExtractFolder = new QLineEdit(page);
		form->addRow(tr("默认解压文件夹名："), m_editExtractFolder);
		m_chkConfirmExtract = new QCheckBox(tr("解压前询问确认"), page);
		form->addRow(QString(), m_chkConfirmExtract);
		m_chkExpandAll = new QCheckBox(tr("打开压缩包后全部展开"), page);
		form->addRow(QString(), m_chkExpandAll);
		tabs->addTab(page, tr("压缩包"));
	}

	// ================= 图片 =================
	{
		auto *page = new QWidget(this);
		auto *form = new QFormLayout(page);
		m_chkKeepAspect = new QCheckBox(tr("保持宽高比"), page);
		form->addRow(QString(), m_chkKeepAspect);
		m_btnColor = new QPushButton(tr("选择背景色…"), page);
		connect(m_btnColor, &QPushButton::clicked, this, &SettingsDialog::onPickImageColor);
		form->addRow(tr("图片背景色："), m_btnColor);
		tabs->addTab(page, tr("图片"));
	}

	// ================= 快捷网页 =================
	{
		auto *page = new QWidget(this);
		auto *lay = new QVBoxLayout(page);
		m_links = new QListWidget(page);
		lay->addWidget(m_links, 1);
		auto *row = new QHBoxLayout();
		m_editTitle = new QLineEdit(page);
		m_editTitle->setPlaceholderText(tr("名称"));
		m_editUrl   = new QLineEdit(page);
		m_editUrl->setPlaceholderText(tr("URL"));
		auto *btnAdd = new QPushButton(tr("添加"), page);
		auto *btnDel = new QPushButton(tr("删除"), page);
		row->addWidget(m_editTitle);
		row->addWidget(m_editUrl, 1);
		row->addWidget(btnAdd);
		row->addWidget(btnDel);
		lay->addLayout(row);
		connect(btnAdd, &QPushButton::clicked, this, &SettingsDialog::onAddLink);
		connect(btnDel, &QPushButton::clicked, this, &SettingsDialog::onRemoveLink);
		tabs->addTab(page, tr("快捷网页"));
	}

	// ================= 拍照 / 录像 =================
	{
		auto *page = new QWidget(this);
		auto *form = new QFormLayout(page);

		// 输出目录
		auto *dirRow = new QHBoxLayout();
		m_editCameraDir = new QLineEdit(page);
		auto *btnBrowse = new QPushButton(tr("浏览…"), page);
		dirRow->addWidget(m_editCameraDir, 1);
		dirRow->addWidget(btnBrowse);
		form->addRow(tr("输出文件夹："), dirRow);
		connect(btnBrowse, &QPushButton::clicked, this, &SettingsDialog::onBrowseCameraDir);

		// 照片格式
		m_cmbPhotoFormat = new QComboBox(page);
		m_cmbPhotoFormat->addItem("JPG", "jpg");
		m_cmbPhotoFormat->addItem("PNG", "png");
		m_cmbPhotoFormat->addItem("BMP", "bmp");
		form->addRow(tr("照片格式："), m_cmbPhotoFormat);

		// 录像格式
		m_cmbVideoFormat = new QComboBox(page);
		m_cmbVideoFormat->addItem("MP4", "mp4");
		m_cmbVideoFormat->addItem("MKV", "mkv");
		m_cmbVideoFormat->addItem("AVI", "avi");
		form->addRow(tr("录像格式："), m_cmbVideoFormat);

		// 编码器
		m_cmbVideoCodec = new QComboBox(page);
		m_cmbVideoCodec->addItem("H.264",  "H264");
		m_cmbVideoCodec->addItem("H.265",  "H265");
		m_cmbVideoCodec->addItem("AV1",    "AV1");
		m_cmbVideoCodec->addItem(tr("自动"), "Auto");
		form->addRow(tr("录像编码器："), m_cmbVideoCodec);

		// 画质
		m_cmbQuality = new QComboBox(page);
		m_cmbQuality->addItem(tr("低"),   "Low");
		m_cmbQuality->addItem(tr("普通"), "Normal");
		m_cmbQuality->addItem(tr("高"),   "High");
		m_cmbQuality->addItem(tr("极高"), "VeryHigh");
		form->addRow(tr("录像画质："), m_cmbQuality);

		// 录音格式
		m_cmbAudioFormat = new QComboBox(page);
		m_cmbAudioFormat->addItem("M4A", "m4a");
		m_cmbAudioFormat->addItem("MP3", "mp3");
		m_cmbAudioFormat->addItem("WAV", "wav");
		form->addRow(tr("录音格式："), m_cmbAudioFormat);

		tabs->addTab(page, tr("拍照/录像"));
	}

	// ================= 录屏 =================
	{
		auto *page = new QWidget(this);
		auto *form = new QFormLayout(page);

		// 默认采集目标
		m_cmbScreenCaptureType = new QComboBox(page);
		m_cmbScreenCaptureType->addItem(tr("显示器"), 0);
		m_cmbScreenCaptureType->addItem(tr("窗口"),   1);
		form->addRow(tr("默认采集目标："), m_cmbScreenCaptureType);

		// 默认帧率
		m_cmbScreenFps = new QComboBox(page);
		m_cmbScreenFps->addItem("15 fps", 15);
		m_cmbScreenFps->addItem("24 fps", 24);
		m_cmbScreenFps->addItem("30 fps", 30);
		m_cmbScreenFps->addItem("60 fps", 60);
		form->addRow(tr("默认帧率："), m_cmbScreenFps);

		// 默认编码器
		m_cmbScreenCodec = new QComboBox(page);
		m_cmbScreenCodec->addItem("H.264", "H264");
		m_cmbScreenCodec->addItem("H.265", "H265");
		m_cmbScreenCodec->addItem("AV1",   "AV1");
		m_cmbScreenCodec->addItem(tr("自动"), "Auto");
		form->addRow(tr("默认编码器："), m_cmbScreenCodec);

		// 默认音频源
		m_cmbScreenAudioSource = new QComboBox(page);
		m_cmbScreenAudioSource->addItem(tr("无音频"),       0);
		m_cmbScreenAudioSource->addItem(tr("系统内部声音"), 1);
		m_cmbScreenAudioSource->addItem(tr("麦克风"),       2);
		form->addRow(tr("默认音频来源："), m_cmbScreenAudioSource);

		// 截图格式
		m_cmbShotFormat = new QComboBox(page);
		m_cmbShotFormat->addItem("PNG", "png");
		m_cmbShotFormat->addItem("JPG", "jpg");
		m_cmbShotFormat->addItem("BMP", "bmp");
		form->addRow(tr("截图格式："), m_cmbShotFormat);

		// 包含鼠标
		m_chkScreenIncludeCursor = new QCheckBox(tr("录制时包含鼠标指针"), page);
		form->addRow(QString(), m_chkScreenIncludeCursor);

		tabs->addTab(page, tr("录屏"));
	}

	// ================= 文件搜索 =================
	{
		auto *page = new QWidget(this);
		auto *form = new QFormLayout(page);

		m_searchPageSize = new QSpinBox(page);
		m_searchPageSize->setRange(100, 10000);
		m_searchPageSize->setSingleStep(100);
		m_searchPageSize->setSuffix(tr(" 项"));
		form->addRow(tr("每页结果数："), m_searchPageSize);

		m_searchHistorySize = new QSpinBox(page);
		m_searchHistorySize->setRange(5, 200);
		m_searchHistorySize->setSuffix(tr(" 条"));
		form->addRow(tr("搜索历史条数："), m_searchHistorySize);

		m_chkClearHistoryOnStart = new QCheckBox(tr("启动时清空搜索历史"), page);
		form->addRow(QString(), m_chkClearHistoryOnStart);

		m_cmbDoubleClickAction = new QComboBox(page);
		m_cmbDoubleClickAction->addItem(tr("打开文件"),         0);
		m_cmbDoubleClickAction->addItem(tr("打开所在文件夹"),   1);
		m_cmbDoubleClickAction->addItem(tr("在资源管理器中定位"), 2);
		form->addRow(tr("双击行为："), m_cmbDoubleClickAction);

		m_cmbDefaultSort = new QComboBox(page);
		m_cmbDefaultSort->addItem(tr("按名称"),   int(EverythingSortBy::Name));
		m_cmbDefaultSort->addItem(tr("按路径"),   int(EverythingSortBy::Path));
		m_cmbDefaultSort->addItem(tr("按大小"),   int(EverythingSortBy::Size));
		m_cmbDefaultSort->addItem(tr("按扩展名"), int(EverythingSortBy::Extension));
		m_cmbDefaultSort->addItem(tr("按修改时间"), int(EverythingSortBy::DateModified));
		m_cmbDefaultSort->addItem(tr("按创建时间"), int(EverythingSortBy::DateCreated));
		form->addRow(tr("默认排序："), m_cmbDefaultSort);

		auto *hint = new QLabel(
		    tr("<i>提示：搜索列显隐可在结果表头右键调整，会自动保存。</i>"), page);
		hint->setWordWrap(true);
		form->addRow(hint);

		tabs->addTab(page, tr("文件搜索"));
	}

	// ================= 浏览器 =================
	{
		auto *page = new QWidget(this);
		auto *form = new QFormLayout(page);

		m_browserHomeUrl = new QLineEdit(page);
		m_browserHomeUrl->setPlaceholderText("https://www.bing.com");
		form->addRow(tr("默认主页："), m_browserHomeUrl);

		// ★ 搜索引擎：只显示"当前默认 + 管理按钮"
		{
			auto *row = new QHBoxLayout();
			m_lblSearchEnginesSummary = new QLabel(page);
			m_btnManageSearchEngines = new QPushButton(tr("管理搜索引擎…"), page);
			row->addWidget(m_lblSearchEnginesSummary, 1);
			row->addWidget(m_btnManageSearchEngines);
			form->addRow(tr("默认搜索引擎："), row);
			connect(m_btnManageSearchEngines, &QPushButton::clicked,
			        this, &SettingsDialog::onManageSearchEngines);
		}

		m_browserRememberHistory = new QCheckBox(tr("记住浏览历史"), page);
		form->addRow(QString(), m_browserRememberHistory);

		auto *hint = new QLabel(
		    tr("<i>提示：浏览历史由 WebView2 自动管理，"
		   "数据目录位于程序目录下的 WebView2Data 文件夹。</i>"), page);
		hint->setWordWrap(true);
		form->addRow(hint);

		tabs->addTab(page, tr("浏览器"));
	}

	// ================= 按钮 =================
	auto *buttons = new QDialogButtonBox(
	    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(buttons);
}

void SettingsDialog::loadFromSettings() {
	auto &s = SettingsManager::instance();

	// 通用
	m_chkRestoreLastTab->setChecked(s.restoreLastTab());

	// 压缩包
	m_editExtractFolder->setText(s.archiveDefaultExtractFolder());
	m_chkConfirmExtract->setChecked(s.archiveConfirmBeforeExtract());
	m_chkExpandAll->setChecked(s.archiveExpandAllOnOpen());

	// 图片
	m_chkKeepAspect->setChecked(s.imageKeepAspect());
	m_imageColor = s.imageBackgroundColor();
	m_btnColor->setStyleSheet(QString("background-color:%1;").arg(m_imageColor));

	// 快捷网页
	m_links->clear();
	for (const auto &p : s.quickLinks()) {
		auto *it = new QListWidgetItem(QString("%1 - %2").arg(p.first, p.second), m_links);
		it->setData(Qt::UserRole,     p.first);
		it->setData(Qt::UserRole + 1, p.second);
	}

	// 浏览器
	m_browserHomeUrl->setText(s.browserHomeUrl());
	{
		const auto def = s.defaultSearchEngine();
		m_lblSearchEnginesSummary->setText(
		    def.name.isEmpty() ? tr("(未设置)") : def.name);
	}
	m_browserRememberHistory->setChecked(s.browserRememberHistory());

	// 拍照 / 录像
	m_editCameraDir->setText(s.cameraOutputDir());
	{
		int i = m_cmbPhotoFormat->findData(s.cameraPhotoFormat());
		if (i >= 0) m_cmbPhotoFormat->setCurrentIndex(i);
	}
	{
		int i = m_cmbVideoFormat->findData(s.cameraVideoFormat());
		if (i >= 0) m_cmbVideoFormat->setCurrentIndex(i);
	}
	{
		int i = m_cmbVideoCodec->findData(s.cameraVideoCodec());
		if (i >= 0) m_cmbVideoCodec->setCurrentIndex(i);
	}
	{
		int i = m_cmbQuality->findData(s.cameraQuality());
		if (i >= 0) m_cmbQuality->setCurrentIndex(i);
	}
	{
		int i = m_cmbAudioFormat->findData(s.cameraAudioFormat());
		if (i >= 0) m_cmbAudioFormat->setCurrentIndex(i);
	}

	// 录屏
	{
		int i = m_cmbScreenCaptureType->findData(s.screenCaptureType());
		if (i >= 0) m_cmbScreenCaptureType->setCurrentIndex(i);
	}
	{
		int i = m_cmbScreenFps->findData(s.screenFps());
		if (i >= 0) m_cmbScreenFps->setCurrentIndex(i);
	}
	{
		int i = m_cmbScreenCodec->findData(s.screenCodec());
		if (i >= 0) m_cmbScreenCodec->setCurrentIndex(i);
	}
	{
		int i = m_cmbScreenAudioSource->findData(s.screenAudioSource());
		if (i >= 0) m_cmbScreenAudioSource->setCurrentIndex(i);
	}
	{
		int i = m_cmbShotFormat->findData(s.screenShotFormat());
		if (i >= 0) m_cmbShotFormat->setCurrentIndex(i);
	}
	m_chkScreenIncludeCursor->setChecked(s.screenIncludeCursor());

	// 文件搜索
	m_searchPageSize->setValue(s.searchPageSize());
	m_searchHistorySize->setValue(s.searchHistorySize());
	m_chkClearHistoryOnStart->setChecked(s.searchClearHistoryOnStart());
	{
		int i = m_cmbDoubleClickAction->findData(s.searchDoubleClickAction());
		if (i >= 0) m_cmbDoubleClickAction->setCurrentIndex(i);
	}
	{
		int i = m_cmbDefaultSort->findData(s.searchDefaultSort());
		if (i >= 0) m_cmbDefaultSort->setCurrentIndex(i);
	}
}

void SettingsDialog::saveToSettings() {
	auto &s = SettingsManager::instance();

	// 通用
	s.setRestoreLastTab(m_chkRestoreLastTab->isChecked());

	// 压缩包
	s.setArchiveDefaultExtractFolder(m_editExtractFolder->text().trimmed().isEmpty()
	                                 ? "Compressed"
	                                 : m_editExtractFolder->text().trimmed());
	s.setArchiveConfirmBeforeExtract(m_chkConfirmExtract->isChecked());
	s.setArchiveExpandAllOnOpen(m_chkExpandAll->isChecked());

	// 图片
	s.setImageKeepAspect(m_chkKeepAspect->isChecked());
	s.setImageBackgroundColor(m_imageColor);

	// 快捷网页
	QList<QPair<QString, QString>> links;
	for (int i = 0; i < m_links->count(); ++i) {
		auto *it = m_links->item(i);
		links.append(qMakePair(it->data(Qt::UserRole).toString(),
		                       it->data(Qt::UserRole + 1).toString()));
	}
	s.setQuickLinks(links);

	// 浏览器
	s.setBrowserHomeUrl(m_browserHomeUrl->text().trimmed());
	s.setBrowserRememberHistory(m_browserRememberHistory->isChecked());
	// 搜索引擎已在 SearchEngineDialog 里保存，这里无需再写

	// 拍照 / 录像
	s.setCameraOutputDir(m_editCameraDir->text().trimmed());
	s.setCameraPhotoFormat(m_cmbPhotoFormat->currentData().toString());
	s.setCameraVideoFormat(m_cmbVideoFormat->currentData().toString());
	s.setCameraVideoCodec(m_cmbVideoCodec->currentData().toString());
	s.setCameraQuality(m_cmbQuality->currentData().toString());
	s.setCameraAudioFormat(m_cmbAudioFormat->currentData().toString());

	// 录屏
	s.setScreenCaptureType(m_cmbScreenCaptureType->currentData().toInt());
	s.setScreenFps(m_cmbScreenFps->currentData().toInt());
	s.setScreenCodec(m_cmbScreenCodec->currentData().toString());
	s.setScreenAudioSource(m_cmbScreenAudioSource->currentData().toInt());
	s.setScreenShotFormat(m_cmbShotFormat->currentData().toString());
	s.setScreenIncludeCursor(m_chkScreenIncludeCursor->isChecked());

	// 文件搜索
	s.setSearchPageSize(m_searchPageSize->value());
	s.setSearchHistorySize(m_searchHistorySize->value());
	s.setSearchClearHistoryOnStart(m_chkClearHistoryOnStart->isChecked());
	s.setSearchDoubleClickAction(m_cmbDoubleClickAction->currentData().toInt());
	s.setSearchDefaultSort(m_cmbDefaultSort->currentData().toInt());
}

void SettingsDialog::onManageSearchEngines()
{
	SearchEngineDialog dlg(this);
	dlg.exec();
	
	// 刷新摘要
	auto &s = SettingsManager::instance();
	const auto def = s.defaultSearchEngine();
	m_lblSearchEnginesSummary->setText(
									   def.name.isEmpty() ? tr("(未设置)") : def.name);
}

void SettingsDialog::onAccept() {
	saveToSettings();
	accept();
}

void SettingsDialog::onBrowseCameraDir() {
	QString dir = QFileDialog::getExistingDirectory(
	                  this, tr("选择输出文件夹"), m_editCameraDir->text());
	if (!dir.isEmpty()) m_editCameraDir->setText(dir);
}

void SettingsDialog::onAddLink() {
	const QString title = m_editTitle->text().trimmed();
	const QString url   = m_editUrl->text().trimmed();
	if (url.isEmpty()) {
		QMessageBox::warning(this, tr("提示"), tr("URL 不能为空"));
		return;
	}
	auto *it = new QListWidgetItem(QString("%1 - %2")
	                               .arg(title.isEmpty() ? url : title, url), m_links);
	it->setData(Qt::UserRole,     title.isEmpty() ? url : title);
	it->setData(Qt::UserRole + 1, url);
	m_editTitle->clear();
	m_editUrl->clear();
}

void SettingsDialog::onRemoveLink() {
	delete m_links->currentItem();
}

void SettingsDialog::onPickImageColor() {
	QColor c = QColorDialog::getColor(QColor(m_imageColor), this, tr("选择背景色"));
	if (!c.isValid()) return;
	m_imageColor = c.name();
	m_btnColor->setStyleSheet(QString("background-color:%1;").arg(m_imageColor));
}
