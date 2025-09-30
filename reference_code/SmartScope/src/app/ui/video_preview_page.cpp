#include "app/ui/video_preview_page.h"
#include "infrastructure/config/config_manager.h"
#include "infrastructure/logging/logger.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QPushButton>
#include <QProcess>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QDateTime>
#include <QDialog>
#include <QApplication>
#include <QScreen>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QIcon>
#include <QFrame>
#include <QMenu>
#include <QAction>
#include <QFile>
#include "app/ui/utils/dialog_utils.h"
#include "app/ui/toast_notification.h"

#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

// ------------------ 无边框视频预览对话框 ------------------
class VideoPreviewDialog : public QDialog {
	Q_OBJECT
public:
	explicit VideoPreviewDialog(QWidget* parent=nullptr)
		: QDialog(parent)
		, m_titleLabel(nullptr)
		, m_infoLabel(nullptr)
	{
		setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
		setAttribute(Qt::WA_TranslucentBackground);
		setWindowOpacity(0.0);

		QScreen* screen = QApplication::primaryScreen();
		QSize screenSize = screen->availableSize();
		int width = int(screenSize.width() * 0.8);
		int height = int(screenSize.height() * 0.8);
		resize(width, height);

		int leftOffset = 80;
		int topOffset = 80;
		int availableWidth = screenSize.width() - leftOffset * 2;
		int availableHeight = screenSize.height() - topOffset - 160;
		if (width > availableWidth) width = availableWidth;
		if (height > availableHeight) height = availableHeight;
		resize(width, height);
		int x = (screenSize.width() - width) / 2;
		int y = topOffset + (availableHeight - height) / 2;
		move(x, y);

		QVBoxLayout* mainLayout = new QVBoxLayout(this);
		mainLayout->setContentsMargins(20, 20, 20, 20);
		mainLayout->setSpacing(0);

		QWidget* container = new QWidget(this);
		container->setObjectName("previewContainer");
		container->setStyleSheet(
			"QWidget#previewContainer {"
			"    background-color: #252526;"
			"    border-radius: 12px;"
			"    border: 1px solid #444;"
			"    padding: 25px;"
			"}"
			"QLabel {"
			"    color: #E0E0E0;"
			"    background-color: transparent;"
			"    padding: 5px;"
			"    font-size: 20pt;"
			"}"
		);
		QVBoxLayout* containerLayout = new QVBoxLayout(container);
		containerLayout->setContentsMargins(15,15,15,15);
		containerLayout->setSpacing(15);

		QWidget* titleBar = new QWidget(container);
		titleBar->setObjectName("titleBar");
		titleBar->setFixedHeight(60);
		QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
		titleLayout->setContentsMargins(20,0,20,0);
		titleLayout->setSpacing(10);

		m_titleLabel = new QLabel("视频预览", titleBar);
		m_titleLabel->setObjectName("titleLabel");
		QPushButton* closeButton = new QPushButton(titleBar);
		closeButton->setObjectName("closeButton");
		closeButton->setIcon(QIcon(":/icons/close.svg"));
		closeButton->setIconSize(QSize(30,30));
		closeButton->setFixedSize(60,60);
		closeButton->setCursor(Qt::PointingHandCursor);
		titleLayout->addWidget(m_titleLabel);
		titleLayout->addStretch();
		titleLayout->addWidget(closeButton);
		closeButton->setStyleSheet(
			"QPushButton#closeButton { background-color: #D9534F; border-radius: 20px; border: none; margin: 10px; }"
			"QPushButton#closeButton:hover { background-color: #C9302C; }"
			"QPushButton#closeButton:pressed { background-color: #B92C28; }"
		);

		QWidget* toolBar = new QWidget(container);
		toolBar->setObjectName("toolBar");
		toolBar->setFixedHeight(80);
		QHBoxLayout* toolLayout = new QHBoxLayout(toolBar);
		toolLayout->setContentsMargins(20,0,20,0);
		toolLayout->setSpacing(20);
		QToolButton* openExtern = new QToolButton(toolBar);
		openExtern->setIcon(QIcon(":/icons/open_in_new.svg"));
		openExtern->setIconSize(QSize(24,24));
		openExtern->setToolTip("使用系统播放器打开");
		openExtern->setFixedSize(50,50);
		QString toolButtonStyle =
			"QToolButton { background-color: #555555; border-radius: 25px; padding: 8px; }"
			"QToolButton:hover { background-color: #666666; }"
			"QToolButton:pressed { background-color: #444444; }";
		openExtern->setStyleSheet(toolButtonStyle);

		QScrollArea* scrollArea = new QScrollArea(container);
		scrollArea->setWidgetResizable(true);
		scrollArea->setFrameShape(QFrame::NoFrame);
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		scrollArea->setStyleSheet(
			"QScrollArea { background-color: rgba(20,20,20,100); border-radius: 10px; }"
			"QScrollBar:horizontal, QScrollBar:vertical { background: rgba(40,40,40,100); height: 12px; width: 12px; border-radius: 6px; margin: 0px; }"
			"QScrollBar::handle:horizontal, QScrollBar::handle:vertical { background: rgba(100,100,100,150); border-radius: 5px; min-width: 30px; min-height: 30px; }"
			"QScrollBar::handle:horizontal:hover, QScrollBar::handle:vertical:hover { background: rgba(120,120,120,200); }"
			"QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }"
			"QScrollBar::add-page, QScrollBar::sub-page { background: none; }"
		);
		m_infoLabel = new QLabel(container);
		m_infoLabel->setAlignment(Qt::AlignCenter);
		m_infoLabel->setStyleSheet("color: #CCCCCC; font-size: 22px; padding: 8px; background-color: rgba(40,40,40,100); border-radius: 8px;");

		// 预览占位：使用缩略图大图显示
		m_previewLabel = new QLabel(scrollArea);
		m_previewLabel->setAlignment(Qt::AlignCenter);
		m_previewLabel->setMinimumSize(640, 360);
		m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		m_previewLabel->setStyleSheet("background-color: transparent; border-radius: 5px;");
		scrollArea->setWidget(m_previewLabel);

		containerLayout->addWidget(titleBar);
		containerLayout->addWidget(scrollArea, 1);
		containerLayout->addWidget(m_infoLabel);
		containerLayout->addWidget(toolBar);
		mainLayout->addWidget(container);

		connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
		connect(openExtern, &QToolButton::clicked, this, [this]() {
			if (!m_videoPath.isEmpty()) QProcess::startDetached("xdg-open", QStringList() << m_videoPath);
		});

		// 拖动窗口支持
		titleBar->installEventFilter(this);
	}

	void setVideo(const QString& path) {
		m_videoPath = path;
		QFileInfo fi(path);
		m_titleLabel->setText("视频预览");
		m_infoLabel->setText(QString("%1 | %2 KB | %3")
			.arg(fi.fileName())
			.arg(fi.size()/1024)
			.arg(fi.lastModified().toString("yyyy-MM-dd HH:mm"))
		);
		// 使用与卡片相同的缩略图风格
		QPixmap px(":/icons/record_start.svg");
		QPixmap scaled = px.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		QPixmap white(scaled.size()); white.fill(Qt::transparent);
		QPainter p(&white);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		p.drawPixmap(0,0,scaled);
		p.setCompositionMode(QPainter::CompositionMode_SourceIn);
		p.fillRect(white.rect(), QColor(255,255,255,255));
		p.end();
		m_previewLabel->setPixmap(white);
	}

	bool eventFilter(QObject* watched, QEvent* event) override {
		static QPoint dragPosition;
		if (watched->objectName() == "titleBar" || (watched->parent() && watched->parent()->objectName()=="titleBar")) {
			if (event->type()==QEvent::MouseButtonPress) {
				QMouseEvent* me = static_cast<QMouseEvent*>(event);
				if (me->button()==Qt::LeftButton) {
					dragPosition = me->globalPos() - frameGeometry().topLeft();
					event->accept(); return true;
				}
			} else if (event->type()==QEvent::MouseMove) {
				QMouseEvent* me = static_cast<QMouseEvent*>(event);
				if (me->buttons() & Qt::LeftButton) {
					move(me->globalPos() - dragPosition);
					event->accept(); return true;
				}
			}
		}
		return QDialog::eventFilter(watched, event);
	}

private:
	QLabel* m_titleLabel;
	QLabel* m_infoLabel;
	QLabel* m_previewLabel;
	QString m_videoPath;
};

// ------------------ 缩略图卡片 ------------------
class VideoCard : public QWidget {
	Q_OBJECT
public:
	explicit VideoCard(const QString& filePath, QWidget* parent=nullptr)
		: QWidget(parent), m_filePath(filePath), m_fileInfo(filePath)
	{
		setFixedSize(260, 320);
		setMouseTracking(true);
		setFocusPolicy(Qt::StrongFocus);

		QVBoxLayout* layout = new QVBoxLayout(this);
		layout->setContentsMargins(8,8,8,8);
		layout->setSpacing(6);

		m_thumbLabel = new QLabel(this);
		m_thumbLabel->setAlignment(Qt::AlignCenter);
		m_thumbLabel->setMinimumSize(240, 200);
		m_thumbLabel->setMaximumSize(240, 200);
		m_thumbLabel->setScaledContents(false);
		m_thumbLabel->setStyleSheet("background-color: #2A2A2A; border-radius: 5px;");
		layout->addWidget(m_thumbLabel);

		m_nameLabel = new QLabel(this);
		m_nameLabel->setAlignment(Qt::AlignCenter);
		m_nameLabel->setWordWrap(true);
		m_nameLabel->setStyleSheet("color: white; font-size: 28px; font-weight: bold;");
		layout->addWidget(m_nameLabel);

		m_infoLabel = new QLabel(this);
		m_infoLabel->setAlignment(Qt::AlignCenter);
		m_infoLabel->setStyleSheet("color: #AAAAAA; font-size: 24px;");
		layout->addWidget(m_infoLabel);

		setStyleSheet(
			"VideoCard { background-color: #333333; border-radius: 10px; border: 1px solid #444444; }"
			"VideoCard:hover { background-color: #444444; border: 1px solid #666666; }"
		);
		QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
		shadow->setBlurRadius(15);
		shadow->setColor(QColor(0,0,0,100));
		shadow->setOffset(0,2);
		setGraphicsEffect(shadow);

		setProperty("hovered", false);
		updateThumbnail();
	}

	QString getFilePath() const { return m_filePath; }
	QFileInfo getFileInfo() const { return m_fileInfo; }

signals:
	void doubleClicked(const QString& path);

protected:
	void mouseDoubleClickEvent(QMouseEvent* event) override {
		if (event->button()==Qt::LeftButton) emit doubleClicked(m_filePath);
		QWidget::mouseDoubleClickEvent(event);
	}
	void paintEvent(QPaintEvent* e) override {
		QWidget::paintEvent(e);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		if (hasFocus() || property("selected").toBool()) {
			painter.setPen(QPen(QColor(0,120,215),3));
			painter.drawRoundedRect(rect().adjusted(1,1,-1,-1),10,10);
			painter.setPen(QPen(QColor(100,180,255,150),1));
			painter.drawRoundedRect(rect(),10,10);
		} else if (property("hovered").toBool()) {
			painter.setPen(QPen(QColor(80,150,255,100),2));
			painter.drawRoundedRect(rect().adjusted(1,1,-1,-1),10,10);
		} else {
			painter.setPen(QPen(QColor(100,100,100,100),1));
			painter.drawRoundedRect(rect().adjusted(1,1,-1,-1),10,10);
		}
	}
	void enterEvent(QEvent* e) override {
		setProperty("hovered", true);
		QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
		anim->setDuration(150);
		anim->setStartValue(pos());
		anim->setEndValue(pos() - QPoint(0,5));
		anim->setEasingCurve(QEasingCurve::OutCubic);
		anim->start(QAbstractAnimation::DeleteWhenStopped);
		if (auto* sh = static_cast<QGraphicsDropShadowEffect*>(graphicsEffect())) {
			QPropertyAnimation* sa = new QPropertyAnimation(sh, "blurRadius");
			sa->setDuration(150);
			sa->setStartValue(15);
			sa->setEndValue(25);
			sa->setEasingCurve(QEasingCurve::OutCubic);
			sa->start(QAbstractAnimation::DeleteWhenStopped);
		}
		update(); QWidget::enterEvent(e);
	}
	void leaveEvent(QEvent* e) override {
		setProperty("hovered", false);
		QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
		anim->setDuration(150);
		anim->setStartValue(pos());
		anim->setEndValue(pos() + QPoint(0,5));
		anim->setEasingCurve(QEasingCurve::OutCubic);
		anim->start(QAbstractAnimation::DeleteWhenStopped);
		if (auto* sh = static_cast<QGraphicsDropShadowEffect*>(graphicsEffect())) {
			QPropertyAnimation* sa = new QPropertyAnimation(sh, "blurRadius");
			sa->setDuration(150);
			sa->setStartValue(25);
			sa->setEndValue(15);
			sa->setEasingCurve(QEasingCurve::OutCubic);
			sa->start(QAbstractAnimation::DeleteWhenStopped);
		}
		update(); QWidget::leaveEvent(e);
	}

private:
	void updateThumbnail() {
		m_nameLabel->setText(m_fileInfo.fileName());
		QString sizeText = QString("%1 KB").arg(m_fileInfo.size()/1024);
		QString dateText = m_fileInfo.lastModified().toString("yyyy-MM-dd HH:mm");
		m_infoLabel->setText(QString("%1 | %2").arg(sizeText).arg(dateText));
		QPixmap px(":/icons/record_start.svg");
		QPixmap scaled = px.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		QPixmap white(scaled.size());
		white.fill(Qt::transparent);
		QPainter p(&white);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		p.drawPixmap(0,0,scaled);
		p.setCompositionMode(QPainter::CompositionMode_SourceIn);
		p.fillRect(white.rect(), QColor(255,255,255,255));
		p.end();
		QPixmap canvas(240,200);
		canvas.fill(Qt::transparent);
		QPainter pc(&canvas);
		pc.fillRect(canvas.rect(), QColor(42,42,42));
		int x = (240 - white.width())/2;
		int y = (200 - white.height())/2;
		pc.drawPixmap(x,y,white);
		pc.end();
		m_thumbLabel->setPixmap(canvas);
	}

	QString m_filePath;
	QFileInfo m_fileInfo;
	QLabel* m_thumbLabel;
	QLabel* m_nameLabel;
	QLabel* m_infoLabel;
};

VideoPreviewPage::VideoPreviewPage(QWidget* parent)
	: BasePage("视频预览", parent)
	, m_fileWatcher(nullptr)
	, m_reloadTimer(nullptr)
	, m_scrollArea(nullptr)
	, m_scrollContent(nullptr)
	, m_gridLayout(nullptr)
	, m_emptyLabel(nullptr)
	, m_dialog(nullptr)
	, m_longPressTimer(nullptr)
	, m_longPressTriggered(false)
{
	initContent();
	m_fileWatcher = new QFileSystemWatcher(this);
	connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &VideoPreviewPage::handleDirectoryChanged);
	connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &VideoPreviewPage::handleFileChanged);
	m_reloadTimer = new QTimer(this);
	m_reloadTimer->setSingleShot(true);
	connect(m_reloadTimer, &QTimer::timeout, this, &VideoPreviewPage::loadVideos);
	// 长按计时器
	m_longPressTimer = new QTimer(this);
	m_longPressTimer->setSingleShot(true);
	connect(m_longPressTimer, &QTimer::timeout, this, &VideoPreviewPage::handleLongPress);
	LOG_INFO("视频预览页面构造完成");
}

VideoPreviewPage::~VideoPreviewPage()
{
	clearVideoCards();
}

void VideoPreviewPage::setCurrentWorkPath(const QString& rootPath)
{
	QString videos = rootPath + "/Videos";
	if (m_currentWorkPath == videos) return;
	if (!m_currentWorkPath.isEmpty() && m_fileWatcher->directories().contains(m_currentWorkPath)) {
		m_fileWatcher->removePath(m_currentWorkPath);
	}
	m_currentWorkPath = videos;
	if (!m_currentWorkPath.isEmpty()) m_fileWatcher->addPath(m_currentWorkPath);
	LOG_INFO(QString("视频预览路径: %1").arg(m_currentWorkPath));
	loadVideos();
}

void VideoPreviewPage::initContent()
{
	QWidget* contentWidget = getContentWidget();
	QVBoxLayout* contentLayout = getContentLayout();
	// 设置内容区域的边距，避开状态栏和菜单栏，与图片/截屏预览页一致
	contentWidget->setContentsMargins(80, STATUS_BAR_HEIGHT, 80, 160);

	m_scrollArea = new QScrollArea(contentWidget);
	m_scrollArea->setWidgetResizable(true);
	m_scrollArea->setFrameShape(QFrame::NoFrame);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scrollArea->setStyleSheet("background-color: #1E1E1E;");

	m_scrollContent = new QWidget(m_scrollArea);
	m_scrollContent->setStyleSheet("background-color: transparent;");

	m_gridLayout = new QGridLayout(m_scrollContent);
	m_gridLayout->setContentsMargins(15, 15, 15, 15);
	m_gridLayout->setSpacing(15);
	m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	m_scrollArea->setWidget(m_scrollContent);

	m_emptyLabel = new QLabel(m_scrollArea);
	m_emptyLabel->setAlignment(Qt::AlignCenter);
	m_emptyLabel->setStyleSheet("color: #AAAAAA; font-size: 36px; background-color: transparent;");
	m_emptyLabel->setText("当前没有视频");
	m_emptyLabel->setVisible(false);

	contentLayout->addWidget(m_scrollArea);
	m_scrollArea->viewport()->installEventFilter(this);
}

void VideoPreviewPage::clearVideoCards()
{
	if (!m_gridLayout) return;
	while (QLayoutItem* item = m_gridLayout->takeAt(0)) {
		if (QWidget* w = item->widget()) w->hide(), w->deleteLater();
		delete item;
	}
	m_videoCards.clear();
	m_selectedCard = nullptr;
	m_lastClickedCard = nullptr;
	LOG_INFO("清除所有视频卡片");
}

void VideoPreviewPage::updateLayout()
{
	if (!m_gridLayout || !m_scrollContent || m_videoCards.isEmpty()) return;
	while (QLayoutItem* item = m_gridLayout->takeAt(0)) delete item;
	for (int i=0;i<m_videoCards.size();++i) {
		int row = i / m_columnsCount;
		int col = i % m_columnsCount;
		m_gridLayout->addWidget(m_videoCards[i], row, col, Qt::AlignLeft);
		m_videoCards[i]->show();
	}
	LOG_INFO(QString("视频布局更新完成，共 %1 个，固定 %2 列").arg(m_videoCards.size()).arg(m_columnsCount));
}

void VideoPreviewPage::loadVideos()
{
	if (m_isLoading) return;
	m_isLoading = true;
	LOG_INFO(QString("开始加载视频，路径: %1").arg(m_currentWorkPath));
	clearVideoCards();
	if (m_currentWorkPath.isEmpty() || !QDir(m_currentWorkPath).exists()) {
		m_emptyLabel->setText("视频路径无效");
		m_emptyLabel->show();
		m_isLoading = false; return;
	}
	QDir dir(m_currentWorkPath);
	QStringList filters; filters << "*.mp4" << "*.avi" << "*.mkv" << "*.mov";
	dir.setNameFilters(filters);
	dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
	dir.setSorting(QDir::Time);
	QFileInfoList list = dir.entryInfoList();
	if (list.isEmpty()) {
		m_emptyLabel->setText("当前没有视频");
		m_emptyLabel->show();
		m_isLoading = false; return;
	}
	m_emptyLabel->hide();
	for (const QFileInfo& info : list) {
		VideoCard* card = createVideoCard(info.absoluteFilePath());
		if (card) m_videoCards.append(card);
	}
	updateLayout();
	m_isLoading = false;
}

VideoCard* VideoPreviewPage::createVideoCard(const QString& filePath)
{
	VideoCard* card = new VideoCard(filePath, m_scrollContent);
	connect(card, &VideoCard::doubleClicked, this, &VideoPreviewPage::showVideoPreview);
	card->setMouseTracking(true);
	card->setAttribute(Qt::WA_Hover, true);
	card->setProperty("selected", false);
	return card;
}

void VideoPreviewPage::showVideoPreview(const QString& filePath)
{
	// 需求变更：双击直接使用系统播放器打开，不弹对话框
	QProcess::startDetached("xdg-open", QStringList() << filePath);
}

void VideoPreviewPage::handleDirectoryChanged(const QString&)
{
	m_reloadTimer->start(500);
}

void VideoPreviewPage::handleFileChanged(const QString&)
{
	m_reloadTimer->start(500);
}

void VideoPreviewPage::handleLongPress()
{
	QPoint cursorPos = m_scrollArea->viewport()->mapFromGlobal(QCursor::pos());
	QPoint contentPos = cursorPos + QPoint(m_scrollArea->horizontalScrollBar()->value(), m_scrollArea->verticalScrollBar()->value());
	QWidget* clicked = m_scrollContent->childAt(contentPos);
	VideoCard* vcard = nullptr;
	while (clicked && !vcard) {
		vcard = qobject_cast<VideoCard*>(clicked);
		if (!vcard) {
			clicked = clicked->parentWidget();
			if (clicked == m_scrollContent) break;
		}
	}
	if (!vcard) return;
	m_longPressTriggered = true;
	QString filePath = vcard->getFilePath();
	QMenu menu(this);
	menu.setStyleSheet(
		"QMenu {"
		"  background-color: #2B2B2B;"
		"  border: 2px solid #666666;"
		"  padding: 18px;"
		"}"
		"QMenu::item {"
		"  color: #FFFFFF;"
		"  padding: 24px 48px;"
		"  font-size: 36px;"
		"}"
		"QMenu::item:selected {"
		"  background-color: #3D3D3D;"
		"}"
		"QMenu::separator {"
		"  height: 2px;"
		"  background: #555555;"
		"  margin: 12px 6px;"
		"}"
	);
	QAction* deleteAction = menu.addAction("删除");
	QAction* chosen = menu.exec(QCursor::pos());
	if (chosen == deleteAction) {
		using SmartScope::App::Ui::DialogUtils;
		QMessageBox::StandardButton reply = DialogUtils::showStyledConfirmationDialog(
			this,
			"确认删除",
			QString("确定要删除该文件吗？\n%1").arg(filePath),
			"删除",
			"取消"
		);
		if (reply == QMessageBox::Yes) {
			if (QFile::remove(filePath)) {
				showToast(this, "文件已删除", 1500);
				loadVideos();
			} else {
				showToast(this, "删除失败", 2000, ToastNotification::BottomCenter, ToastNotification::Error);
			}
		}
	}
}

void VideoPreviewPage::showEvent(QShowEvent* event)
{
	BasePage::showEvent(event);
	if (m_currentWorkPath.isEmpty()) {
		QString root = SmartScope::Infrastructure::ConfigManager::instance().getValue("app/root_directory", QDir::homePath() + "/data").toString();
		setCurrentWorkPath(root);
	}
	QTimer::singleShot(0, this, [this](){ loadVideos(); });
}

void VideoPreviewPage::hideEvent(QHideEvent* event)
{
	BasePage::hideEvent(event);
}

bool VideoPreviewPage::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == m_scrollArea->viewport()) {
		switch (event->type()) {
			case QEvent::MouseButtonPress: {
				QMouseEvent* me = static_cast<QMouseEvent*>(event);
				if (me->button()==Qt::LeftButton) {
					m_isScrolling = true;
					m_lastMousePos = me->pos();
					m_pressPos = me->pos();
					m_scrollArea->viewport()->setCursor(Qt::ClosedHandCursor);
					m_lastClickTime = QDateTime::currentMSecsSinceEpoch();
					// 启动长按计时器
					m_longPressTriggered = false;
					if (!m_longPressTimer) {
						m_longPressTimer = new QTimer(this);
						m_longPressTimer->setSingleShot(true);
						connect(m_longPressTimer, &QTimer::timeout, this, &VideoPreviewPage::handleLongPress);
					}
					m_longPressTimer->start(600);
				}
				break;
			}
			case QEvent::MouseMove: {
				QMouseEvent* me = static_cast<QMouseEvent*>(event);
				if (m_isScrolling) {
					int deltaY = m_lastMousePos.y() - me->pos().y();
					QScrollBar* v = m_scrollArea->verticalScrollBar();
					if (v && qAbs(deltaY)>2) {
						v->setValue(v->value()+deltaY);
						if (m_longPressTimer && m_longPressTimer->isActive()) m_longPressTimer->stop();
					}
					m_lastMousePos = me->pos();
				}
				break;
			}
			case QEvent::MouseButtonRelease: {
				QMouseEvent* me = static_cast<QMouseEvent*>(event);
				if (me->button()==Qt::LeftButton && m_isScrolling) {
					if (m_longPressTimer && m_longPressTimer->isActive()) m_longPressTimer->stop();
					if (m_longPressTriggered) {
						m_isScrolling = false;
						m_scrollArea->viewport()->setCursor(Qt::ArrowCursor);
						return true;
					}

					// 与拍照预览一致：小位移+短时间窗口 -> 单击/双击判定
					QPoint moveDelta = m_pressPos - me->pos();
					qint64 timeDelta = QDateTime::currentMSecsSinceEpoch() - m_lastClickTime;
					if (moveDelta.manhattanLength() < 10 && timeDelta < 300) {
						QPoint viewportPos = me->pos();
						QPoint contentPos = viewportPos + QPoint(m_scrollArea->horizontalScrollBar()->value(), m_scrollArea->verticalScrollBar()->value());
						QWidget* clicked = m_scrollContent->childAt(contentPos);
						VideoCard* vcard = nullptr;
						while (clicked && !vcard) {
							vcard = qobject_cast<VideoCard*>(clicked);
							if (!vcard) {
								clicked = clicked->parentWidget();
								if (clicked == m_scrollContent) break;
							}
						}
						if (vcard) {
							if (m_selectedCard) {
								m_selectedCard->setFocus(Qt::NoFocusReason);
								m_selectedCard->setProperty("selected", false);
								m_selectedCard->update();
							}
							vcard->setFocus(Qt::MouseFocusReason);
							vcard->setProperty("selected", true);
							vcard->update();
							m_selectedCard = vcard;
							qint64 current = QDateTime::currentMSecsSinceEpoch();
							if (m_lastClickedCard == vcard && current - m_lastClickTime < 500) {
								LOG_INFO(QString("双击视频: %1").arg(vcard->getFilePath()));
								showVideoPreview(vcard->getFilePath());
							}
							m_lastClickedCard = vcard;
							m_lastClickTime = current;
						} else {
							if (m_selectedCard) {
								m_selectedCard->setFocus(Qt::NoFocusReason);
								m_selectedCard->setProperty("selected", false);
								m_selectedCard->update();
								m_selectedCard = nullptr;
							}
						}
					}

					m_isScrolling = false;
					m_scrollArea->viewport()->setCursor(Qt::ArrowCursor);
				}
				break;
			}
			default: break;
		}
	}
	return BasePage::eventFilter(watched, event);
}

#include "video_preview_page.moc" 