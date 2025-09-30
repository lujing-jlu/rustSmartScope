#include "app/ui/measurement_delete_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QIcon>
#include <QSpacerItem>
#include <QStyle>
#include <QSizePolicy>
#include "infrastructure/logging/logger.h"

namespace SmartScope::App::Ui {

MeasurementDeleteDialog::MeasurementDeleteDialog(QWidget *parent)
    : QDialog(parent)
{
    // 设置为无边框工具窗口样式
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint); // 无边框，工具窗口，保持在前
    // Qt::Popup 也是一种选择，行为略有不同（点击外部通常会关闭）
    // setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);

    // 设置窗口背景透明，让 QSS 中的圆角等效果生效（如果需要的话）
    // setAttribute(Qt::WA_TranslucentBackground); // 注意: 可能需要调整 QSS

    setWindowTitle("删除测量对象");
    setMinimumSize(1000, 600); // 增加宽度，为长测量值提供更多空间
    
    // 更新样式表 - 调整颜色风格
    setStyleSheet(
        "QDialog { "
        "    background-color: #252526; "
        "    border-radius: 12px; "
        "    border: 1px solid #444; "
        "    padding: 25px; "
        "}"
        "QLabel { "
        "    color: #E0E0E0; "
        "    background-color: transparent; "
        "    padding: 5px; "
        "    font-size: 20pt; "
        "}"
        "QPushButton#closeButton { "
        "    background-color: #D9534F; "
        "    color: white; "
        "    padding: 10px 25px; "
        "    border-radius: 8px; "
        "    border: none; "
        "    min-height: 45px; "
        "    min-width: 160px; "
        "    font-size: 18pt; "
        "    margin: 10px 15px; "
        "}"
        "QPushButton#closeButton:hover { background-color: #C9302C; }"
        "QPushButton#closeButton:pressed { background-color: #AC2925; }"
        "QPushButton#deleteButton { "
        "    background-color: #555555; "
        "    color: white; "
        "    padding: 10px 25px; "
        "    border-radius: 8px; "
        "    border: none; "
        "    min-height: 45px; "
        "    min-width: 160px; "
        "    font-size: 18pt; "
        "    margin: 10px 15px; "
        "}"
        "QPushButton#deleteButton:hover { background-color: #666666; }"
        "QPushButton#deleteButton:pressed { background-color: #444444; }"
        "QScrollArea { "
        "    border: none; "
        "    background-color: #333333; "
        "    border-radius: 5px; "
        "}"
        "QWidget#scrollWidget { "
        "    background-color: #333333; "
        "}"
        "QScrollBar:vertical { "
        "    border: none; "
        "    background: #333333; "
        "    width: 18px; "
        "    margin: 0px 0px 0px 0px; "
        "    border-radius: 9px; "
        "}"
        "QScrollBar::handle:vertical { "
        "    background: #555555; "
        "    border-radius: 9px; "
        "    min-height: 40px; "
        "}"
        "QScrollBar::handle:vertical:hover { background: #666666; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "    border: none; "
        "    background: none; "
        "    height: 0px; "
        "}"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { "
        "    background: none; "
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
        "    background: none; "
        "}"
    );

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(25, 25, 25, 25); // 边距改回
    m_mainLayout->setSpacing(25); // 间距改回

    // 滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    m_scrollWidget = new QWidget(m_scrollArea);
    m_scrollWidget->setObjectName("scrollWidget");
    m_listLayout = new QVBoxLayout(m_scrollWidget);
    m_listLayout->setContentsMargins(15, 15, 15, 15); // 列表内边距改回
    m_listLayout->setSpacing(18); // 列表项间距改回
    m_listLayout->addStretch();
    m_scrollWidget->setLayout(m_listLayout);
    
    m_scrollArea->setWidget(m_scrollWidget);
    m_mainLayout->addWidget(m_scrollArea, 1);

    // 关闭按钮
    m_closeButton = new QPushButton("关闭", this);
    m_closeButton->setObjectName("closeButton"); 
    connect(m_closeButton, &QPushButton::clicked, this, &MeasurementDeleteDialog::reject);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    buttonLayout->addStretch();
    
    m_mainLayout->addLayout(buttonLayout);

    setLayout(m_mainLayout);
}

MeasurementDeleteDialog::~MeasurementDeleteDialog()
{
    LOG_INFO("MeasurementDeleteDialog销毁");
    // Qt的父子关系会自动处理子控件的删除
}

void MeasurementDeleteDialog::populateList(const QVector<MeasurementObject*>& measurements)
{
    clearList(); // 清空旧列表（包括可能的空提示和伸缩项）

    if (measurements.isEmpty()) {
        // m_scrollArea->setVisible(false); // 不再隐藏滚动区域
        LOG_INFO("测量列表为空，列表已清空，滚动区域保持可见");
        return; // 直接返回，列表区域显示为空
    }

    // // 如果之前隐藏了，现在显示滚动区域 - 这段逻辑不再需要
    // if (!m_scrollArea->isVisible()) {
    //     m_scrollArea->setVisible(true);
    // }

    int index = 1;
    for (MeasurementObject* measurement : measurements) {
        if (!measurement) continue;

        QWidget* itemWidget = new QWidget(m_scrollWidget);
        // 调整列表项样式，确保文本清晰可见
        itemWidget->setStyleSheet(
            "QWidget { background-color: #3C3C3C; border-radius: 8px; }"
            "QLabel { background-color: transparent; padding: 8px; font-size: 18pt; color: #FFFFFF; }"
            "QLabel[resultLabel=\"true\"] { font-weight: bold; color: #4CAF50; }" // 结果标签特殊样式
        );
        
        QHBoxLayout* itemLayout = new QHBoxLayout(itemWidget);
        itemLayout->setContentsMargins(15, 12, 15, 12); // 列表项内边距改回
        itemLayout->setSpacing(20); // 列表项内控件间距改回

        // 序号
        QLabel* indexLabel = new QLabel(QString::number(index) + ".", itemWidget);
        indexLabel->setFixedWidth(60); // 宽度改回
        indexLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // 类型 (只用文本)
        QLabel* typeLabel = new QLabel(measurementTypeToString(measurement->getType()), itemWidget);
        typeLabel->setFixedWidth(120); // 设置固定宽度，避免占用过多空间
        typeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 结果
        QLabel* resultLabel = new QLabel(measurement->getResult(), itemWidget);
        resultLabel->setMinimumWidth(250); // 增加最小宽度，确保长测量值能显示
        resultLabel->setMaximumWidth(400); // 设置最大宽度，避免过长
        resultLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        resultLabel->setWordWrap(true); // 允许文本换行
        resultLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); // 允许水平扩展
        resultLabel->setProperty("resultLabel", true); // 设置属性用于样式选择器

        // 删除按钮 (自定义 SVG 图标)
        QPushButton* deleteButton = new QPushButton(itemWidget);
        deleteButton->setObjectName("deleteButton");
        deleteButton->setIcon(QIcon(":/icons/delete.svg")); 
        deleteButton->setIconSize(QSize(48, 48)); // 图标尺寸改回
        deleteButton->setFixedSize(QSize(60, 60)); // 按钮大小改回
        deleteButton->setCursor(Qt::PointingHandCursor);
        deleteButton->setToolTip("删除此项");
        deleteButton->setFlat(true);
        deleteButton->setProperty("measurementPtr", QVariant::fromValue(reinterpret_cast<void*>(measurement)));
        connect(deleteButton, &QPushButton::clicked, this, &MeasurementDeleteDialog::onDeleteButtonClicked);

        itemLayout->addWidget(indexLabel);
        itemLayout->addWidget(typeLabel); // 移除拉伸因子，使用固定宽度
        itemLayout->addWidget(resultLabel, 1); // 给结果标签设置拉伸因子，让它占用主要空间
        itemLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Minimum)); // 减小弹性空间
        itemLayout->addWidget(deleteButton);

        itemWidget->setLayout(itemLayout);
        // 插入到伸缩项之前
        m_listLayout->insertWidget(m_listLayout->count() - 1, itemWidget);
        index++;
    }
}

void MeasurementDeleteDialog::clearList()
{
    // 从布局中移除所有项目，包括伸缩项和可能的空列表提示
    QLayoutItem* item;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater(); // 安全删除控件
        }
        // 不直接删除 item，因为 takeAt(0) 已经将所有权转移
        delete item; // 删除布局项本身
    }
    // 重新添加伸缩项，确保新项从顶部开始
    m_listLayout->addStretch(); 
}

void MeasurementDeleteDialog::onDeleteButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button) {
        void* ptr = button->property("measurementPtr").value<void*>();
        MeasurementObject* measurement = reinterpret_cast<MeasurementObject*>(ptr);
        if (measurement) {
            LOG_INFO(QString("请求删除测量对象 - 类型: %1, 结果: %2")
                         .arg(static_cast<int>(measurement->getType())) // 记录类型枚举值
                         .arg(measurement->getResult()));         // 记录结果字符串
            emit measurementToDelete(measurement);
            
            // 可选: 立即从对话框的视觉列表中移除该项，或者由 MeasurementPage 更新后统一处理
            // QWidget* itemWidget = qobject_cast<QWidget*>(sender()->parent());
            // if (itemWidget) {
            //     itemWidget->hide();
            //     itemWidget->deleteLater(); 
            // }
        } else {
            LOG_WARNING("无效的删除按钮点击事件");
        }
    }
}

QString MeasurementDeleteDialog::measurementTypeToString(MeasurementType type)
{
    switch (type) {
        case MeasurementType::Length:       return "长度";
        case MeasurementType::PointToLine:  return "点到线";
        case MeasurementType::Depth:        return "深度(点到面)";
        case MeasurementType::Area:         return "面积";
        case MeasurementType::Polyline:     return "折线";
        case MeasurementType::Profile:      return "剖面";
        case MeasurementType::RegionProfile: return "区域剖面";
        case MeasurementType::MissingArea:  return "缺失面积";  // 添加缺失面积类型
        default:                            return "未知类型";
    }
}

QString MeasurementDeleteDialog::measurementTypeToIconPath(MeasurementType type)
{
    // 这里可以根据类型返回图标路径
    // 例如: return ":/icons/length.svg";
    return ""; // 暂不使用图标
}

} // namespace SmartScope::App::Ui 