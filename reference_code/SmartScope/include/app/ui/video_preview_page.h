#ifndef VIDEO_PREVIEW_PAGE_H
#define VIDEO_PREVIEW_PAGE_H

#include "app/ui/base_page.h"
#include <QFileSystemWatcher>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QVector>
#include <QPoint>

class VideoPreviewDialog;

class VideoPreviewPage : public BasePage {
    Q_OBJECT
public:
    explicit VideoPreviewPage(QWidget* parent = nullptr);
    ~VideoPreviewPage();

    void setCurrentWorkPath(const QString& rootPath);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void handleDirectoryChanged(const QString& path);
    void handleFileChanged(const QString& path);
    void loadVideos();
    void showVideoPreview(const QString& filePath);
    void handleLongPress();

private:
    void initContent();
    void clearVideoCards();
    void updateLayout();
    class VideoCard* createVideoCard(const QString& filePath);
    void showContextTreeForFile(const QString& filePath);

    QString m_currentWorkPath;
    QFileSystemWatcher* m_fileWatcher;
    QTimer* m_reloadTimer;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QGridLayout* m_gridLayout;
    QLabel* m_emptyLabel;

    QVector<class VideoCard*> m_videoCards;
    class VideoCard* m_selectedCard = nullptr;
    class VideoCard* m_lastClickedCard = nullptr;
    int m_columnsCount = 5;
    bool m_isLoading = false;
    bool m_isScrolling = false;
    QPoint m_lastMousePos;
    QPoint m_pressPos;
    qint64 m_lastClickTime = 0;

    VideoPreviewDialog* m_dialog = nullptr;

    QTimer* m_longPressTimer = nullptr;
    bool m_longPressTriggered = false;
};

#endif // VIDEO_PREVIEW_PAGE_H 