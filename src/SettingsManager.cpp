#include "SettingsManager.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>

SettingsManager &SettingsManager::instance() {
	static SettingsManager s;
	return s;
}

SettingsManager::SettingsManager() : QObject(nullptr) {
	// ============================================================
	// ★ 一次性迁移：把旧的 bing 默认改为 baidu / hao123
	// ============================================================
	QSettings s;
	const QString kMigFlag = "migration/v2_home_search_defaults";
	
	if (!s.value(kMigFlag, false).toBool()) {
		// 主页：旧默认值（bing）或从未设置 → 改成 hao123
		const QString homeUrl = s.value("browser/homeUrl").toString();
		if (homeUrl.isEmpty() || homeUrl == "https://www.bing.com") {
			s.setValue("browser/homeUrl", "https://www.hao123.com");
		}
		
		// 默认搜索引擎 id：旧的 "bing" 或从未设置 → 改成 baidu
		const QString defId = s.value("browser/defaultSearchEngineId").toString();
		if (defId.isEmpty() || defId == "bing") {
			s.setValue("browser/defaultSearchEngineId", "baidu");
		}
		
		// 旧接口 browser/searchEngine
		const QString oldEngine = s.value("browser/searchEngine").toString();
		if (oldEngine.isEmpty() || oldEngine == "bing") {
			s.setValue("browser/searchEngine", "baidu");
		}
		
		// 搜索引擎列表：如果从未写过，删掉让 searchEngines() 重新生成
		// （这样新的默认顺序「百度首位」才会生效）
		if (!s.contains("browser/searchEngines")) {
			s.remove("browser/searchEngines");
		}
		
		s.setValue(kMigFlag, true);
		s.sync();
	}
}

// ---------- 通用 ----------
bool SettingsManager::restoreLastTab() const {
	return QSettings().value("general/restoreLastTab", true).toBool();
}
void SettingsManager::setRestoreLastTab(bool v) {
	QSettings().setValue("general/restoreLastTab", v);
	emit settingsChanged();
}

int SettingsManager::lastTabIndex() const {
	return QSettings().value("general/lastTabIndex", 0).toInt();
}
void SettingsManager::setLastTabIndex(int v) {
	QSettings().setValue("general/lastTabIndex", v);
}

// ---------- 压缩包预览 ----------
QString SettingsManager::archiveDefaultExtractFolder() const {
	return QSettings().value("archive/defaultExtractFolder", "Compressed").toString();
}
void SettingsManager::setArchiveDefaultExtractFolder(const QString &name) {
	QSettings().setValue("archive/defaultExtractFolder", name);
	emit settingsChanged();
}

bool SettingsManager::archiveConfirmBeforeExtract() const {
	return QSettings().value("archive/confirmBeforeExtract", false).toBool();
}
void SettingsManager::setArchiveConfirmBeforeExtract(bool v) {
	QSettings().setValue("archive/confirmBeforeExtract", v);
	emit settingsChanged();
}

bool SettingsManager::archiveExpandAllOnOpen() const {
	return QSettings().value("archive/expandAllOnOpen", false).toBool();
}
void SettingsManager::setArchiveExpandAllOnOpen(bool v) {
	QSettings().setValue("archive/expandAllOnOpen", v);
	emit settingsChanged();
}

// ---------- 图片查看 ----------
bool SettingsManager::imageKeepAspect() const {
	return QSettings().value("image/keepAspect", true).toBool();
}
void SettingsManager::setImageKeepAspect(bool v) {
	QSettings().setValue("image/keepAspect", v);
	emit settingsChanged();
}

QString SettingsManager::imageBackgroundColor() const {
	return QSettings().value("image/backgroundColor", "#222222").toString();
}
void SettingsManager::setImageBackgroundColor(const QString &c) {
	QSettings().setValue("image/backgroundColor", c);
	emit settingsChanged();
}

// ---------- 快捷网页 ----------
QList<QPair<QString, QString>> SettingsManager::quickLinks() const {
	QList<QPair<QString, QString>> list;
	QSettings s;
	int n = s.beginReadArray("quickLinks");
	for (int i = 0; i < n; ++i) {
		s.setArrayIndex(i);
		list.append(qMakePair(s.value("title").toString(), s.value("url").toString()));
	}
	s.endArray();
	
	if (list.isEmpty()) {
		list.append(QPair<QString, QString>("百度",     "https://www.baidu.com"));
		list.append(QPair<QString, QString>("百度翻译", "https://fanyi.baidu.com"));
		list.append(QPair<QString, QString>("B站",      "https://www.bilibili.com"));
	}
	return list;
}
void SettingsManager::setQuickLinks(const QList<QPair<QString, QString>> &links) {
	QSettings s;
	s.beginWriteArray("quickLinks", links.size());
	for (int i = 0; i < links.size(); ++i) {
		s.setArrayIndex(i);
		s.setValue("title", links[i].first);
		s.setValue("url",   links[i].second);
	}
	s.endArray();
	emit settingsChanged();
}

// ---------- 拍照 / 录像 / 录音 ----------
QString SettingsManager::cameraOutputDir() const {
	QString def = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
	if (def.isEmpty()) def = QDir::homePath();
	return QSettings().value("camera/outputDir", def).toString();
}
void SettingsManager::setCameraOutputDir(const QString &dir) {
	QSettings().setValue("camera/outputDir", dir);
	emit settingsChanged();
}

QString SettingsManager::cameraPhotoFormat() const {
	return QSettings().value("camera/photoFormat", "jpg").toString();
}
void SettingsManager::setCameraPhotoFormat(const QString &fmt) {
	QSettings().setValue("camera/photoFormat", fmt);
	emit settingsChanged();
}

QString SettingsManager::cameraVideoFormat() const {
	return QSettings().value("camera/videoFormat", "mp4").toString();
}
void SettingsManager::setCameraVideoFormat(const QString &fmt) {
	QSettings().setValue("camera/videoFormat", fmt);
	emit settingsChanged();
}

QString SettingsManager::cameraVideoCodec() const {
	return QSettings().value("camera/videoCodec", "H264").toString();
}
void SettingsManager::setCameraVideoCodec(const QString &codec) {
	QSettings().setValue("camera/videoCodec", codec);
	emit settingsChanged();
}

QString SettingsManager::cameraQuality() const {
	return QSettings().value("camera/quality", "High").toString();
}
void SettingsManager::setCameraQuality(const QString &q) {
	QSettings().setValue("camera/quality", q);
	emit settingsChanged();
}

QString SettingsManager::cameraAudioFormat() const {
	return QSettings().value("camera/audioFormat", "m4a").toString();
}
void SettingsManager::setCameraAudioFormat(const QString &fmt) {
	QSettings().setValue("camera/audioFormat", fmt);
	emit settingsChanged();
}

// ---------- 录屏 ----------
int SettingsManager::screenFps() const {
	return QSettings().value("screen/fps", 30).toInt();
}
void SettingsManager::setScreenFps(int fps) {
	QSettings().setValue("screen/fps", fps);
	emit settingsChanged();
}

QString SettingsManager::screenCodec() const {
	return QSettings().value("screen/codec", "H264").toString();
}
void SettingsManager::setScreenCodec(const QString &codec) {
	QSettings().setValue("screen/codec", codec);
	emit settingsChanged();
}

int SettingsManager::screenAudioSource() const {
	return QSettings().value("screen/audioSource", 0).toInt();
}
void SettingsManager::setScreenAudioSource(int src) {
	QSettings().setValue("screen/audioSource", src);
	emit settingsChanged();
}

int SettingsManager::screenCaptureType() const {
	return QSettings().value("screen/captureType", 0).toInt();
}
void SettingsManager::setScreenCaptureType(int type) {
	QSettings().setValue("screen/captureType", type);
	emit settingsChanged();
}

QString SettingsManager::screenShotFormat() const {
	return QSettings().value("screen/shotFormat", "png").toString();
}
void SettingsManager::setScreenShotFormat(const QString &fmt) {
	QSettings().setValue("screen/shotFormat", fmt);
	emit settingsChanged();
}

bool SettingsManager::screenIncludeCursor() const {
	return QSettings().value("screen/includeCursor", true).toBool();
}
void SettingsManager::setScreenIncludeCursor(bool v) {
	QSettings().setValue("screen/includeCursor", v);
	emit settingsChanged();
}

// ---------- 文件搜索 ----------
int SettingsManager::searchPageSize() const {
	return QSettings().value("filesearch/pageSize", 1000).toInt();
}
void SettingsManager::setSearchPageSize(int v) {
	QSettings().setValue("filesearch/pageSize", v);
	emit settingsChanged();
}

int SettingsManager::searchHistorySize() const {
	return QSettings().value("filesearch/historySize", 30).toInt();
}
void SettingsManager::setSearchHistorySize(int v) {
	QSettings().setValue("filesearch/historySize", v);
	emit settingsChanged();
}

bool SettingsManager::searchClearHistoryOnStart() const {
	return QSettings().value("filesearch/clearHistoryOnStart", false).toBool();
}
void SettingsManager::setSearchClearHistoryOnStart(bool v) {
	QSettings().setValue("filesearch/clearHistoryOnStart", v);
	emit settingsChanged();
}

int SettingsManager::searchDoubleClickAction() const {
	return QSettings().value("filesearch/doubleClickAction", 0).toInt();
}
void SettingsManager::setSearchDoubleClickAction(int a) {
	QSettings().setValue("filesearch/doubleClickAction", a);
	emit settingsChanged();
}

int SettingsManager::searchDefaultSort() const {
	return QSettings().value("filesearch/defaultSort", 0).toInt();
}
void SettingsManager::setSearchDefaultSort(int s) {
	QSettings().setValue("filesearch/defaultSort", s);
	emit settingsChanged();
}

// ---------- 浏览器（WebView2） ----------
QString SettingsManager::browserHomeUrl() const {
	// ★ 默认主页：hao123
	return QSettings().value("browser/homeUrl", "https://www.hao123.com").toString();
}
void SettingsManager::setBrowserHomeUrl(const QString &url) {
	QSettings().setValue("browser/homeUrl", url);
	emit settingsChanged();
}

QString SettingsManager::browserSearchEngine() const {
	// 旧接口，兼容保留；★ 默认改为 baidu
	return QSettings().value("browser/searchEngine", "baidu").toString();
}
void SettingsManager::setBrowserSearchEngine(const QString &engine) {
	QSettings().setValue("browser/searchEngine", engine);
	emit settingsChanged();
}

bool SettingsManager::browserRememberHistory() const {
	return QSettings().value("browser/rememberHistory", true).toBool();
}
void SettingsManager::setBrowserRememberHistory(bool v) {
	QSettings().setValue("browser/rememberHistory", v);
	emit settingsChanged();
}

// ---------- 搜索引擎 ----------
QList<SearchEngine> SettingsManager::searchEngines() const {
	QList<SearchEngine> list;
	QSettings s;
	int n = s.beginReadArray("browser/searchEngines");
	for (int i = 0; i < n; ++i) {
		s.setArrayIndex(i);
		SearchEngine e;
		e.id      = s.value("id").toString();
		e.name    = s.value("name").toString();
		e.keyword = s.value("keyword").toString();
		e.url     = s.value("url").toString();
		if (!e.id.isEmpty() && !e.url.isEmpty())
			list.append(e);
	}
	s.endArray();
	
	// 首次使用：给一份默认列表（★ 百度放首位）
	if (list.isEmpty()) {
		auto mk = [](const QString & id, const QString & name,
					 const QString & kw, const QString & url) {
			SearchEngine e;
			e.id = id;
			e.name = name;
			e.keyword = kw;
			e.url = url;
			return e;
		};
		list.append(mk("baidu",  "百度",   "baidu",
					   "https://www.baidu.com/s?ie=UTF-8&wd=%1"));
		list.append(mk("bing",   "Bing",   "bing",
					   "https://www.bing.com/search?q=%1"));
		list.append(mk("google", "Google", "google",
					   "https://www.google.com/search?q=%1"));
		list.append(mk("sogou",  "搜狗",   "sogou",
					   "https://www.sogou.com/web?query=%1"));
	}
	return list;
}

void SettingsManager::setSearchEngines(const QList<SearchEngine> &engines) {
	QSettings s;
	s.beginWriteArray("browser/searchEngines", engines.size());
	for (int i = 0; i < engines.size(); ++i) {
		s.setArrayIndex(i);
		s.setValue("id",      engines[i].id);
		s.setValue("name",    engines[i].name);
		s.setValue("keyword", engines[i].keyword);
		s.setValue("url",     engines[i].url);
	}
	s.endArray();
	emit settingsChanged();
}

QString SettingsManager::defaultSearchEngineId() const {
	// ★ 默认搜索引擎：baidu
	return QSettings().value("browser/defaultSearchEngineId", "baidu").toString();
}
void SettingsManager::setDefaultSearchEngineId(const QString &id) {
	QSettings().setValue("browser/defaultSearchEngineId", id);
	emit settingsChanged();
}

SearchEngine SettingsManager::searchEngineById(const QString &id) const {
	const auto list = searchEngines();
	for (const auto &e : list) if (e.id == id) return e;
	return list.isEmpty() ? SearchEngine{} : list.first();
}

SearchEngine SettingsManager::defaultSearchEngine() const {
	return searchEngineById(defaultSearchEngineId());
}

// ---------- VLC ----------
QString SettingsManager::vlcPluginPath() const {
	return QSettings().value("vlc/pluginPath", "").toString();
}
void SettingsManager::setVlcPluginPath(const QString &p) {
	QSettings().setValue("vlc/pluginPath", p);
	emit settingsChanged();
}
