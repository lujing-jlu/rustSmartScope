#include "app/ui/measurement_type_selection_page.h"
#include "infrastructure/logging/logger.h"

#include <QMouseEvent>
#include <QPainter>
#include <QBoxLayout>
#include <QStyleOption>
#include <QApplication>
#include <QPainterPath>
#include <QScreen>

// MeasurementTypeCard 实现
MeasurementTypeCard::MeasurementTypeCard(MeasurementType type, const QString& title, const QString& iconPath, QWidget* parent)
    : QWidget(parent)
    , m_type(type)
    , m_title(title)
{
    // 设置固定大小和样式
    setFixedSize(320, 360);  // 进一步增大卡片尺寸
    setObjectName("measurementTypeCard");
    setStyleSheet(
        "QWidget#measurementTypeCard {"
        "  background-color: rgba(40, 40, 45, 0.95);"
        "  border-radius: 15px;"
        "  border: 3px solid #3a3a3a;"
        "}"
        "QWidget#measurementTypeCard:hover {"
        "  background-color: rgba(55, 55, 65, 0.95);"
        "  border: 3px solid #5d9cec;"
        "}"
        "QLabel#titleLabel {"
        "  color: #ffffff;"
        "  font-size: 32px;"  // 显著增大字体
        "  font-weight: bold;"
        "}"
        "QLabel#descriptionLabel {"
        "  color: #bbbbbb;"
        "  font-size: 24px;"  // 显著增大字体
        "  font-style: italic;"
        "}"
    );

    // 创建布局
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(20, 20, 20, 20);
    m_layout->setSpacing(15);
    m_layout->setAlignment(Qt::AlignCenter);

    // 创建图标标签
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(200, 200);  // 进一步增大图标尺寸
    m_iconLabel->setScaledContents(true);
    m_iconLabel->setPixmap(QPixmap(iconPath));
    m_layout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    // 创建标题标签
    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName("titleLabel");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true);
    m_layout->addWidget(m_titleLabel, 0, Qt::AlignCenter);
    
    // 添加描述标签
    QString description;
    switch (type) {
        case MeasurementType::Length:
            description = "测量两点间的直线距离";
            break;
        case MeasurementType::PointToLine:
            description = "测量点到线的垂直距离";
            break;
        case MeasurementType::Depth:
            description = "测量点的深度值";
            break;
        case MeasurementType::Area:
            description = "测量选定区域的面积";
            break;
        case MeasurementType::Polyline:
            description = "测量多段线的总长度";
            break;
        case MeasurementType::Profile:
            description = "分析线段上的高度变化";
            break;
        case MeasurementType::RegionProfile:
            description = "分析区域内的剖面特征";
            break;
        case MeasurementType::MissingArea:
            description = "计算缺失部分的面积";
            break;
        default:
            description = "点击选择此测量类型";
            break;
    }
    
    QLabel* descriptionLabel = new QLabel(description, this);
    descriptionLabel->setObjectName("descriptionLabel");
    descriptionLabel->setAlignment(Qt::AlignCenter);
    descriptionLabel->setWordWrap(true);
    m_layout->addWidget(descriptionLabel, 0, Qt::AlignCenter);

    // 添加弹性空间
    m_layout->addStretch();
}

MeasurementTypeCard::~MeasurementTypeCard()
{
}

MeasurementType MeasurementTypeCard::getType() const
{
    return m_type;
}

void MeasurementTypeCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit cardClicked(m_type);
    }
    QWidget::mousePressEvent(event);
}

void MeasurementTypeCard::paintEvent(QPaintEvent* event)
{
    // 绘制自定义背景
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
    QWidget::paintEvent(event);
}

// MeasurementTypeSelectionPage 实现
MeasurementTypeSelectionPage::MeasurementTypeSelectionPage(QWidget* parent)
    : BasePage("选择测量类型", parent)
    , m_cardsLayout(nullptr)
    , m_cancelButton(nullptr)
{
    LOG_INFO("创建测量类型选择页面");
    
    // 设置窗口属性为无边框
    setWindowFlags(Qt::FramelessWindowHint);
    
    // 先调用initContent初始化UI
    initContent();
    
    // 然后创建测量类型卡片
    createMeasurementTypeCards();
}

MeasurementTypeSelectionPage::~MeasurementTypeSelectionPage()
{
    LOG_INFO("销毁测量类型选择页面");
}

void MeasurementTypeSelectionPage::initContent()
{
    // 获取内容区域
    QWidget* contentWidget = getContentWidget();
    QVBoxLayout* contentLayout = getContentLayout();
    
    // 设置内容区域样式
    contentWidget->setStyleSheet(
        "QWidget#pageContent {"
        "  background-color: #1E1E1E;"
        "  border-radius: 0px;"
        "}"
    );
    
    // 设置标题样式
    m_titleLabel->setStyleSheet(
        "QLabel#pageTitle {"
        "  color: #ffffff;"
        "  font-size: 42px;"  // 增大页面标题字体
        "  font-weight: bold;"
        "  margin-top: 20px;"
        "  margin-bottom: 30px;"
        "}"
    );
    m_titleLabel->setFixedHeight(70); // 增加标题高度
    m_titleLabel->setContentsMargins(0, 0, 0, 0);
    
    // 创建中间容器，用于存放卡片网格
    QWidget* cardsContainer = new QWidget(contentWidget);
    cardsContainer->setStyleSheet("background: transparent;");
    
    // 创建卡片布局
    m_cardsLayout = new QGridLayout(cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(70);  // 进一步增加卡片间距
    m_cardsLayout->setAlignment(Qt::AlignCenter);
    
    contentLayout->addWidget(cardsContainer, 1, Qt::AlignCenter);

    // 创建取消按钮
    m_cancelButton = new QPushButton("取消", contentWidget);
    m_cancelButton->setObjectName("cancelButton");
    m_cancelButton->setFixedSize(240, 80);  // 进一步增大按钮尺寸
    m_cancelButton->setStyleSheet(
        "QPushButton#cancelButton {"
        "  background-color: #3a3a3a;"
        "  color: #ffffff;"
        "  border-radius: 10px;"
        "  border: 2px solid #505050;"
        "  font-size: 30px;"  // 显著增大字体
        "  font-weight: bold;"
        "  padding: 10px 20px;"
        "}"
        "QPushButton#cancelButton:hover {"
        "  background-color: #454545;"
        "  border: 2px solid #5d9cec;"
        "}"
        "QPushButton#cancelButton:pressed {"
        "  background-color: #2c2c2c;"
        "}"
    );
    connect(m_cancelButton, &QPushButton::clicked, this, &MeasurementTypeSelectionPage::onCancelClicked);
    
    // 创建一个容器来放置取消按钮，并设置底部边距
    QWidget* buttonContainer = new QWidget(contentWidget);
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(m_cancelButton, 0, Qt::AlignCenter);
    
    // 添加按钮容器到内容布局，并设置底部边距
    contentLayout->addWidget(buttonContainer, 0, Qt::AlignCenter);
    
    // 设置内容布局的边距，增加底部边距以避免被菜单栏遮挡
    contentLayout->setContentsMargins(80, 30, 80, 170);  // 增加顶部和底部边距
    contentLayout->setSpacing(50);  // 增加间距
}

void MeasurementTypeSelectionPage::createMeasurementTypeCards()
{
    // 创建测量类型卡片
    struct CardInfo {
        MeasurementType type;
        QString title;
        QString iconPath;
    };

    // 定义卡片信息（使用SVG图标）
    QVector<CardInfo> cardInfos = {
        { MeasurementType::Length, "长度测量", ":/icons/measurement/length.svg" },
        { MeasurementType::PointToLine, "点到线测量", ":/icons/measurement/point_to_line.svg" },
        { MeasurementType::Depth, "深度测量", ":/icons/measurement/depth.svg" },
        { MeasurementType::Area, "面积测量", ":/icons/measurement/area.svg" },
        { MeasurementType::Polyline, "折线测量", ":/icons/measurement/polyline.svg" },
        { MeasurementType::Profile, "轮廓测量", ":/icons/measurement/profile.svg" },
        { MeasurementType::MissingArea, "缺失面积", ":/icons/measurement/missing_area.svg" }
    };
    
    // 强制两行布局
    int totalCards = cardInfos.size();
    int firstRowCount = (totalCards + 1) / 2; // 向上取整，第一行放置更多卡片
    int secondRowCount = totalCards - firstRowCount;
    
    // 第一行的列计数
    for (int i = 0; i < firstRowCount; i++) {
        MeasurementTypeCard* card = new MeasurementTypeCard(
            cardInfos[i].type, 
            cardInfos[i].title, 
            cardInfos[i].iconPath, 
            this
        );
        connect(card, &MeasurementTypeCard::cardClicked, this, &MeasurementTypeSelectionPage::onCardClicked);
        m_cardsLayout->addWidget(card, 0, i, Qt::AlignCenter);
        m_typeCards.append(card);
    }
    
    // 第二行的列计数
    for (int i = 0; i < secondRowCount; i++) {
        MeasurementTypeCard* card = new MeasurementTypeCard(
            cardInfos[i + firstRowCount].type, 
            cardInfos[i + firstRowCount].title, 
            cardInfos[i + firstRowCount].iconPath, 
            this
        );
        connect(card, &MeasurementTypeCard::cardClicked, this, &MeasurementTypeSelectionPage::onCardClicked);
        // 第二行的水平偏移，确保居中对齐
        int horizontalOffset = (firstRowCount - secondRowCount) / 2;
        m_cardsLayout->addWidget(card, 1, i + horizontalOffset, Qt::AlignCenter);
        m_typeCards.append(card);
    }
}

void MeasurementTypeSelectionPage::onCardClicked(MeasurementType type)
{
    LOG_INFO(QString("选择测量类型: %1").arg(static_cast<int>(type)));
    emit measurementTypeSelected(type);
}

void MeasurementTypeSelectionPage::onCancelClicked()
{
    LOG_INFO("取消选择测量类型");
    emit cancelSelection();
}

// 显示前调整大小并隐藏菜单栏
void MeasurementTypeSelectionPage::showEvent(QShowEvent* event)
{
    // 确保覆盖整个内容区域，但不遮挡状态栏
    if (parentWidget()) {
        QRect parentRect = parentWidget()->rect();

        // 使用状态栏高度常量，保留顶部空间
        int statusBarHeight = BasePage::STATUS_BAR_HEIGHT;
        setGeometry(parentRect.x(),
                    parentRect.y() + statusBarHeight,
                    parentRect.width(),
                    parentRect.height() - statusBarHeight);
        
        // 隐藏菜单栏
        QWidget* mainWindow = this->window();
        if (mainWindow) {
            QList<QWidget*> menuBars = mainWindow->findChildren<QWidget*>("measurementMenuBar");
            for (QWidget* menuBar : menuBars) {
                if (menuBar->isVisible()) {
                    menuBar->setProperty("was_visible", true);
                    menuBar->hide();
                    LOG_INFO("测量类型选择页面：隐藏菜单栏");
                }
            }
        }
    }
    QWidget::showEvent(event);
}

// 隐藏时恢复菜单栏
void MeasurementTypeSelectionPage::hideEvent(QHideEvent* event)
{
    // 恢复菜单栏
    QWidget* mainWindow = this->window();
    if (mainWindow) {
        QList<QWidget*> menuBars = mainWindow->findChildren<QWidget*>("measurementMenuBar");
        for (QWidget* menuBar : menuBars) {
            // 无论之前的状态如何，都显示菜单栏
            menuBar->show();
            menuBar->setProperty("was_visible", false);
            LOG_INFO("测量类型选择页面：恢复菜单栏显示");
        }
    }
    QWidget::hideEvent(event);
} 