#include "ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QDebug>

ThemeManager &ThemeManager::instance()
{
	static ThemeManager s;
	return s;
}

ThemeManager::ThemeManager() : QObject(nullptr) {}

void ThemeManager::loadFromSettings()
{
	QSettings s;
	m_dark = s.value("ui/darkTheme", false).toBool();
}

void ThemeManager::saveToSettings()
{
	QSettings s;
	s.setValue("ui/darkTheme", m_dark);
}

void ThemeManager::apply(QApplication *app)
{
	applyStyleSheet(app);
}

void ThemeManager::setDark(bool dark)
{
	if (m_dark == dark) return;
	m_dark = dark;
	saveToSettings();
	
	if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
		applyStyleSheet(app);
	}
	emit themeChanged(m_dark);
}

void ThemeManager::applyStyleSheet(QApplication *app)
{
	if (!app) return;
	
	// ★ 按主题选择对应 QSS 文件
	const QString path = m_dark ? ":/dark.qss" : ":/light.qss";
	
	QFile f(path);
	QString qss;
	if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qss = QString::fromUtf8(f.readAll());
		f.close();
	} else {
		qWarning() << "[ThemeManager] 无法加载:" << path;
	}
	
	// ★ 整体替换 QSS。所有顶层窗口、菜单、弹窗、Tooltip 一起变
	app->setStyleSheet(qss);
}
