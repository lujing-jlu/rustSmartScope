#include "app/ui/debug_page.h"
#include "app/ui/page_manager.h"
#include "infrastructure/logging/logger.h"
#include <QApplication>
#include <QScreen>
#include <QStyle>
#include <QTimer>

using namespace SmartScope::App::Ui;

DebugPage::DebugPage(QWidget *parent)
    : BasePage("调试页面", parent),
      m_rectifiedLeftLabel(nullptr),
      m_depthMapLabel(nullptr),
      m_predictedDepthLabel(nullptr),
      m_calibratedDepthLabel(nullptr),
      m_mainLayout(nullptr),
      m_imageLayout(nullptr),
      m_rectifiedLeftTitle(nullptr),
      m_depthMapTitle(nullptr),
      m_predictedDepthTitle(nullptr),
      m_calibratedDepthTitle(nullptr),
      m_backButton(nullptr)
{
    initContent();
}

DebugPage::~DebugPage()
{
}

void DebugPage::initContent()
{
    // 使用 BasePage 的内容布局
    m_mainLayout = getContentLayout();
    if (m_mainLayout) {
        m_mainLayout->setContentsMargins(20, 20, 20, 120);
    m_mainLayout->setSpacing(20);
    }
    
    // 创建标题
    QLabel *titleLabel = new QLabel("调试页面 - 图像处理结果", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #333; margin: 10px;");
    m_mainLayout->addWidget(titleLabel);
    
    // 创建图像显示区域
    createImageDisplayArea();
    
    // 创建底部按钮区域
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch(); // 左侧弹性空间
    
    // 创建返回按钮
    m_backButton = new QPushButton("返回3D测量", this);
    m_backButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border: none;"
        "    padding: 15px 30px;"
        "    font-size: 16px;"
        "    border-radius: 8px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #3d8b40;"
        "}"
    );
    m_backButton->setFixedSize(150, 50);
    
    // 连接返回按钮信号
    connect(m_backButton, &QPushButton::clicked, this, &DebugPage::handleBackButtonClicked);
    
    bottomLayout->addWidget(m_backButton);
    bottomLayout->addStretch(); // 右侧弹性空间
    
    m_mainLayout->addLayout(bottomLayout);
    
    LOG_INFO("调试页面初始化完成");
}

void DebugPage::createImageDisplayArea()
{
    // 创建图像布局容器
    QWidget *imageContainer = new QWidget(this);
    m_imageLayout = new QHBoxLayout(imageContainer);
    m_imageLayout->setContentsMargins(10, STATUS_BAR_HEIGHT + 10, 10, 10);
    m_imageLayout->setSpacing(15);
    
    // 创建四个图像显示区域
    // 1. 校正后的左图
    QVBoxLayout *rectifiedLeftLayout = new QVBoxLayout();
    m_rectifiedLeftTitle = new QLabel("校正后的左图", this);
    m_rectifiedLeftTitle->setAlignment(Qt::AlignCenter);
    m_rectifiedLeftTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; margin-bottom: 5px;");
    m_rectifiedLeftLabel = new ClickableImageLabel(this, 2.0/3.0);
    m_rectifiedLeftLabel->setMinimumSize(360, 540);
    m_rectifiedLeftLabel->setMaximumSize(480, 720);
    m_rectifiedLeftLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 8px; background-color: #f5f5f5;");
    m_rectifiedLeftLabel->setAlignment(Qt::AlignCenter);
    rectifiedLeftLayout->addWidget(m_rectifiedLeftTitle);
    rectifiedLeftLayout->addWidget(m_rectifiedLeftLabel);
    m_imageLayout->addLayout(rectifiedLeftLayout);
    
    // 2. 深度图
    QVBoxLayout *depthMapLayout = new QVBoxLayout();
    m_depthMapTitle = new QLabel("双目深度图", this);
    m_depthMapTitle->setAlignment(Qt::AlignCenter);
    m_depthMapTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; margin-bottom: 5px;");
    m_depthMapLabel = new ClickableImageLabel(this, 2.0/3.0);
    m_depthMapLabel->setMinimumSize(360, 540);
    m_depthMapLabel->setMaximumSize(480, 720);
    m_depthMapLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 8px; background-color: #f5f5f5;");
    m_depthMapLabel->setAlignment(Qt::AlignCenter);
    depthMapLayout->addWidget(m_depthMapTitle);
    depthMapLayout->addWidget(m_depthMapLabel);
    m_imageLayout->addLayout(depthMapLayout);
    
    // 3. 预测深度图
    QVBoxLayout *predictedDepthLayout = new QVBoxLayout();
    m_predictedDepthTitle = new QLabel("预测深度图", this);
    m_predictedDepthTitle->setAlignment(Qt::AlignCenter);
    m_predictedDepthTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; margin-bottom: 5px;");
    m_predictedDepthLabel = new ClickableImageLabel(this, 2.0/3.0);
    m_predictedDepthLabel->setMinimumSize(360, 540);
    m_predictedDepthLabel->setMaximumSize(480, 720);
    m_predictedDepthLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 8px; background-color: #f5f5f5;");
    m_predictedDepthLabel->setAlignment(Qt::AlignCenter);
    predictedDepthLayout->addWidget(m_predictedDepthTitle);
    predictedDepthLayout->addWidget(m_predictedDepthLabel);
    m_imageLayout->addLayout(predictedDepthLayout);
    
    // 4. 校准后的预测深度图
    QVBoxLayout *calibratedDepthLayout = new QVBoxLayout();
    m_calibratedDepthTitle = new QLabel("校准后的预测深度图", this);
    m_calibratedDepthTitle->setAlignment(Qt::AlignCenter);
    m_calibratedDepthTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; margin-bottom: 5px;");
    m_calibratedDepthLabel = new ClickableImageLabel(this, 2.0/3.0);
    m_calibratedDepthLabel->setMinimumSize(360, 540);
    m_calibratedDepthLabel->setMaximumSize(480, 720);
    m_calibratedDepthLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 8px; background-color: #f5f5f5;");
    m_calibratedDepthLabel->setAlignment(Qt::AlignCenter);
    calibratedDepthLayout->addWidget(m_calibratedDepthTitle);
    calibratedDepthLayout->addWidget(m_calibratedDepthLabel);
    m_imageLayout->addLayout(calibratedDepthLayout);
    
    // 将图像容器添加到主布局
    m_mainLayout->addWidget(imageContainer);
}

void DebugPage::setDebugImages(const cv::Mat &rectifiedLeftImage, 
                              const cv::Mat &depthMap,
                              const cv::Mat &predictedDepthMap,
                              const cv::Mat &calibratedPredictedDepthMap)
{
    m_rectifiedLeftImage = rectifiedLeftImage.clone();
    m_depthMap = depthMap.clone();
    m_predictedDepthMap = predictedDepthMap.clone();
    m_calibratedPredictedDepthMap = calibratedPredictedDepthMap.clone();
    
    updateImageDisplays();
}

void DebugPage::updateImageDisplays()
{
    // 更新校正后的左图
    if (!m_rectifiedLeftImage.empty()) {
        QPixmap pixmap = matToPixmap(m_rectifiedLeftImage);
        m_rectifiedLeftLabel->setPixmap(pixmap.scaled(
            m_rectifiedLeftLabel->size(), 
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        ));
    }
    
    // 统一三张深度图的颜色方案：同一归一化范围（毫米，非反色）、同一色带（TURBO）
    const double expectedMaxMm = 100.0;
    std::vector<float> allValues;
    auto appendValues = [&](const cv::Mat& depth) {
        if (depth.empty()) return;
        cv::Mat f32; depth.convertTo(f32, CV_32F);
        cv::Mat valid = (f32 > 0) & (f32 < 1e7f);
        cv::Mat clipped = cv::min(f32, (float)expectedMaxMm);
        cv::Mat vals; clipped.copyTo(vals, valid);
        if (!vals.empty()) {
            vals = vals.reshape(1, 1);
            allValues.insert(allValues.end(), (float*)vals.datastart, (float*)vals.dataend);
        }
    };
    appendValues(m_depthMap);
    appendValues(m_predictedDepthMap);
    appendValues(m_calibratedPredictedDepthMap);

    double pmin = 0.0, pmax = 0.0;
    if (!allValues.empty()) {
        std::sort(allValues.begin(), allValues.end());
        auto atQ = [&](double q){ return allValues[std::min(std::max<size_t>(0, (size_t)(q*(allValues.size()-1))), allValues.size()-1)]; };
        pmin = atQ(0.005); pmax = atQ(0.995);
        if (pmax <= pmin) { pmin = allValues.front(); pmax = allValues.back(); }
    }

    auto colorizeWithRange = [&](const cv::Mat& depth) -> cv::Mat {
        if (depth.empty()) return cv::Mat();
        cv::Mat f32; depth.convertTo(f32, CV_32F);
        cv::Mat z = f32.clone();
        if (pmax > pmin) {
            cv::min(z, (float)pmax, z);
            cv::max(z, (float)pmin, z);
        }
        double denom = (pmax > pmin) ? (pmax - pmin) : 1.0;
        cv::Mat norm = (z - pmin) * (1.0 / denom);
        cv::min(norm, 1.0, norm);
        cv::max(norm, 0.0, norm);
        cv::Mat viz8U; norm.convertTo(viz8U, CV_8U, 255.0);
        // 不使用CLAHE，避免对比度局部自适应导致“不可比”
        cv::Mat colored; cv::applyColorMap(viz8U, colored, cv::COLORMAP_TURBO);
        return colored;
    };

    // 更新深度图（图2）
    if (!m_depthMap.empty()) {
        cv::Mat colorDepthMap = colorizeWithRange(m_depthMap);
        if (!colorDepthMap.empty()) {
        QPixmap pixmap = matToPixmap(colorDepthMap);
        m_depthMapLabel->setPixmap(pixmap.scaled(
            m_depthMapLabel->size(), 
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        ));
        }
    }
    
    // 更新预测深度图（图3）
    if (!m_predictedDepthMap.empty()) {
        cv::Mat colorPredictedDepthMap = colorizeWithRange(m_predictedDepthMap);
        if (!colorPredictedDepthMap.empty()) {
        QPixmap pixmap = matToPixmap(colorPredictedDepthMap);
        m_predictedDepthLabel->setPixmap(pixmap.scaled(
            m_predictedDepthLabel->size(), 
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        ));
        }
    }
    
    // 更新校准后的预测深度图（图4）
    if (!m_calibratedPredictedDepthMap.empty()) {
        cv::Mat colorCalibratedDepthMap = colorizeWithRange(m_calibratedPredictedDepthMap);
        if (!colorCalibratedDepthMap.empty()) {
        QPixmap pixmap = matToPixmap(colorCalibratedDepthMap);
        m_calibratedDepthLabel->setPixmap(pixmap.scaled(
            m_calibratedDepthLabel->size(), 
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        ));
        }
    }
}

QPixmap DebugPage::matToPixmap(const cv::Mat &mat)
{
    if (mat.empty()) {
        return QPixmap();
    }
    
    cv::Mat rgbMat;
    if (mat.channels() == 1) {
        cv::cvtColor(mat, rgbMat, cv::COLOR_GRAY2RGB);
    } else if (mat.channels() == 3) {
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
    } else if (mat.channels() == 4) {
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGRA2RGB);
    } else {
        rgbMat = mat.clone();
    }
    
    QImage qimg(rgbMat.data, rgbMat.cols, rgbMat.rows, rgbMat.step, QImage::Format_RGB888);
    return QPixmap::fromImage(qimg);
}

cv::Mat DebugPage::depthMapToColor(const cv::Mat &depthMap)
{
    if (depthMap.empty()) {
        return cv::Mat();
    }
    cv::Mat depth32F; depthMap.convertTo(depth32F, CV_32F);
    // 逆深度 + 百分位 + CLAHE，与测量页一致
    cv::Mat validMask = (depth32F > 0) & (depth32F < 1e7f);
    cv::Mat inv = 1.0 / cv::max(depth32F, 1.0f);
    double pmin = 0, pmax = 0;
    {
        cv::Mat vals; inv.copyTo(vals, validMask);
        if (!vals.empty()) {
            vals = vals.reshape(1, 1);
            std::vector<float> v; v.assign((float*)vals.datastart, (float*)vals.dataend);
            std::sort(v.begin(), v.end());
            auto idx = [&](double q){ return std::min((size_t)std::max(0.0, q*(v.size()-1)), v.size()-1); };
            pmin = v[idx(0.005)]; pmax = v[idx(0.995)];
            if (pmax <= pmin) { pmin = v.front(); pmax = v.back(); }
        }
    }
    cv::Mat viz8U;
    if (pmax > pmin) {
        cv::Mat f = (inv - pmin) * (1.0 / (pmax - pmin));
        cv::min(f, 1.0, f); cv::max(f, 0.0, f);
        f.convertTo(viz8U, CV_8U, 255.0);
    } else {
        cv::normalize(inv, viz8U, 0, 255, cv::NORM_MINMAX, CV_8U);
    }
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8,8));
    cv::Mat vizCLAHE; clahe->apply(viz8U, vizCLAHE);
    cv::Mat colorDepthMap;
    cv::applyColorMap(vizCLAHE, colorDepthMap, cv::COLORMAP_TURBO);
    return colorDepthMap;
}

void DebugPage::showEvent(QShowEvent *event)
{
    BasePage::showEvent(event);

    // 隐藏底部导航栏，仅保留本页的“返回3D测量”按钮
    QWidget* mainWindow = this->window();
    if (mainWindow) {
        QList<QWidget*> navs = mainWindow->findChildren<QWidget*>("navigationBar");
        for (QWidget* n : navs) {
            n->hide();
            n->setVisible(false);
            n->lower();
        }
    }

    updateImageDisplays();
}

void DebugPage::hideEvent(QHideEvent *event)
{
    BasePage::hideEvent(event);
}

void DebugPage::handleBackButtonClicked()
{
    // 获取页面管理器并切换到3D测量页面
    PageManager *pageManager = qobject_cast<PageManager*>(parent());
    if (pageManager) {
        // 直接切换，由 PageManager 在进入 Debug 时已经设置过保留标志
        pageManager->switchToPage(PageType::Measurement);
    }
} 