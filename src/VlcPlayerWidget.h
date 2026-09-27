#pragma once

#include <QWidget>
#include <QStringList>

struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_media_t;

class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class QComboBox;
class QListWidget;
class QSplitter;

class VlcPlayerWidget : public QWidget
{
	Q_OBJECT
public:
	explicit VlcPlayerWidget(QWidget *parent = nullptr);
	~VlcPlayerWidget() override;
	
	void playFile(const QString &path);
	
public slots:
	void togglePause();
	void stop();
	
protected:
	void dragEnterEvent(QDragEnterEvent *e) override;
	void dropEvent(QDropEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	
private slots:
	void onOpenFile();
	void onOpenFolder();
	void onPlayPause();
	void onStop();
	void onPrev();
	void onNext();
	
	void onSliderPressed();
	void onSliderReleased();
	void onSliderMoved(int value);
	
	void onVolumeChanged(int value);
	void onMuteToggled(bool muted);
	
	void onSpeedChanged(int index);
	
	void onToggleFullscreen();
	
	void onTick();
	
	void onPlaylistDoubleClicked();
	
private:
	void buildUi();
	void attachVlcToWindow();
	void updateButtons();
	void updateTimeLabel();
	
	bool loadMedia(const QString &path);
	void playCurrent();
	void setPlaylist(const QStringList &files, int startIndex);
	void refreshPlaylistView();
	
	libvlc_instance_t *m_vlc = nullptr;
	libvlc_media_player_t *m_mp = nullptr;
	
	// ---- UI ----
	QWidget *m_videoFrame = nullptr;
	QPushButton *m_btnOpen = nullptr;
	QPushButton *m_btnOpenFolder = nullptr;
	QPushButton *m_btnPrev = nullptr;
	QPushButton *m_btnPlayPause = nullptr;
	QPushButton *m_btnStop = nullptr;
	QPushButton *m_btnNext = nullptr;
	QPushButton *m_btnMute = nullptr;
	QPushButton *m_btnFullscreen = nullptr;
	
	QSlider *m_slider = nullptr;        // 进度
	QSlider *m_volume = nullptr;        // 音量
	QLabel *m_timeLabel = nullptr;
	QLabel *m_volumeLabel = nullptr;
	QComboBox *m_speedCombo = nullptr;
	
	QListWidget *m_playlist = nullptr;
	QSplitter *m_splitter = nullptr;
	
	QTimer *m_timer = nullptr;
	bool m_sliderDragging = false;
	
	// ---- 播放状态 ----
	QStringList m_files;
	int m_currentIndex = -1;
	bool m_fullscreen = false;
	bool m_muted = false;
	int m_lastVolume = 80;
};
