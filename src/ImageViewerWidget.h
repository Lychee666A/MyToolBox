#pragma once

#include <QWidget>
#include <QPixmap>

class QLabel;
class QPushButton;

class ImageViewerWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ImageViewerWidget(QWidget *parent = nullptr);
	void openImageDialog();   // 公开入口，供托盘调用
	
protected:
	void resizeEvent(QResizeEvent *e) override;
	
private slots:
	void onOpenImage();
	void onPasteImage();
	void applySettings();
	
private:
	void buildUi();
	void setImage(const QPixmap &pix);
	void updatePixmap();
	
	QLabel *m_view = nullptr;
	QPushButton *m_btnOpen = nullptr;
	QPushButton *m_btnPaste = nullptr;
	QPixmap m_original;
};
