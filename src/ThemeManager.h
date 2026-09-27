#pragma once

#include <QObject>
#include <QString>

class QApplication;

// 主题管理：负责加载/应用 QSS、切换亮/暗主题
class ThemeManager : public QObject
{
	Q_OBJECT
public:
	static ThemeManager &instance();
	
	// 应用当前主题（应在 QApplication 创建后调用一次）
	void apply(QApplication *app);
	
	// 亮/暗切换
	bool isDark() const { return m_dark; }
	void setDark(bool dark);
	void toggle() { setDark(!m_dark); }
	
	// 从 QSettings 读取上次的主题
	void loadFromSettings();
	// 写入 QSettings
	void saveToSettings();
	
	signals:
	void themeChanged(bool dark);
	
private:
	ThemeManager();
	Q_DISABLE_COPY(ThemeManager)
	
	void applyStyleSheet(QApplication *app);
	
	bool m_dark = false;
};
