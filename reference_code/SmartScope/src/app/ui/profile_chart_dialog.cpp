#include "app/ui/profile_chart_dialog.h"
#include "qcustomplot.h" // 修正：直接包含，因为路径已在CMake中指定
#include "infrastructure/logging/logger.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QStyle>

namespace SmartScope {
namespace App {
namespace Ui {

ProfileChartDialog::ProfileChartDialog(QWidget *parent)
    : QDialog(parent)
    , m_customPlot(nullptr)
    , m_profileGraph(nullptr)
    , m_mainLayout(nullptr)
    , m_titleBar(nullptr)
    , m_titleLabel(nullptr)
    , m_closeButton(nullptr)
    , m_dragging(false)
{
    setupUi();
    setupPlot();
    LOG_INFO("ProfileChartDialog created.");
}

ProfileChartDialog::~ProfileChartDialog()
{
    LOG_INFO("ProfileChartDialog destroyed.");
}

void ProfileChartDialog::setupUi()
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(1, 1, 1, 1);
    m_mainLayout->setSpacing(0);

    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("profileTitleBar");
    m_titleBar->setFixedHeight(60);
    auto* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(20, 0, 0, 0);
    titleLayout->setSpacing(10);

    m_titleLabel = new QLabel("Profile Chart", m_titleBar);
    m_titleLabel->setObjectName("profileTitleLabel");
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_closeButton = new QPushButton(m_titleBar);
    m_closeButton->setObjectName("profileCloseButton");
    m_closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    m_closeButton->setFixedSize(48, 48);
    m_closeButton->setIconSize(QSize(24, 24));
    m_closeButton->setFlat(true);

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_closeButton);

    connect(m_closeButton, &QPushButton::clicked, this, &ProfileChartDialog::close);

    m_customPlot = new QCustomPlot(this);
    m_customPlot->setObjectName("profileCustomPlot");

    m_mainLayout->addWidget(m_titleBar);
    m_mainLayout->addWidget(m_customPlot, 1);

    setStyleSheet(R"(
        ProfileChartDialog {
            background-color: #252526;
            border-radius: 12px;
            border: 1px solid #444;
            padding: 25px;
        }
        #profileTitleBar {
            background-color: #252526;
            border-top-left-radius: 12px;
            border-top-right-radius: 12px;
            border-bottom: 1px solid #444;
        }
        #profileTitleLabel {
            color: #E0E0E0;
            background-color: transparent;
            padding: 5px;
            font-size: 20pt;
        }
        #profileCloseButton {
            background-color: #D9534F;
            color: white;
            padding: 10px 25px;
            border-radius: 8px;
            border: none;
            min-height: 45px;
            min-width: 160px;
            font-size: 18pt;
            margin: 10px 15px;
        }
        #profileCloseButton:hover {
            background-color: #C9302C;
        }
        #profileCloseButton:pressed {
            background-color: #AC2925;
        }
        #profileCustomPlot {
            background-color: #333333;
            border-bottom-left-radius: 12px;
            border-bottom-right-radius: 12px;
            border: none;
        }
    )");

    resize(1200, 900);
}

void ProfileChartDialog::setupPlot()
{
    if (!m_customPlot) return;

    m_customPlot->setBackground(Qt::transparent);

    m_profileGraph = m_customPlot->addGraph();
    QPen pen;
    pen.setColor(QColor(100, 180, 255));
    pen.setWidth(4);
    m_profileGraph->setPen(pen);
    m_profileGraph->setBrush(QBrush(QColor(100, 180, 255, 50)));

    QFont axisLabelFont = m_customPlot->xAxis->labelFont();
    axisLabelFont.setPointSize(axisLabelFont.pointSize() * 1.8);
    QFont tickLabelFont = m_customPlot->xAxis->tickLabelFont();
    tickLabelFont.setPointSize(tickLabelFont.pointSize() * 1.6);

    m_customPlot->xAxis->setLabelFont(axisLabelFont);
    m_customPlot->yAxis->setLabelFont(axisLabelFont);
    m_customPlot->xAxis->setTickLabelFont(tickLabelFont);
    m_customPlot->yAxis->setTickLabelFont(tickLabelFont);

    m_customPlot->xAxis->setLabel("Distance (pixels)");
    m_customPlot->yAxis->setLabel("Depth / Height");
    m_customPlot->xAxis->setLabelColor(Qt::white);
    m_customPlot->yAxis->setLabelColor(Qt::white);
    m_customPlot->xAxis->setBasePen(QPen(Qt::white));
    m_customPlot->yAxis->setBasePen(QPen(Qt::white));
    m_customPlot->xAxis->setTickPen(QPen(Qt::white));
    m_customPlot->yAxis->setTickPen(QPen(Qt::white));
    m_customPlot->xAxis->setSubTickPen(QPen(Qt::white));
    m_customPlot->yAxis->setSubTickPen(QPen(Qt::white));
    m_customPlot->xAxis->setTickLabelColor(Qt::white);
    m_customPlot->yAxis->setTickLabelColor(Qt::white);
    m_customPlot->xAxis->grid()->setPen(QPen(QColor(100, 100, 100), 2, Qt::DotLine));
    m_customPlot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 2, Qt::DotLine));
    m_customPlot->xAxis->grid()->setSubGridVisible(true);
    m_customPlot->yAxis->grid()->setSubGridVisible(true);
    m_customPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    m_customPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));

    m_customPlot->setInteraction(QCP::iRangeDrag, true);
    m_customPlot->setInteraction(QCP::iRangeZoom, true);
}

void ProfileChartDialog::setData(const QVector<QPointF>& profileData, const QString& title)
{
    if (!m_customPlot || !m_profileGraph) return;

    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }

    if (profileData.isEmpty()) {
        // 处理空数据的情况
        m_profileGraph->data()->clear();
        m_customPlot->xAxis->setRange(0, 10);
        m_customPlot->yAxis->setRange(0, 10);
        
        // 添加文本提示
        if (m_customPlot->itemCount() > 0) {
            m_customPlot->removeItem(0); // 移除已有文本
        }
        QCPItemText* noDataText = new QCPItemText(m_customPlot);
        noDataText->position->setCoords(5, 5);
        noDataText->setText("无剖面数据");
        noDataText->setFont(QFont("WenQuanYi Zen Hei", 24, QFont::Bold));
        noDataText->setColor(Qt::white);
        
        m_customPlot->replot();
        LOG_WARNING("剖面图表更新: 无数据");
        return;
    }

    QVector<double> xData, yData;
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::min();
    for (const auto& point : profileData) {
        xData.append(point.x());
        yData.append(point.y());
        if (point.y() < minY) minY = point.y();
        if (point.y() > maxY) maxY = point.y();
    }

    m_profileGraph->setData(xData, yData);

    m_customPlot->xAxis->rescale();
    if (minY == maxY) {
        m_customPlot->yAxis->setRange(minY - 1, maxY + 1);
    } else {
        double padding = (maxY - minY) * 0.1;
        m_customPlot->yAxis->setRange(minY - padding, maxY + padding);
    }
    m_customPlot->replot();

    LOG_INFO(QString("Profile chart updated with %1 data points. Title: %2").arg(profileData.size()).arg(title));
}

void ProfileChartDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_titleBar && m_titleBar->rect().contains(event->pos())) {
        m_dragging = true;
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    } else {
        QDialog::mousePressEvent(event);
    }
}

void ProfileChartDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    } else {
        QDialog::mouseMoveEvent(event);
    }
}

void ProfileChartDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    } else {
        QDialog::mouseReleaseEvent(event);
    }
}

} // namespace Ui
} // namespace App
} // namespace SmartScope 