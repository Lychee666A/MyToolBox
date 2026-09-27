#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>

// ★ 搜索引擎
struct SearchEngine {
	QString id;        // 唯一 ID
	QString name;      // 显示名，如 "Bing"
	QString keyword;   // 关键字，如 "bing"（可选）
	QString url;       // 含 %1 占位符
};

class SettingsManager : public QObject {
	Q_OBJECT
public:
	static SettingsManager &instance();
	
	// ---- 通用 ----
	bool restoreLastTab() const;
	void setRestoreLastTab(bool v);
	int  lastTabIndex() const;
	void setLastTabIndex(int v);
	
	// ---- 压缩包预览 ----
	QString archiveDefaultExtractFolder() const;
	void setArchiveDefaultExtractFolder(const QString &name);
	bool archiveConfirmBeforeExtract() const;
	void setArchiveConfirmBeforeExtract(bool v);
	bool archiveExpandAllOnOpen() const;
	void setArchiveExpandAllOnOpen(bool v);
	
	// ---- 图片查看 ----
	bool imageKeepAspect() const;
	void setImageKeepAspect(bool v);
	QString imageBackgroundColor() const;
	void setImageBackgroundColor(const QString &c);
	
	// ---- 快捷网页 ----
	QList<QPair<QString, QString>> quickLinks() const;
	void setQuickLinks(const QList<QPair<QString, QString>> &links);
	
	// ---- 拍照 / 录像 / 录音 ----
	QString cameraOutputDir() const;         void setCameraOutputDir(const QString &dir);
	QString cameraPhotoFormat() const;       void setCameraPhotoFormat(const QString &fmt);
	QString cameraVideoFormat() const;       void setCameraVideoFormat(const QString &fmt);
	QString cameraVideoCodec() const;        void setCameraVideoCodec(const QString &codec);
	QString cameraQuality() const;           void setCameraQuality(const QString &q);
	QString cameraAudioFormat() const;       void setCameraAudioFormat(const QString &fmt);
	bool    cameraAutoStart() const;         void setCameraAutoStart(bool v);
	
	// ---- 录屏 ----
	int     screenFps() const;               void setScreenFps(int fps);
	QString screenCodec() const;             void setScreenCodec(const QString &codec);
	int     screenAudioSource() const;       void setScreenAudioSource(int src);
	int     screenCaptureType() const;       void setScreenCaptureType(int type);
	QString screenShotFormat() const;        void setScreenShotFormat(const QString &fmt);
	bool    screenIncludeCursor() const;     void setScreenIncludeCursor(bool v);
	
	// ---- 文件搜索 ----
	int     searchPageSize() const;          void setSearchPageSize(int v);
	int     searchHistorySize() const;       void setSearchHistorySize(int v);
	bool    searchClearHistoryOnStart() const; void setSearchClearHistoryOnStart(bool v);
	int     searchDoubleClickAction() const; void setSearchDoubleClickAction(int a);
	int     searchDefaultSort() const;       void setSearchDefaultSort(int s);
	
	// ---- 浏览器（WebView2） ----
	QString browserHomeUrl() const;          void setBrowserHomeUrl(const QString &url);
	QString browserSearchEngine() const;     void setBrowserSearchEngine(const QString &engine); // 旧接口，保留兼容
	bool    browserRememberHistory() const;  void setBrowserRememberHistory(bool v);
	
	// ★ 新增：搜索引擎列表
	QList<SearchEngine> searchEngines() const;
	void setSearchEngines(const QList<SearchEngine> &engines);
	
	// ★ 新增：默认搜索引擎 id
	QString defaultSearchEngineId() const;
	void setDefaultSearchEngineId(const QString &id);
	
	// ★ 便捷：按 id 取引擎；取不到返回第一个；列表空返回无效引擎
	SearchEngine searchEngineById(const QString &id) const;
	SearchEngine defaultSearchEngine() const;
	
	// ---- VLC 播放器（预留） ----
	QString vlcPluginPath() const;           void setVlcPluginPath(const QString &p);
	
	signals:
	void settingsChanged();
	
private:
	SettingsManager();
	Q_DISABLE_COPY(SettingsManager)
};
