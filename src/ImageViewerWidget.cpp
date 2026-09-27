#include "ImageViewerWidget.h"
#include "SettingsManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QResizeEvent>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMessageBox>
#include <QUrl>

ImageViewerWidget::ImageViewerWidget(QWidget *parent)
: QWidget(parent)
{
	buildUi();
	applySettings();
	
	connect(&SettingsManager::instance(), &SettingsManager::settingsChanged,
			this, &ImageViewerWidget::applySettings);
}

void ImageViewerWidget::buildUi()
{
	auto *root = new QVBoxLayout(this);
	
	auto *top = new QHBoxLayout();
	m_btnOpen = new QPushButton(tr("打开图片"), this);
	m_btnPaste = new QPushButton(tr("从剪贴板粘贴"), this);
	top->addWidget(m_btnOpen);
	top->addWidget(m_btnPaste);
	top->addStretch();
	root->addLayout(top);
	
	m_view = new QLabel(tr("拖入、打开或粘贴一张图片"), this);
	m_view->setAlignment(Qt::AlignCenter);
	m_view->setMinimumSize(200, 200);
	root->addWidget(m_view, 1);
	
	connect(m_btnOpen,  &QPushButton::clicked, this, &ImageViewerWidget::onOpenImage);
	connect(m_btnPaste, &QPushButton::clicked, this, &ImageViewerWidget::onPasteImage);
}

void ImageViewerWidget::openImageDialog()
{
	onOpenImage();   // 直接复用现有槽
}

void ImageViewerWidget::onPasteImage()
{
	const QClipboard *clip = QApplication::clipboard();
	if (!clip) return;
	
	const QMimeData *mime = clip->mimeData();
	if (!mime) return;
	
	if (mime->hasImage()) {
		QImage img = qvariant_cast<QImage>(mime->imageData());
		if (!img.isNull()) {
			setImage(QPixmap::fromImage(img));
			return;
		}
	}
	
	if (mime->hasUrls()) {
		for (const QUrl &u : mime->urls()) {
			if (!u.isLocalFile()) continue;
			QPixmap pix(u.toLocalFile());
			if (!pix.isNull()) {
				setImage(pix);
				return;
			}
		}
	}
	
	QMessageBox::information(this, tr("粘贴图片"), tr("剪贴板里没有图片。"));
}

void ImageViewerWidget::applySettings()
{
	auto &s = SettingsManager::instance();
	m_view->setStyleSheet(QString("background:%1;color:#aaa;")
						  .arg(s.imageBackgroundColor()));
	updatePixmap();
}

void ImageViewerWidget::onOpenImage()
{
	QString path = QFileDialog::getOpenFileName(
												this, tr("选择图片"), QString(),
												tr("图片 (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;所有文件 (*.*)"));
	if (path.isEmpty()) return;
	
	QPixmap pix(path);
	if (pix.isNull()) return;
	setImage(pix);
}

void ImageViewerWidget::setImage(const QPixmap &pix)
{
	m_original = pix;
	updatePixmap();
}

void ImageViewerWidget::updatePixmap()
{
	if (m_original.isNull()) return;
	
	const bool keep = SettingsManager::instance().imageKeepAspect();
	m_view->setPixmap(m_original.scaled(
										m_view->size(),
										keep ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio,
										Qt::SmoothTransformation));
}

void ImageViewerWidget::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	updatePixmap();
}
