#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QListWidget;
class QComboBox;
class QLabel;

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
	void onBrowseCameraDir();
	void onAddLink();
	void onRemoveLink();
	void onPickImageColor();
	void onAccept();
	void onManageSearchEngines();

private:
	void buildUi();
	void loadFromSettings();
	void saveToSettings();

	// 通用
	QCheckBox *m_chkRestoreLastTab = nullptr;

	// 压缩包
	QLineEdit *m_editExtractFolder = nullptr;
	QCheckBox *m_chkConfirmExtract = nullptr;
	QCheckBox *m_chkExpandAll = nullptr;

	// 图片
	QCheckBox *m_chkKeepAspect = nullptr;
	QPushButton *m_btnColor = nullptr;
	QString m_imageColor;

	// 快捷网页
	QListWidget *m_links = nullptr;
	QLineEdit *m_editTitle = nullptr;
	QLineEdit *m_editUrl = nullptr;

	// 拍照 / 录像
	QLineEdit *m_editCameraDir = nullptr;
	QComboBox *m_cmbPhotoFormat = nullptr;
	QComboBox *m_cmbVideoFormat = nullptr;
	QComboBox *m_cmbVideoCodec = nullptr;
	QComboBox *m_cmbQuality = nullptr;
	QComboBox *m_cmbAudioFormat = nullptr;
	QCheckBox *m_chkCameraAutoStart = nullptr;

	// 录屏
	QComboBox *m_cmbScreenFps = nullptr;
	QComboBox *m_cmbScreenCodec = nullptr;
	QComboBox *m_cmbScreenAudioSource = nullptr;
	QComboBox *m_cmbScreenCaptureType = nullptr;
	QComboBox *m_cmbShotFormat = nullptr;
	QCheckBox *m_chkScreenIncludeCursor = nullptr;

	// 文件搜索
	QSpinBox  *m_searchPageSize = nullptr;
	QSpinBox  *m_searchHistorySize = nullptr;
	QCheckBox *m_chkClearHistoryOnStart = nullptr;
	QComboBox *m_cmbDoubleClickAction = nullptr;
	QComboBox *m_cmbDefaultSort = nullptr;

	// 浏览器
	QLineEdit *m_browserHomeUrl = nullptr;
	QComboBox *m_browserSearchEngine = nullptr;
	QCheckBox *m_browserRememberHistory = nullptr;
	QPushButton *m_btnManageSearchEngines = nullptr;
	QLabel      *m_lblSearchEnginesSummary = nullptr;
};
