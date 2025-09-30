#include "app/ui/measurement_page.h"
#include "app/ui/measurement_menu.h"
#include "core/camera/camera_correction_manager.h"
#include "core/camera/camera_correction_factory.h"
#include "app/ui/measurement_delete_dialog.h" // <-- 包含新头文件
#include "app/ui/utils/dialog_utils.h" // <-- 包含新的头文件
#include "mainwindow.h" // <-- 添加主窗口头文件
#include "app/ui/toolbar.h" // <-- 添加工具栏头文件
#include "app/ui/home_page.h" // <-- 添加HomePage头文件，解决前向声明问题
#include <QtWidgets>
#include <QDebug>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStyle>
#include <QDateTime>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QSplitter>
#include <QPushButton>
#include <QTimer>
#include "infrastructure/logging/logger.h"
#include "app/ui/toast_notification.h"
#include "app/ui/page_manager.h"
#include "inference/inference_service.hpp"
#include "app/ui/measurement_object.h" // 添加 MeasurementObject 头文件
#include "qcustomplot.h" // Correct include path
#include "app/utils/screenshot_manager.h"
#include "app/ui/profile_chart_manager.h" // 添加 ProfileChartManager 头文件
#include "app/ui/image_interaction_manager.h"
#include "app/ui/profile_chart_dialog.h" // 添加 ProfileChartDialog 头文件
#include "stereo_depth/comprehensive_depth_processor.hpp"
#include <cmath>

using namespace SmartScope::App::Image;
// 移除旧的点云生成器依赖
// using namespace SmartScope::App::Measurement; // <-- Add namespace for PointCloudGenerator
// 定义客户端ID
const std::string MeasurementPage::CLIENT_ID = "MeasurementPage";

// MeasurementPage的实现从这里开始
MeasurementPage::MeasurementPage(QWidget *parent)
    : BasePage("3D测量", parent),
      m_leftImageLabel(nullptr),
      m_rightImageLabel(nullptr),
      m_depthImageLabel(nullptr),
      m_disparityImageLabel(nullptr),
      m_pointCloudWidget(nullptr),
      m_menuBar(nullptr),
      m_leftCameraId(""),
      m_rightCameraId(""),
      m_imagesReady(false),
      m_inferenceService(SmartScope::InferenceService::instance()),
      m_inferenceInitialized(false),
      m_measurementState(MeasurementState::Idle),  // 初始化测量状态为空闲
      m_measurementManager(nullptr),
      m_stateManager(nullptr),
      m_typeSelectionPage(nullptr),
      m_addMeasurementButton(nullptr),
      m_magnifierManager(new MagnifierManager(this)), // 初始化放大镜管理器，但不显示
      m_imageInteractionManager(nullptr), // 初始化为 nullptr，在 initMeasurementFeatures 中创建
      m_correctionManager(nullptr), // 将在initMeasurementFeatures中初始化
      m_measurementCalculator(new SmartScope::App::Measurement::MeasurementCalculator()), // <-- 初始化测量计算器
      m_pointCloudRenderer(nullptr), // 初始化为 nullptr
      m_measurementRenderer(new SmartScope::App::Ui::MeasurementRenderer()), // 在初始化列表中创建
      m_deleteDialog(nullptr), // <-- 初始化对话框指针
       /* m_pointCloudGenerator(new PointCloudGenerator()), */ // 不再使用生成器
      m_profileChartPlot(nullptr),
      m_screenshotManager(new SmartScope::App::Utils::ScreenshotManager(this)),
      m_profileChartManager(new SmartScope::App::Ui::ProfileChartManager(this)),
      m_profileDialog(nullptr)
{
    initContent();
    createMenuBar();
    initMeasurementFeatures();
    
    // 相机校正管理器将在initMeasurementFeatures中初始化
    
    // 检查模型文件是否存在并初始化推理服务
            QString modelPath = QCoreApplication::applicationDirPath() + "/models/depth_anything_v2_vits.rknn";
    // 检查模型文件是否存在
    QFile modelFile(modelPath);
    if (!modelFile.exists()) {
        LOG_ERROR(QString("推理模型文件不存在: %1").arg(modelPath));
        
        // 尝试查找其他可能的模型路径
        QStringList possibleModelPaths = {
            "../models/depth_anything_v2_vits.rknn",
            "../点云渲染管理器",
            "./models/depth_anything_v2_vits.rknn",
            QDir::currentPath() + "/models/depth_anything_v2_vits.rknn",
            QDir::currentPath() + "/../models/depth_anything_v2_vits.rknn"
        };
        
        for (const auto& path : possibleModelPaths) {
            LOG_INFO(QString("尝试寻找模型: %1").arg(path));
            if (QFile::exists(path)) {
                modelPath = path;
                LOG_INFO(QString("找到模型文件: %1").arg(path));
                break;
            }
        }
    }

    if (m_inferenceService.initialize(modelPath)) {
        m_inferenceInitialized = true;
        LOG_INFO(QString("推理服务初始化成功, 使用模型: %1").arg(modelPath));
        
        // 连接推理完成信号
        disconnect(&m_inferenceService, &SmartScope::InferenceService::inferenceCompleted,
                 this, &MeasurementPage::handleInferenceResult); // 先断开可能的旧连接
        
        bool connected = connect(&m_inferenceService, &SmartScope::InferenceService::inferenceCompleted,
                               this, &MeasurementPage::handleInferenceResult, 
                               Qt::QueuedConnection); // 使用队列连接确保线程安全
        
        if (connected) {
            LOG_INFO("推理完成信号已成功连接到handleInferenceResult槽函数");
        } else {
            LOG_ERROR("推理完成信号连接失败");
        }
    } else {
        m_inferenceInitialized = false;
        LOG_ERROR(QString("推理服务初始化失败, 模型路径: %1").arg(modelPath));
    }

    // 初始化 PointCloudRenderer - 确保 m_pointCloudWidget 已创建
    if (m_pointCloudWidget) {
        m_pointCloudRenderer = new SmartScope::App::Ui::PointCloudRenderer(m_pointCloudWidget);
    } else {
        LOG_ERROR("PointCloudGLWidget 未初始化，无法创建 PointCloudRenderer");
        // 可以在这里添加错误处理，例如禁用相关功能
    }
    
    // 添加截图按钮到工具栏
    // QAction *screenshotAction = new QAction(QIcon(":/icons/screenshot.png"), tr("截图"), this);
    // screenshotAction->setStatusTip(tr("截取当前测量画面"));
    // connect(screenshotAction, &QAction::triggered, this, &MeasurementPage::onScreenshot);
    // m_toolBar->addAction(screenshotAction); // 注释掉，避免未声明成员报错
}

MeasurementPage::~MeasurementPage()
{
    LOG_INFO("销毁3D测量页面");
    delete m_menuBar;
    delete m_typeSelectionPage;
    delete m_magnifierManager;
    // m_correctionManager 是智能指针，会自动删除
    delete m_measurementCalculator;
    /* 不再使用 PointCloudGenerator */
    delete m_measurementManager; 
    delete m_stateManager;
    delete m_measurementRenderer;
    delete m_deleteDialog;
    // delete m_profileChartDialog; // <-- 移除
    // QCustomPlot 会作为 pointCloudContainer 的子控件被自动删除，无需手动 delete m_profileChartPlot
    delete m_profileChartManager;
}

void MeasurementPage::initContent()
{
    LOG_INFO("初始化测量页面内容");
    
    // 在 initContent 中创建 m_pointCloudContainer 的变量引用，方便后续添加测量控件
    QWidget *pointCloudContainer = nullptr;
    
    // 设置内容区域的边距，避开状态栏和菜单栏
    m_contentWidget->setContentsMargins(0, STATUS_BAR_HEIGHT, 0, 160);  // 上边距避开状态栏，下边距160px避开菜单栏
    
    // 创建水平布局替代分割器
    QHBoxLayout *horizontalLayout = new QHBoxLayout(m_contentWidget);
    horizontalLayout->setContentsMargins(10, 10, 10, 10);  // 设置整体边距
    horizontalLayout->setSpacing(10);  // 设置区域之间的间距
    
    // 创建左相机区域
    QWidget *leftArea = new QWidget(m_contentWidget);
    leftArea->setObjectName("leftImageArea");
    leftArea->setStyleSheet(
        "QWidget#leftImageArea {"
        "   background-color: #111111;"
        "   border: 2px solid #444444;"  // 添加明显的边框
        "   border-radius: 5px;"        // 稍微圆角化边框
        "}"
    );
    
    // 创建垂直布局
    QVBoxLayout *leftLayout = new QVBoxLayout(leftArea);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0); // 无间距
    
    // 使用ClickableImageLabel显示左相机图像，设置宽高比为2:3以匹配裁剪后的图像尺寸(720x1080)
    m_leftImageLabel = new ClickableImageLabel(leftArea, 2.0/3.0);  // 修改为2:3比例，与裁剪后图像匹配
    m_leftImageLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #111111;"
        "   border: none;"
        "   margin: 0px;"
        "   padding: 0px;"
        "}"
    );
    m_leftImageLabel->setClickEnabled(false); // 默认禁用点击，只在长度测量模式时启用
    // 修正：连接正确的带坐标的信号 clicked
    connect(m_leftImageLabel, &ClickableImageLabel::clicked, 
            this, &MeasurementPage::handleImageClicked);
    
    // 将左图像添加到左侧垂直布局中，占据整个区域
    leftLayout->addWidget(m_leftImageLabel, 1);  // 占比因子为1，表示填满整个区域
    
    // 创建右相机区域 - 隐藏不显示，但保留代码以便将来可能需要
    QWidget *rightArea = new QWidget(m_contentWidget);
    rightArea->setObjectName("rightImageArea");
    rightArea->setVisible(false);  // 设置为不可见
    rightArea->hide();  // 确保隐藏
    
    QVBoxLayout *rightLayout = new QVBoxLayout(rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    
    m_rightImageLabel = new QLabel(rightArea);
    m_rightImageLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(m_rightImageLabel);
    
    // 创建点云显示区域
    m_pointCloudWidget = new PointCloudGLWidget();
    m_pointCloudWidget->setPointSize(3.0f);  // 设置点大小
    m_pointCloudWidget->setShowAxes(false);  // 不显示坐标轴
    m_pointCloudWidget->setObjectName("pointCloudArea");
    m_pointCloudWidget->setStyleSheet(
        "QWidget#pointCloudArea {"
        "   background-color: #D9D9D9;"
        "   border: 2px solid #AAAAAA;"
        "   border-radius: 5px;"
        "}"
    );
    m_pointCloudWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 创建点云容器
    pointCloudContainer = new QWidget(m_contentWidget);
    pointCloudContainer->setObjectName("pointCloudContainer");
    pointCloudContainer->setStyleSheet(
        "QWidget#pointCloudContainer {"
        "   background-color: #D9D9D9;" // 背景色可以考虑移除或调整
        "   border: 2px solid #AAAAAA;"
        "   border-radius: 5px;"
        "}"
    );
    
    // 使用QSplitter替代垂直布局，使3D视图和剖面图能够平分右侧区域
    QSplitter *splitter = new QSplitter(Qt::Vertical, pointCloudContainer);
    splitter->setObjectName("pointCloudSplitter");
    splitter->setChildrenCollapsible(false); // 防止子控件被完全折叠
    
    // 禁用拖动功能
    splitter->setHandleWidth(1); // 设置极小的手柄宽度
    
    // 设置分隔条样式，使其几乎不可见
    splitter->setStyleSheet(
        "QSplitter::handle {"
        "   background-color: #AAAAAA;"
        "   height: 1px;"
        "}"
    );
    
    // 创建并设置容器布局
    QVBoxLayout *pointCloudLayout = new QVBoxLayout(pointCloudContainer);
    pointCloudLayout->setContentsMargins(0, 0, 0, 0); // 内部无边距
    pointCloudLayout->setSpacing(0); // 无间距
    pointCloudLayout->addWidget(splitter); // 添加分割器到容器
    
    // 添加3D点云控件到分割器
    splitter->addWidget(m_pointCloudWidget);
    
    // 创建嵌入式剖面图表控件
    m_profileChartPlot = new QCustomPlot(splitter);
    m_profileChartPlot->setObjectName("profileChartPlot");
    
    // 添加剖面图表控件到分割器
    splitter->addWidget(m_profileChartPlot);
    
    // 使用 ProfileChartManager 初始化图表
    if (m_profileChartManager) {
        m_profileChartManager->initializeChart(m_profileChartPlot);
    } else {
        LOG_ERROR("ProfileChartManager未初始化，无法正确设置图表样式");
        // 添加默认图表配置作为备选
        m_profileChartPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        m_profileChartPlot->setVisible(false);
    }
    
    // 设置初始比例为1:1（平分区域）
    QList<int> sizes;
    sizes << 500 << 500; // 设置固定值，确保1:1比例
    splitter->setSizes(sizes);
    
    // 固定分割位置，禁止用户拖动
    // 通过创建一个特殊的QSplitterHandle子类来禁用移动
    for(int i = 1; i < splitter->count(); i++) {
        QSplitterHandle *handle = splitter->handle(i);
        if(handle) {
            handle->installEventFilter(this); // 使用Page自身的eventFilter来阻止拖动
            // 设置鼠标悬停时不显示拖动光标
            handle->setCursor(Qt::ArrowCursor);
        }
    }
    
    // 创建深度图区域 - 隐藏不显示
    QWidget *depthArea = new QWidget(m_contentWidget);
    depthArea->setObjectName("depthImageArea");
    depthArea->setVisible(false);  // 设置为不可见
    depthArea->hide();  // 确保隐藏
    
    QVBoxLayout *depthLayout = new QVBoxLayout(depthArea);
    depthLayout->setContentsMargins(0, 0, 0, 0);
    depthLayout->setSpacing(0);
    
    m_depthImageLabel = new QLabel(depthArea);
    m_depthImageLabel->setAlignment(Qt::AlignCenter);
    m_depthImageLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #111111;"
        "   border: none;"
        "   margin: 0px;"
        "   padding: 0px;"
        "}"
    );
    m_depthImageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    depthLayout->addWidget(m_depthImageLabel, 1);  // 1表示拉伸因子
    
    // --- 将 pointCloudContainer 存为成员变量 ---
    m_pointCloudContainer = pointCloudContainer; // <-- 添加赋值
    // --- 结束添加 ---
    
    // 添加区域到水平布局 - 只显示左图像和点云区域
    horizontalLayout->addWidget(leftArea, 15);
    horizontalLayout->addWidget(m_pointCloudContainer, 25); // <-- 使用成员变量
    
    // 延迟初始化放大镜 - 捕获 leftArea 指针
    QTimer::singleShot(1000, this, [this, leftArea]() { // <-- 捕获 leftArea
        // 这里使用m_magnifierManager来创建放大镜
        if (m_contentWidget && m_leftImageLabel && m_magnifierManager) {
            // 基于实际控件宽度计算左区域的比例
            int leftWidth = leftArea->width();
            int totalWidth = m_contentWidget->width();
            float leftAreaRatio = 0.0f;
            if (totalWidth > 0) {
                 leftAreaRatio = static_cast<float>(leftWidth) / totalWidth;
                 // 保存leftAreaRatio到成员变量
                 m_leftAreaRatio = leftAreaRatio;
                 LOG_INFO(QString("Magnifier init: leftWidth=%1, totalWidth=%2, leftAreaRatio=%3")
                          .arg(leftWidth).arg(totalWidth).arg(leftAreaRatio, 0, 'f', 3));
            } else {
                 LOG_WARNING("Content widget width is zero, cannot calculate left area ratio for magnifier.");
                 // 可以在这里设置一个默认值或尝试稍后重新计算
            }
            
            // TODO: 确认 m_magnifierManager 如何使用这个比例，或者它是否需要实际的 Rect。
            // 假设放大镜管理器会在后续步骤中使用这个（现在正确的）比例或相关信息。
            // 例如: m_magnifierManager->updateTargetAreaGeometry(...);

            // 创建放大镜，但不显示，也不启用
            m_magnifierManager->createMagnifier(m_contentWidget, m_leftImageLabel, leftAreaRatio);
            m_magnifierManager->hideMagnifier(); // 确保隐藏放大镜
            m_magnifierManager->setEnabled(false); // 默认不启用
            
            LOG_INFO("放大镜初始化完成 - 默认隐藏状态");
        }
    });
    
    // 应用水平布局到内容区域
    m_contentLayout->addLayout(horizontalLayout);
    
    // 不再硬编码相机ID，改为在使用时从PageManager获取
    m_leftCameraId = "";
    m_rightCameraId = "";
    
    LOG_INFO("3D测量页面内容初始化完成");
}

void MeasurementPage::createMenuBar()
{
    // 创建菜单栏 - 确保菜单栏直接附加到主窗口而不是内容区域
    QWidget* mainWindow = this->window();
    QWidget* parent = mainWindow ? mainWindow : this;
    
    m_menuBar = new MeasurementMenuBar(parent);
    m_menuBar->setObjectName("3DMeasurementMenuBar");  // 设置唯一的对象名，方便查找和调试
    
    // 添加菜单按钮
    MeasurementMenuButton *homeButton = m_menuBar->addButton(":/icons/home.svg", "");  // 移除文字
    // 视图按钮已移除
    // 注释按钮已移除
    MeasurementMenuButton *undoButton = m_menuBar->addButton(":/icons/undo.svg", "撤回");
    MeasurementMenuButton *deleteButton = m_menuBar->addButton(":/icons/delete.svg", "删除");
    m_addMeasurementButton = m_menuBar->addButton(":/icons/plus.svg", "测量");
    m_finishButton = m_menuBar->addButton(":/icons/check.svg", "完成");  // 保存完成按钮的引用
    
	// 添加调试按钮（默认隐藏，由设置页控制显示）
	m_debugButton = m_menuBar->addButton(":/icons/setting.svg", "调试");
	if (m_debugButton) m_debugButton->setVisible(false);
    
	// 深度模式切换按钮已移除（固定使用单目校准点云）
    
    // 添加截图按钮
    // MeasurementMenuButton *screenshotButton = m_menuBar->addButton(":/icons/screenshot.svg", "截图");
    // connect(screenshotButton, &MeasurementMenuButton::clicked, this, &MeasurementPage::onScreenshot);
    
    MeasurementMenuButton *backButton = m_menuBar->addButton(":/icons/back.svg", "");  // 移除文字
    
    // 连接按钮信号
    connect(homeButton, &MeasurementMenuButton::clicked, this, [this]() {
		LOG_INFO("点击主页按钮 - 弹窗确认后返回首页");
		PageManager* pageManager = qobject_cast<PageManager*>(parentWidget());
		if (!pageManager) return;
		bool hasData = (m_measurementManager && !m_measurementManager->getMeasurements().isEmpty());
		QString text = hasData
			? QString("当前页面有测量结果，返回主页将放弃未保存的测量，是否继续？")
			: QString("确定要返回主页吗？");
		QMessageBox::StandardButton reply = SmartScope::App::Ui::DialogUtils::showStyledConfirmationDialog(
			this,
			"确认返回",
			text,
			"返回主页",
			"取消"
		);
		if (reply != QMessageBox::Yes) return;
		if (hasData) {
			m_measurementManager->clearMeasurements();
			redrawMeasurementsOnLabel();
			updatePointCloudMeasurements();
			update();
		}
            pageManager->switchToPage(PageType::Home);
    });
    
    // 视图和注释按钮已移除，相关连接代码也已移除
    
    // 撤回按钮防重复触发标志
    static bool isUndoInProgress = false;
    
    connect(undoButton, &MeasurementMenuButton::clicked, this, [this]() {
        LOG_INFO("点击撤回按钮 - 开始执行撤回操作");

        // 防止重复触发
        if (isUndoInProgress) {
            LOG_INFO("撤回操作正在进行中，忽略重复触发");
            return;
        }

        isUndoInProgress = true;

        // 记录当前鼠标位置，方便调试
        QPoint mousePos = QCursor::pos();
        LOG_INFO(QString("当前鼠标位置: (%1, %2)").arg(mousePos.x()).arg(mousePos.y()));

        // 检查是否正在进行测量操作
        if (m_stateManager && m_stateManager->getMeasurementMode() != MeasurementMode::View) {
            LOG_INFO("正在进行测量操作，取消当前测量");
            resetMeasurementState();
            showToast(this, "已取消当前测量", 1500);
        } else {
            // 检查是否可以撤销已完成的测量
            if (m_measurementManager && m_measurementManager->canUndo()) {
                LOG_INFO(QString("调用管理器undo方法执行撤销操作"));

                // 使用标准的undo方法进行撤销，而不是直接删除最后一个
                bool undoResult = m_measurementManager->undo();
                LOG_INFO(QString("撤销结果: %1").arg(undoResult ? "成功" : "失败"));

                // 显示通知
                showToast(this, "撤销成功", 1500);
                LOG_INFO("撤回操作执行完成");
            } else {
                LOG_INFO("没有可撤销的操作");
                showToast(this, "没有可撤销的操作", 1500);
            }
        }

        // 延迟重置标志，以防止快速点击
        QTimer::singleShot(300, []() {
            isUndoInProgress = false;
            LOG_INFO("撤回操作处理完成，重置触发标志");
        });
    });
    
    // --- 修改：连接删除按钮到新的槽函数 ---
    connect(deleteButton, &MeasurementMenuButton::clicked, this, &MeasurementPage::openDeleteMeasurementDialog);
    // --- 结束修改 ---
    
    connect(m_addMeasurementButton, &MeasurementMenuButton::clicked, this, &MeasurementPage::openMeasurementTypeSelection);
    
    connect(m_finishButton, &MeasurementMenuButton::clicked, this, &MeasurementPage::completeMeasurementOperation);
    
    // 连接调试按钮
	connect(m_debugButton, &MeasurementMenuButton::clicked, this, [this]() {
        LOG_INFO("点击调试按钮");
        // 获取页面管理器
        PageManager *pageManager = qobject_cast<PageManager*>(parentWidget());
        if (pageManager) {
            // 获取调试页面并设置图像
            SmartScope::App::Ui::DebugPage *debugPage = pageManager->getDebugPage();
            if (debugPage) {
                // 从推理服务获取最新的推理结果
                SmartScope::InferenceService& inferenceService = SmartScope::InferenceService::instance();
                
				// 设置调试图像，使用3D测量入口"已算好的缓存结果"（必须一致，不得在调试页重新计算）
				cv::Mat predictedDepthMap = m_monoDepthRaw.clone();
				cv::Mat calibratedPredictedDepthMap = m_monoDepthCalibrated.clone();
				cv::Mat stereoDepthForDebug;
				auto* processor = inferenceService.getComprehensiveProcessor();

				// 双目深度用于图2，可从处理器缓存/结果或 m_depthMap 获取，但不重新计算单目
				if (processor) {
					cv::Mat cachedStereo = processor->getIntermediateResult("stereo_depth");
					if (!cachedStereo.empty()) stereoDepthForDebug = cachedStereo.clone();
				}
				if (stereoDepthForDebug.empty() && !m_disparityMap.empty() && processor) {
					cv::Mat disp32; m_disparityMap.convertTo(disp32, CV_32F);
					auto stereoHelper = m_correctionManager ? m_correctionManager->getStereoCalibrationHelper() : nullptr;
					cv::Mat Q = (stereoHelper ? stereoHelper->getQMatrix() : cv::Mat());
					cv::Mat z = processor->depthFromDisparity(disp32, Q);
					if (!z.empty()) stereoDepthForDebug = z;
				}
				if (stereoDepthForDebug.empty() && !m_depthMap.empty()) {
					stereoDepthForDebug = m_depthMap.clone();
				}

				// 回退：仅单目预测
				if (predictedDepthMap.empty()) {
					if (m_leftRectifiedImage.empty()) {
						LOG_WARNING("左校正图为空，跳过单目预测计算");
						predictedDepthMap = cv::Mat();
					} else if (processor) {
						// 使用与推理一致的3:4裁剪窗作为单目输入
						int ow = m_leftRectifiedImage.cols;
						int oh = m_leftRectifiedImage.rows;
						cv::Rect cropROI(0,0,ow,oh);
						int targetW = (oh * 3) / 4;
						if (targetW > ow) {
							int targetH = (ow * 4) / 3;
							int y0 = std::max(0, (oh - targetH) / 2);
							cropROI = cv::Rect(0, y0, ow, std::min(targetH, oh));
						} else {
							int x0 = std::max(0, (ow - targetW) / 2);
							cropROI = cv::Rect(x0, 0, std::min(targetW, ow), oh);
						}
						cv::Mat leftCropped = m_leftRectifiedImage(cropROI).clone();
						predictedDepthMap = processor->computeMonoDepthOnly(leftCropped);
					} else {
						LOG_WARNING("综合深度处理器不可用，无法计算单目深度");
						predictedDepthMap = cv::Mat();
					}
				}

				// 回退：优先使用缓存的校准后单目
				if (calibratedPredictedDepthMap.empty()) {
					if (!m_monoDepthCalibrated.empty()) {
						calibratedPredictedDepthMap = m_monoDepthCalibrated.clone();
					} else if (processor) {
						calibratedPredictedDepthMap = processor->getIntermediateResult("calibrated");
					}
				}
				
				// 为第二张图确定双目深度：优先处理器缓存 -> 刚算的 comp -> 由视差+Q快速还原 -> 兜底使用 m_depthMap
				if (stereoDepthForDebug.empty() && processor) {
					cv::Mat cachedStereo = processor->getIntermediateResult("stereo_depth");
					if (!cachedStereo.empty()) stereoDepthForDebug = cachedStereo.clone();
				}
				if (stereoDepthForDebug.empty() && processor && !m_disparityMap.empty()) {
					cv::Mat disp32; m_disparityMap.convertTo(disp32, CV_32F);
					auto stereoHelper = m_correctionManager ? m_correctionManager->getStereoCalibrationHelper() : nullptr;
					cv::Mat Q = (stereoHelper ? stereoHelper->getQMatrix() : cv::Mat());
					cv::Mat z = processor->depthFromDisparity(disp32, Q);
					if (!z.empty()) stereoDepthForDebug = z;
				}
				if (stereoDepthForDebug.empty() && !m_depthMap.empty()) {
					stereoDepthForDebug = m_depthMap.clone();
				}
				
				// 构造 filtered 双目深度（不做鲁棒裁剪，不清理极值，仅使用有效掩码）
				cv::Mat stereoDepthFilteredForDebug = stereoDepthForDebug;
				if (!stereoDepthForDebug.empty() && processor) {
					// 计算与推理一致的中心3:4裁剪ROI（基于校正后的左图尺寸）
					cv::Rect cropROI(0, 0, m_leftRectifiedImage.cols, m_leftRectifiedImage.rows);
					if (!m_leftRectifiedImage.empty()) {
						int ow = m_leftRectifiedImage.cols;
						int oh = m_leftRectifiedImage.rows;
						int targetW = (oh * 3) / 4;
						if (targetW > ow) {
							int targetH = (ow * 4) / 3;
							int y0 = std::max(0, (oh - targetH) / 2);
							cropROI = cv::Rect(0, y0, ow, std::min(targetH, oh));
						} else {
							int x0 = std::max(0, (ow - targetW) / 2);
							cropROI = cv::Rect(x0, 0, std::min(targetW, ow), oh);
						}
					}

					cv::Mat disparityForMask;
					// 优先使用本地缓存视差，其次处理器缓存
					if (!m_disparityMap.empty()) disparityForMask = m_disparityMap;
					else disparityForMask = processor->getIntermediateResult("disparity");

					// 若视差图为裁剪后的尺寸而双目深度仍为全尺寸，则先裁剪双目深度到相同ROI
					if (!disparityForMask.empty() && !stereoDepthForDebug.empty()) {
						if (!m_leftRectifiedImage.empty() && m_leftRectifiedImage.size() == stereoDepthForDebug.size()) {
							if (disparityForMask.cols == cropROI.width && disparityForMask.rows == cropROI.height) {
								stereoDepthForDebug = stereoDepthForDebug(cropROI).clone();
							}
						}
					}

					cv::Mat validMask;
					if (!disparityForMask.empty()) {
						cv::Mat disp32; disparityForMask.convertTo(disp32, CV_32F);
						if (disp32.size() == stereoDepthForDebug.size()) {
							validMask = (disp32 > 0) & (stereoDepthForDebug > 0) & (stereoDepthForDebug < 1e7f);
						} else {
							// 回退：仅使用双目深度自身构造有效掩码，避免尺寸不一致导致异常
							validMask = (stereoDepthForDebug > 0) & (stereoDepthForDebug < 1e7f);
						}
					}
					cv::Mat filtered = processor->filterDepth(stereoDepthForDebug, validMask);
					if (!filtered.empty()) stereoDepthFilteredForDebug = filtered;
				}
				
				// 左图也按3:4裁剪后再显示
				cv::Mat leftCroppedForDebug = m_leftRectifiedImage;
				if (!m_leftRectifiedImage.empty()) {
					int ow = m_leftRectifiedImage.cols;
					int oh = m_leftRectifiedImage.rows;
					int targetW = (oh * 3) / 4;
					if (targetW > ow) {
						int targetH = (ow * 4) / 3;
						int y0 = std::max(0, (oh - targetH) / 2);
						leftCroppedForDebug = m_leftRectifiedImage(cv::Rect(0, y0, ow, std::min(targetH, oh))).clone();
					} else {
						int x0 = std::max(0, (ow - targetW) / 2);
						leftCroppedForDebug = m_leftRectifiedImage(cv::Rect(x0, 0, std::min(targetW, ow), oh)).clone();
					}
				}
                
                debugPage->setDebugImages(
					leftCroppedForDebug,  // 3:4 裁剪后的左图
					stereoDepthFilteredForDebug,   // 过滤后的双目深度（已对齐3:4时裁剪）
					predictedDepthMap,     // 单目预测深度图（按3:4输入）
                    calibratedPredictedDepthMap // 校准后的单目深度图
                );
            }
            pageManager->switchToPage(PageType::Debug);
        }
    });
    
	// 深度模式切换已禁用
    
    // 将返回按钮连接到新的智能处理槽
    connect(backButton, &MeasurementMenuButton::clicked, this, &MeasurementPage::handleIntelligentBack); // <--- 新的连接
    
    // 设置菜单栏位置
    updateLayout();
    
    // 默认隐藏菜单栏，等显示页面时再显示
    m_menuBar->hide();
    
    LOG_INFO("3D测量菜单栏创建完成");
}

void MeasurementPage::updateLayout()
{
    if (m_menuBar) {
        // 计算菜单栏位置（底部居中）
        int menuBarWidth = m_menuBar->width();
        int menuBarHeight = m_menuBar->height();
        
        // 获取窗口宽度 - 使用根窗口的宽度
        QWidget* mainWindow = this->window();
        int windowWidth = mainWindow ? mainWindow->width() : parentWidget()->width();
        int windowHeight = mainWindow ? mainWindow->height() : parentWidget()->height();
        
        // 确保菜单栏不超过窗口宽度
        if (menuBarWidth > windowWidth - 40) {
            menuBarWidth = windowWidth - 40;
        }
        
        // 放置在靠近底部的位置
        // 注意：在此页面中导航栏是隐藏的，所以不需要为它留出空间
        int menuBarX = (windowWidth - menuBarWidth) / 2;
        int menuBarY = windowHeight - menuBarHeight;
        
        // 设置菜单栏位置
        m_menuBar->setGeometry(menuBarX, menuBarY, menuBarWidth, menuBarHeight);
        LOG_INFO(QString("更新菜单栏位置: (%1, %2) 尺寸: %3x%4").arg(menuBarX).arg(menuBarY).arg(menuBarWidth).arg(menuBarHeight));
    }
    
    // 重新计算左区域比例，用于放大镜定位
    if (m_contentWidget && m_leftImageLabel && m_leftImageLabel->parentWidget()) {
        int leftWidth = m_leftImageLabel->parentWidget()->width();
        int totalWidth = m_contentWidget->width();
        if (totalWidth > 0) {
            // 更新左区域比例
            m_leftAreaRatio = static_cast<float>(leftWidth) / totalWidth;
            LOG_INFO(QString("更新左区域比例: leftWidth=%1, totalWidth=%2, leftAreaRatio=%3")
                    .arg(leftWidth).arg(totalWidth).arg(m_leftAreaRatio, 0, 'f', 3));
        }
    }
}

void MeasurementPage::showEvent(QShowEvent *event)
{
    BasePage::showEvent(event);
    
    // 在页面显示时从PageManager获取最新的相机ID
    PageManager* pageManager = qobject_cast<PageManager*>(parentWidget());
    if (pageManager) {
        HomePage* homePage = pageManager->getHomePage();
        if (homePage) {
            m_leftCameraId = homePage->getLeftCameraId();
            m_rightCameraId = homePage->getRightCameraId();
            LOG_INFO(QString("页面显示时更新相机ID - 左: %1, 右: %2").arg(m_leftCameraId).arg(m_rightCameraId));
        }
    }
    
    // 在页面显示时初始化工具栏按钮
    initToolBarButtons();

	// 固定使用图4（单目校准）作为点云深度来源
	setDepthMode(SmartScope::InferenceService::DepthMode::MONO_CALIBRATED);
	
	// 进入页面时清空点云显示，避免显示上次残留（除非明确要求保留）
	if (m_pointCloudWidget && !m_skipClearOnNextShow) {
		m_pointCloudWidget->clearGeometryObjects();
		m_pointCloudWidget->clearPointCloud();
		m_pointCloudWidget->update();
		LOG_INFO("3D测量页面显示 - 已清空点云显示");
	}
	m_skipClearOnNextShow = false;
    
    // 显示校正、裁剪后的图像（推理输入图像）
    if (!m_inferenceInputLeftImage.empty() && m_leftImageLabel) {
        QImage qImage = ImageProcessor::matToQImage(m_inferenceInputLeftImage);
        if (!qImage.isNull()) {
            // 设置原始图像尺寸用于坐标转换 - 使用裁剪后图像的尺寸
            m_leftImageLabel->setOriginalImageSize(QSize(m_inferenceInputLeftImage.cols, m_inferenceInputLeftImage.rows));

            m_leftImageLabel->setPixmap(QPixmap::fromImage(qImage.scaled(
                m_leftImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            LOG_INFO(QString("显示校正裁剪后的左相机图像，尺寸: %1x%2")
                    .arg(m_inferenceInputLeftImage.cols).arg(m_inferenceInputLeftImage.rows));
        }
    } else if (!m_leftImage.empty() && m_leftImageLabel) {
        // 如果没有推理输入图像，显示原始图像（这种情况不应该发生）
        QImage qImage = ImageProcessor::matToQImage(m_leftImage);
        if (!qImage.isNull()) {
            // 设置原始图像尺寸用于坐标转换
            m_leftImageLabel->setOriginalImageSize(QSize(m_leftImage.cols, m_leftImage.rows));

            m_leftImageLabel->setPixmap(QPixmap::fromImage(qImage.scaled(
                m_leftImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            LOG_WARNING(QString("显示原始左相机图像（应显示裁剪后图像），尺寸: %1x%2")
                    .arg(m_leftImage.cols).arg(m_leftImage.rows));
        }
    }
    
    // 右相机图像不再显示，已隐藏控件
    
    // 显示深度图（如果有）
    if (!m_depthMap.empty() && m_depthImageLabel) {
		// m_depthMap 为 CV_32F 毫米单位，使用合理范围归一化，避免整幅蓝色
		float min_mm = 0.0f;
		float max_mm = 10000.0f;
		// 尝试从综合处理器读取配置范围
		if (auto* proc = m_inferenceService.getComprehensiveProcessor()) {
			// 无直接接口，保持默认；如需动态范围，可计算有效像素的分位数
		}
		cv::Mat clamped;
		m_depthMap.convertTo(clamped, CV_32F);
		// 更强对比：使用逆深度 + 更激进的百分位（0.5%~99.5%） + CLAHE
		cv::Mat validMask = (clamped > 0) & (clamped < 1e7f);
		cv::Mat inv = 1.0 / cv::max(clamped, 1.0f); // 1/mm，近处更亮
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
		// 局部对比度增强
		cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8,8));
		cv::Mat vizCLAHE; clahe->apply(viz8U, vizCLAHE);
        cv::Mat colored;
		cv::applyColorMap(vizCLAHE, colored, cv::COLORMAP_TURBO);
        
        QImage depthQImage = ImageProcessor::matToQImage(colored);
		if (!depthQImage.isNull()) {
			m_depthImageLabel->setPixmap(QPixmap::fromImage(depthQImage.scaled(
				m_depthImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
			LOG_INFO("深度图显示成功");
        } else {
                LOG_ERROR("深度图转换为QImage失败");
        }
    }
    
    // 显示视差图（如果有）
    /* 视差图显示代码已移除
    if (!m_disparityMap.empty() && m_disparityImageLabel) {
        // 将视差图转换为可视化图像
        cv::Mat normalized;
        cv::normalize(m_disparityMap, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
        
        // 应用热力图颜色映射
        cv::Mat colored;
        cv::applyColorMap(normalized, colored, cv::COLORMAP_HOT);
        
        QImage disparityQImage = ImageProcessor::matToQImage(colored);
        if (!disparityQImage.isNull()) {
            // 清除任何旧的文本
            m_disparityImageLabel->clear();
            
            // 设置新的图像，使用平滑缩放
            QPixmap pixmap = QPixmap::fromImage(disparityQImage);
            m_disparityImageLabel->setPixmap(pixmap.scaled(
                m_disparityImageLabel->size(), 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation));
                
            LOG_INFO(QString("视差图显示成功，尺寸: %1x%2").arg(pixmap.width()).arg(pixmap.height()));
        } else {
            LOG_ERROR("视差图转换为QImage失败");
            m_disparityImageLabel->setText("视差图显示失败");
        }
    } else {
        LOG_WARNING("无视差图数据可显示");
        if (m_disparityImageLabel) {
            m_disparityImageLabel->setText("等待视差图...");
        }
    }
    */
    
    // 添加相机引用
    using namespace SmartScope::Core::CameraUtils;
    MultiCameraManager::instance().addReference(CLIENT_ID);
    
    // 更新菜单栏位置
    updateLayout();
    
    // 确保导航栏完全隐藏，且不会被其他组件重新显示
    QWidget* mainWindow = this->window();
    if (mainWindow) {
        QList<QWidget*> children = mainWindow->findChildren<QWidget*>("navigationBar");
        for (QWidget* child : children) {
            child->hide();
            child->setVisible(false);
            child->lower(); // 将导航栏放到最底层，避免它意外显示
        }
    }
    
    // 确保菜单栏显示在最上层
    if (m_menuBar) {
        m_menuBar->show();
        m_menuBar->raise();
        m_menuBar->setVisible(true); // 确保菜单栏可见
        LOG_INFO("3D测量页面菜单栏已显示并提升到顶层");
    }
    
    // 重置放大镜状态
    if (m_magnifierManager) {
        // 安装事件过滤器，用于处理放大镜
        m_leftImageLabel->removeEventFilter(this);
        m_leftImageLabel->installEventFilter(this);
        
        // 同时安装在左图的父容器上，确保边缘区域也能捕获
        if (m_leftImageLabel->parentWidget()) {
            m_leftImageLabel->parentWidget()->removeEventFilter(this);
            m_leftImageLabel->parentWidget()->installEventFilter(this);
        }
        
        LOG_INFO("安装事件过滤器到左图区域，用于处理放大镜功能");
    }
    
    LOG_INFO("3D测量页面显示事件处理完成，放大镜将在用户点击左图区域时创建和显示");
    
    // 确保显示时是最新绘制状态
    redrawMeasurementsOnLabel();
    updatePointCloudMeasurements();
}

void MeasurementPage::hideEvent(QHideEvent *event)
{
	LOG_INFO("3D测量页面隐藏事件 - 开始清理放大镜");
	
	// 移除事件过滤器，确保放大镜不会在其他页面出现
	if (m_leftImageLabel) {
		m_leftImageLabel->removeEventFilter(this);
		if (m_leftImageLabel->parentWidget()) {
			m_leftImageLabel->parentWidget()->removeEventFilter(this);
		}
		LOG_INFO("移除左图区域的事件过滤器");
	}
	
	// 彻底销毁放大镜
	if (m_magnifierManager) {
		m_magnifierManager->destroyMagnifier();
		LOG_INFO("3D测量页面隐藏 - 销毁放大镜");
	}
	
	// 恢复显示导航栏（如果不是跳转到调试页）
	QWidget* mainWindow = this->window();
	if (mainWindow) {
		QList<QWidget*> children = mainWindow->findChildren<QWidget*>("navigationBar");
		for (QWidget* child : children) {
			if (!m_preserveOnHide) child->show();
		}
	}
	
	// 确保菜单栏隐藏
	if (m_menuBar) {
		m_menuBar->hide();
		LOG_INFO("3D测量页面菜单栏已隐藏");
	}
	
	if (!m_preserveOnHide) {
		// 离开页面时清空测量结果与临时数据，保证下次进入为空
		if (m_measurementManager) {
			m_measurementManager->clearMeasurements();
		}
		m_measurementPoints.clear();
		m_measurementPointsTemp.clear();
		m_originalClickPoints.clear();
		if (m_imageInteractionManager) {
			m_imageInteractionManager->clearCurrentMeasurementPoints();
			m_imageInteractionManager->clearTemporaryPoints();
		}
		if (m_stateManager) {
			m_stateManager->setMeasurementMode(MeasurementMode::View);
		}
		if (m_resultsLayout) {
			QLayoutItem* item;
			while ((item = m_resultsLayout->takeAt(1)) != nullptr) {
				if (item->widget()) {
					item->widget()->deleteLater();
				}
				delete item;
			}
		}
		if (m_resultsPanel) {
			m_resultsPanel->setVisible(false);
		}
		// 刷新 UI 以移除测量绘制
		redrawMeasurementsOnLabel();
		updatePointCloudMeasurements();
		update();
		LOG_INFO("3D测量页面隐藏 - 已清空测量结果与临时数据");
		
		// 离开页面时清理点云显示，但保留推理/深度缓存以避免重复计算
		if (m_pointCloudWidget) {
			m_pointCloudWidget->clearGeometryObjects();
			m_pointCloudWidget->clearPointCloud();
			m_pointCloudWidget->update();
			LOG_INFO("3D测量页面隐藏 - 已清理点云显示（保留推理缓存）");
		}
	} else {
		LOG_INFO("3D测量页面隐藏 - preserveOnHide=true，保留测量结果与点云，供调试页返回后继续显示");
	}

	// 一次性标志：使用后复位
	m_preserveOnHide = false;
}

bool MeasurementPage::setImagesFromHomePage(const cv::Mat& leftImage, const cv::Mat& rightImage)
{
    try {
        // 检查输入图像
        if (leftImage.empty() && rightImage.empty()) {
            LOG_ERROR("左右相机图像均为空");
            return false;
        }

        LOG_INFO(QString("接收到相机图像 - 左: %1x%2 类型: %3, 右: %4x%5 类型: %6")
                 .arg(leftImage.cols).arg(leftImage.rows).arg(leftImage.type())
                 .arg(rightImage.cols).arg(rightImage.rows).arg(rightImage.type()));

        // 检查图像标签是否存在 - 这个检查应该已经在Page Manager中完成，但这里再次确认
        if (!m_leftImageLabel) {
            LOG_ERROR("左图像标签未初始化");
            return false;
        }
        
        if (!m_rightImageLabel) {
            LOG_ERROR("右图像标签未初始化");
            return false;
        }

        // 先旋转图像，然后克隆
        cv::Mat rotatedLeft, rotatedRight;
        
        if (!leftImage.empty()) {
            // 向右旋转90度（顺时针）
            cv::rotate(leftImage, rotatedLeft, cv::ROTATE_90_CLOCKWISE);
            m_leftImage = rotatedLeft.clone();
            LOG_INFO(QString("左相机图像旋转后: %1x%2").arg(m_leftImage.cols).arg(m_leftImage.rows));
        }
        
        if (!rightImage.empty()) {
            // 向右旋转90度（顺时针）
            cv::rotate(rightImage, rotatedRight, cv::ROTATE_90_CLOCKWISE);
            m_rightImage = rotatedRight.clone();
            LOG_INFO(QString("右相机图像旋转后: %1x%2").arg(m_rightImage.cols).arg(m_rightImage.rows));
        }

        // 如果相机参数已加载并且重映射已初始化，进行畸变校正
        // --- 修改：使用 Calibration Helper 检查状态和校正 ---
        if (m_correctionManager && m_correctionManager->isInitialized()) {
            LOG_INFO("开始进行图像畸变校正 (通过 Correction Manager)");
            cv::Mat correctedLeft = m_leftImage.clone();
            cv::Mat correctedRight = m_rightImage.clone();

            auto result = m_correctionManager->correctImages(
                correctedLeft, correctedRight, 
                SmartScope::Core::CorrectionType::DISTORTION | SmartScope::Core::CorrectionType::STEREO_RECTIFICATION
            );
            
            if (result.success) {
                LOG_INFO("图像畸变校正成功 (通过 Correction Manager)");
                // 使用校正后的图像
                result.correctedLeftImage.copyTo(m_leftImage);
                result.correctedRightImage.copyTo(m_rightImage);
            } else {
                LOG_WARNING(QString("图像畸变校正失败: %1，使用未校正的图像").arg(result.errorMessage));
            }
        } else {
            LOG_WARNING("相机校正管理器未初始化，跳过畸变校正");
        }
        // --- 修改结束 ---

        // 转换并显示图像
        if (!m_leftImage.empty()) {
            // 确保图像类型正确
            cv::Mat displayImage;
            if (m_leftImage.type() == CV_8UC1) {
                // 如果是灰度图像，转换为彩色
                cv::cvtColor(m_leftImage, displayImage, cv::COLOR_GRAY2BGR);
            } else if (m_leftImage.type() == CV_8UC3) {
                // 如果已经是彩色图像，直接使用
                displayImage = m_leftImage;
            } else if (m_leftImage.type() == CV_16UC1) {
                // 如果是16位灰度图像（如深度图），归一化并转换
                cv::Mat normalized;
                cv::normalize(m_leftImage, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
                cv::cvtColor(normalized, displayImage, cv::COLOR_GRAY2BGR);
            } else {
                LOG_WARNING(QString("不支持的左相机图像类型: %1，尝试转换").arg(m_leftImage.type()));
                try {
                    // 尝试转换为8位3通道
                    m_leftImage.convertTo(displayImage, CV_8U);
                    if (displayImage.channels() == 1) {
                        cv::cvtColor(displayImage, displayImage, cv::COLOR_GRAY2BGR);
                    }
                } catch (const cv::Exception& e) {
                    LOG_ERROR(QString("转换左相机图像类型失败: %1").arg(e.what()));
                    return false;
                }
            }
            
            QImage leftQImage = ImageProcessor::matToQImage(displayImage);
            if (!leftQImage.isNull()) {
                // 设置原始图像尺寸用于坐标转换
                m_leftImageLabel->setOriginalImageSize(QSize(displayImage.cols, displayImage.rows));
                
                m_leftImageLabel->setPixmap(QPixmap::fromImage(leftQImage.scaled(
                    m_leftImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                LOG_INFO(QString("左相机图像显示成功，尺寸: %1x%2")
                        .arg(displayImage.cols).arg(displayImage.rows));
            } else {
                LOG_ERROR("左相机图像转换为QImage失败");
                return false;
            }
        }

        if (!m_rightImage.empty()) {
            // 确保图像类型正确
            cv::Mat displayImage;
            if (m_rightImage.type() == CV_8UC1) {
                // 如果是灰度图像，转换为彩色
                cv::cvtColor(m_rightImage, displayImage, cv::COLOR_GRAY2BGR);
            } else if (m_rightImage.type() == CV_8UC3) {
                // 如果已经是彩色图像，直接使用
                displayImage = m_rightImage;
            } else if (m_rightImage.type() == CV_16UC1) {
                // 如果是16位灰度图像（如深度图），归一化并转换
                cv::Mat normalized;
                cv::normalize(m_rightImage, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
                cv::cvtColor(normalized, displayImage, cv::COLOR_GRAY2BGR);
            } else {
                LOG_WARNING(QString("不支持的右相机图像类型: %1，尝试转换").arg(m_rightImage.type()));
                try {
                    // 尝试转换为8位3通道
                    m_rightImage.convertTo(displayImage, CV_8U);
                    if (displayImage.channels() == 1) {
                        cv::cvtColor(displayImage, displayImage, cv::COLOR_GRAY2BGR);
                    }
                } catch (const cv::Exception& e) {
                    LOG_ERROR(QString("转换右相机图像类型失败: %1").arg(e.what()));
                    return false;
                }
            }
            
            QImage rightQImage = ImageProcessor::matToQImage(displayImage);
            if (!rightQImage.isNull()) {
                m_rightImageLabel->setPixmap(QPixmap::fromImage(rightQImage.scaled(
                    m_rightImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                LOG_INFO("右相机图像显示成功");
            } else {
                LOG_ERROR("右相机图像转换为QImage失败");
                return false;
            }
        }

        // 设置图像就绪标志
        m_imagesReady = (!m_leftImage.empty() || !m_rightImage.empty());
        
        // 保存原始图像尺寸，用于深度图缩放计算
        if (!m_leftImage.empty()) {
            m_originalImageSize = QSize(m_leftImage.cols, m_leftImage.rows);
            LOG_INFO(QString("保存原始图像尺寸: %1x%2").arg(m_originalImageSize.width()).arg(m_originalImageSize.height()));
        }
        
        // 如果图像已就绪并且推理服务已初始化，执行深度推理
        if (m_imagesReady && m_inferenceInitialized && !m_leftImage.empty() && !m_rightImage.empty()) {
            LOG_INFO("准备执行深度推理...");
            performDepthInference(m_leftImage, m_rightImage);
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("设置相机图像异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("设置相机图像时发生未知异常");
        return false;
    }
}

void MeasurementPage::performDepthInference(const cv::Mat& leftImage, const cv::Mat& rightImage)
{
    try {
        // 检查推理服务是否初始化
        if (!m_inferenceInitialized) {
            LOG_ERROR("推理服务未初始化，无法执行深度推理");
            showToast(this, "推理服务未初始化，请检查模型", 2000);
            return;
        }

        // 检查输入图像
        if (leftImage.empty() || rightImage.empty()) {
            LOG_ERROR("输入图像为空，无法执行深度推理");
            showToast(this, "相机图像为空，无法执行深度推理", 2000);
            return;
        }

        // 检查输入图像尺寸是否一致
        if (leftImage.size() != rightImage.size()) {
            LOG_ERROR(QString("左右相机图像尺寸不一致 - 左: %1x%2, 右: %3x%4")
                    .arg(leftImage.cols).arg(leftImage.rows)
                    .arg(rightImage.cols).arg(rightImage.rows));
            showToast(this, "左右相机图像尺寸不一致", 2000);
            return;
        }

        // 创建深拷贝以避免数据竞争
        cv::Mat leftCopy = leftImage.clone();
        cv::Mat rightCopy = rightImage.clone();
        
        // 重要：不在这里应用ROI，因为立体校正已经应用了ROI
        // 直接使用校正后的图像进行推理预处理
        LOG_INFO(QString("使用立体校正后的图像进行推理，尺寸: %1x%2")
                .arg(leftCopy.cols).arg(leftCopy.rows));
        // --- 修改结束 ---
		
		// 缓存校正后的左右图，供调试页面使用
		m_leftRectifiedImage = leftCopy.clone();
		m_rightRectifiedImage = rightCopy.clone();
        
        // 记录原始尺寸
        int originalWidth = leftCopy.cols;   // 通常是720
        int originalHeight = leftCopy.rows;  // 通常是1280
        
        LOG_INFO(QString("推理前图像尺寸 - 左右均为: %1x%2")
                .arg(originalWidth).arg(originalHeight));

        // 检查图像格式和通道数
        if (leftCopy.channels() != 3 || rightCopy.channels() != 3) {
            LOG_WARNING(QString("输入图像通道数非3通道 - 左: %1, 右: %2, 尝试转换为BGR格式")
                    .arg(leftCopy.channels()).arg(rightCopy.channels()));
            
            // 转换为3通道BGR格式
            if (leftCopy.channels() == 1) {
                cv::cvtColor(leftCopy, leftCopy, cv::COLOR_GRAY2BGR);
            }
            if (rightCopy.channels() == 1) {
                cv::cvtColor(rightCopy, rightCopy, cv::COLOR_GRAY2BGR);
            }
        }

        // 检查图像是否连续存储
        if (!leftCopy.isContinuous()) {
            LOG_WARNING("左图像数据不连续，创建连续副本");
            leftCopy = leftCopy.clone();
        }
        if (!rightCopy.isContinuous()) {
            LOG_WARNING("右图像数据不连续，创建连续副本");
            rightCopy = rightCopy.clone();
        }

		// 计算中心 3:4 裁剪ROI（高:宽=4:3），以高度为基准裁剪宽度
		cv::Rect cropROI(0, 0, originalWidth, originalHeight);
		m_cropROI = cropROI;
		{
			// 目标宽度 = 高 * 3/4
			int targetW = (originalHeight * 3) / 4;
			if (targetW > originalWidth) {
				// 如果宽不够，则以宽为基准裁剪高度：目标高度 = 宽 * 4/3
				int targetH = (originalWidth * 4) / 3;
				int y0 = std::max(0, (originalHeight - targetH) / 2);
				cropROI = cv::Rect(0, y0, originalWidth, std::min(targetH, originalHeight));
			} else {
				int x0 = std::max(0, (originalWidth - targetW) / 2);
				cropROI = cv::Rect(x0, 0, std::min(targetW, originalWidth), originalHeight);
			}
			m_cropROI = cropROI;
		}
		LOG_INFO(QString("中心3:4裁剪ROI: x=%1 y=%2 w=%3 h=%4")
				 .arg(cropROI.x).arg(cropROI.y).arg(cropROI.width).arg(cropROI.height));

        // 提交推理请求
        SmartScope::InferenceRequest request;
        request.left_image = leftCopy;
        request.right_image = rightCopy;
        request.save_path = "";  // 不保存到文件
        request.generate_pointcloud = false;
        request.original_width = originalWidth;
        request.original_height = originalHeight;
		// 启用3:4裁剪流程：stereo 用原图，mono 用裁剪图，stereo输出再按同ROI裁剪
		request.apply_43_crop = true;
		request.crop_roi = cropROI;
        
        // 从相机标定参数获取基线长度和焦距
        auto stereoHelper = m_correctionManager ? m_correctionManager->getStereoCalibrationHelper() : nullptr;
        if (stereoHelper && stereoHelper->isRemapInitialized()) {
            // 获取基线长度（从平移向量）
            cv::Mat translationVector = stereoHelper->getTranslationVector();
            if (!translationVector.empty()) {
                double baseline = cv::norm(translationVector);
                request.baseline = static_cast<float>(baseline);
                LOG_INFO(QString("从标定参数获取基线长度: %1 mm").arg(baseline, 0, 'f', 2));
            } else {
                request.baseline = 2.06f; // 使用当前设备的默认值
                LOG_WARNING("无法获取基线长度，使用默认值: 2.06 mm");
            }
            
            // 获取焦距（从左相机内参矩阵）
            cv::Mat cameraMatrixLeft = stereoHelper->getCameraMatrixLeft();
            if (!cameraMatrixLeft.empty()) {
                double focalLength = cameraMatrixLeft.at<double>(0, 0);
                request.focal_length = static_cast<float>(focalLength);
                LOG_INFO(QString("从标定参数获取焦距: %1 像素").arg(focalLength, 0, 'f', 2));
            } else {
                request.focal_length = 905.41f; // 使用当前设备的默认值
                LOG_WARNING("无法获取焦距，使用默认值: 905.41 像素");
            }
        } else {
            // 使用当前设备的默认参数
            request.baseline = 2.06f;
            request.focal_length = 905.41f;
            LOG_WARNING("标定参数未初始化，使用默认参数 - 基线: 2.06mm, 焦距: 905.41像素");
        }
        
        // 设置推理前的UI状态
        m_measurementState = MeasurementState::Processing;
        updateMeasurementState();
        showToast(this, "正在进行深度推理...", 1000);
        
		// UI：显示裁剪后的左图作为推理输入视图
		if (m_leftImageLabel) {
			cv::Mat leftCroppedView = leftCopy(cropROI).clone();
			m_inferenceInputLeftImage = leftCroppedView;
			LOG_INFO(QString("坐标变换跟踪 - rectified: %1x%2 → cropped(3:4): %3x%4")
                        .arg(leftCopy.cols).arg(leftCopy.rows)
                        .arg(m_inferenceInputLeftImage.cols).arg(m_inferenceInputLeftImage.rows));

                m_coordinateTransform.originalSize = QSize(m_leftImage.cols, m_leftImage.rows);
                m_coordinateTransform.rectifiedSize = QSize(leftCopy.cols, leftCopy.rows);
                m_coordinateTransform.finalSize = QSize(m_inferenceInputLeftImage.cols, m_inferenceInputLeftImage.rows);

			// 将裁剪ROI告知交互管理器，用于K的主点修正
			if (m_imageInteractionManager) {
				m_imageInteractionManager->setCropROI(m_cropROI);
			}

                    QImage qImage = ImageProcessor::matToQImage(m_inferenceInputLeftImage);
                    if (!qImage.isNull()) {
                        m_leftImageLabel->setOriginalImageSize(QSize(m_inferenceInputLeftImage.cols, m_inferenceInputLeftImage.rows));
                        m_leftImageLabel->setPixmap(QPixmap::fromImage(qImage.scaled(
                            m_leftImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
				LOG_INFO("立即更新左侧显示为3:4裁剪后图像");
                    }
                }

	// 将Q矩阵注入综合深度处理器，确保后续流程走已校正图路径
	auto stereoHelper2 = m_correctionManager ? m_correctionManager->getStereoCalibrationHelper() : nullptr;
	if (stereoHelper2) {
			cv::Mat Q = stereoHelper2->getQMatrix();
			auto* proc = m_inferenceService.getComprehensiveProcessor();
			if (proc && !Q.empty()) {
				proc->setQMatrix(Q);
				LOG_INFO("已向综合深度处理器注入Q矩阵");
			}
        }

        LOG_INFO("提交深度推理请求");
        m_inferenceService.submitRequest(request);
        LOG_INFO("深度推理请求已提交，等待异步结果");

    } catch (const cv::Exception& e) {
        LOG_ERROR(QString("深度推理OpenCV异常: %1").arg(e.what()));
        showToast(this, "深度推理失败: 图像处理错误", 2000);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("深度推理标准异常: %1").arg(e.what()));
        showToast(this, "深度推理失败: 内部错误", 2000);
    } catch (...) {
        LOG_ERROR("深度推理未知异常");
        showToast(this, "深度推理失败: 未知错误", 2000);
    }
}

void MeasurementPage::handleInferenceResult(const SmartScope::InferenceResult& result)
{
    // 检查结果是否有效
    if (!result.success) {
        LOG_ERROR("推理失败: " + result.error_message);
        showToast(this, "深度推理失败: " + result.error_message, 2000);
        m_measurementState = MeasurementState::Error;
        updateMeasurementState();
        return;
    }
    
    // 检查会话ID是否匹配当前会话
    qint64 currentSessionId = m_inferenceService.getCurrentSessionId();
    if (result.session_id != currentSessionId) {
        LOG_WARNING(QString("收到过时的推理结果，会话ID不匹配 - 当前: %1, 结果: %2")
                   .arg(currentSessionId).arg(result.session_id));
        return;
    }
    
    LOG_INFO(QString("收到推理结果 - 深度图尺寸: %1x%2, 类型: %3")
             .arg(result.depth_map.cols).arg(result.depth_map.rows).arg(result.depth_map.type()));

    // 保存深度图
    m_depthMap = result.depth_map.clone();
    
    // 保存其他深度信息
    if (!result.mono_depth_raw.empty()) {
		m_monoDepthRaw = result.mono_depth_raw.clone();
        cv::imwrite("mono_depth_raw.png", result.mono_depth_raw);
        LOG_INFO("单目原始深度图已保存到 mono_depth_raw.png");
	} else {
		m_monoDepthRaw = cv::Mat();
    }
    
    if (!result.mono_depth_calibrated.empty()) {
        m_monoDepthCalibrated = result.mono_depth_calibrated.clone();
        cv::imwrite("mono_depth_calibrated.png", result.mono_depth_calibrated);
        LOG_INFO("校准后单目深度图已保存到 mono_depth_calibrated.png");
    } else {
        m_monoDepthCalibrated = cv::Mat(); // 清空
    }
    
    if (!result.disparity_map.empty()) {
        cv::imwrite("disparity_map.png", result.disparity_map);
        LOG_INFO("视差图已保存到 disparity_map.png");
		// 缓存真实视差图
		m_disparityMap = result.disparity_map.clone();
    }
    
    if (!result.confidence_map.empty()) {
        cv::imwrite("confidence_map.png", result.confidence_map);
        LOG_INFO("置信度图已保存到 confidence_map.png");
    }
    
    // 保存深度图用于分析
	if (!m_depthMap.empty()) {
    cv::imwrite("depth_map.png", m_depthMap);
    LOG_INFO("深度图已保存到 depth_map.png");
	} else {
		LOG_WARNING("深度图为空，跳过保存 depth_map.png");
	}
    
    // 记录校准信息
    if (result.calibration_success) {
        LOG_INFO(QString("深度校准成功 - 缩放: %1, 偏移: %2")
                .arg(result.calibration_scale).arg(result.calibration_bias));
    } else {
        LOG_WARNING("深度校准失败");
    }
    
    // 生成视差图（从深度图）
    if (!m_depthMap.empty()) {
		// 替换为：优先使用推理结果中的真实视差图
		if (m_disparityMap.empty()) {
			LOG_WARNING("推理结果未提供视差图，无法从深度图可靠反推，视差图保持为空");
		} else {
			LOG_INFO(QString("使用推理结果中的视差图 - 尺寸: %1x%2, 类型: %3")
                .arg(m_disparityMap.cols).arg(m_disparityMap.rows).arg(m_disparityMap.type()));
		}
    } else {
        LOG_WARNING("深度图为空，无法生成视差图");
        m_disparityMap = cv::Mat(); // 清空
    }
    
    // 显示深度图
    if (!m_depthMap.empty() && m_depthImageLabel) {
		// m_depthMap 为 CV_32F 毫米单位，使用合理范围归一化，避免整幅蓝色
		float min_mm = 0.0f;
		float max_mm = 10000.0f;
		// 尝试从综合处理器读取配置范围
		if (auto* proc = m_inferenceService.getComprehensiveProcessor()) {
			// 无直接接口，保持默认；如需动态范围，可计算有效像素的分位数
		}
		cv::Mat clamped;
		m_depthMap.convertTo(clamped, CV_32F);
		cv::Mat validMask = (clamped > 0) & (clamped < 1e7f);
		double vmin = 0, vmax = 0;
		cv::minMaxIdx(clamped, &vmin, &vmax, nullptr, nullptr, validMask);
        cv::Mat normalized;
		if (vmax > vmin) {
			cv::Mat scaled = (clamped - vmin) * (255.0 / (vmax - vmin));
			scaled.setTo(0, ~validMask);
			scaled.convertTo(normalized, CV_8U);
		} else {
			cv::normalize(clamped, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
		}
		// 可选：按固定范围归一化，避免受异常值影响
		// normalized = ((clamped - min_mm) / (max_mm - min_mm)) * 255
		// 注意裁剪
		// ... 这里保留当前NORM_MINMAX，若仍偏蓝，可切换为固定范围
		
		// 采用鲁棒可视化：使用2%-98%百分位范围，反色+gamma，再用TURBO
		// 这里normalized已是0-255，我们重新按鲁棒范围构造
		// 计算鲁棒范围
		double pmin = 0, pmax = 0;
		{
			// 使用直方图估计百分位
			cv::Mat validMask = (clamped > 0) & (clamped < 1e7f);
			// 将有效像素收集为单列
			cv::Mat vals; clamped.copyTo(vals, validMask);
			if (!vals.empty()) {
				vals = vals.reshape(1, 1); // 单行
				std::vector<float> v; v.assign((float*)vals.datastart, (float*)vals.dataend);
				std::sort(v.begin(), v.end());
				auto idx = [&](double q){ return std::min((size_t)std::max(0.0, q*(v.size()-1)), v.size()-1); };
				pmin = v[idx(0.02)]; pmax = v[idx(0.98)];
				if (pmax <= pmin) { pmin = v.front(); pmax = v.back(); }
			}
		}
		cv::Mat robustNorm;
		if (pmax > pmin) {
			cv::Mat f = (clamped - pmin) * (1.0 / (pmax - pmin));
			cv::min(f, 1.0, f); cv::max(f, 0.0, f);
			f = 1.0 - f; // 近处高亮
			cv::pow(f, 0.7, f); // gamma提升
			f.convertTo(robustNorm, CV_8U, 255.0);
		} else {
			robustNorm = normalized; // 回退
		}
        cv::Mat colored;
		cv::applyColorMap(robustNorm, colored, cv::COLORMAP_TURBO);
        
        QImage depthQImage = ImageProcessor::matToQImage(colored);
        if (!depthQImage.isNull()) {
            m_depthImageLabel->setPixmap(QPixmap::fromImage(depthQImage.scaled(
                m_depthImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            LOG_INFO("深度图显示成功");
        } else {
            LOG_ERROR("深度图转换为QImage失败");
        }
    }
    
    // 显示视差图
    if (!m_disparityMap.empty() && m_disparityImageLabel) {
        // 将视差图转换为可视化图像
        cv::Mat normalized;
        cv::normalize(m_disparityMap, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
        
        // 应用热力图颜色映射
        cv::Mat colored;
        cv::applyColorMap(normalized, colored, cv::COLORMAP_HOT);
        
        QImage disparityQImage = ImageProcessor::matToQImage(colored);
        if (!disparityQImage.isNull()) {
            m_disparityImageLabel->setPixmap(QPixmap::fromImage(disparityQImage.scaled(
                m_disparityImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            LOG_INFO("视差图显示成功");
        } else {
            LOG_ERROR("视差图转换为QImage失败");
        }
    } else {
		// 未启用视差显示区域或无视差数据，静默跳过
        if (m_disparityImageLabel) {
            m_disparityImageLabel->setText("视差图不可用");
        }
    }
    
    // 更新测量状态
    m_measurementState = MeasurementState::Completed;
    updateMeasurementState();
    
    // 生成点云
    generatePointCloud(m_depthMap, cv::Mat());
    
    // 显示成功通知
    showToast(this, "深度推理完成", 1500);
}

void MeasurementPage::captureImages()
{
    try {
        // 获取相机管理器单例
        using namespace SmartScope::Core::CameraUtils;
        MultiCameraManager& cameraManager = MultiCameraManager::instance();
        
        // 检查相机管理器是否已运行
        if (!cameraManager.isRunning()) {
            LOG_ERROR("相机管理器未运行");
            showToast(this, "相机系统未运行，请重启应用", 2000);
            return;
        }
        
        // 从PageManager获取最新的相机ID
        PageManager* pageManager = qobject_cast<PageManager*>(parentWidget());
        if (pageManager) {
            HomePage* homePage = pageManager->getHomePage();
            if (homePage) {
                m_leftCameraId = homePage->getLeftCameraId();
                m_rightCameraId = homePage->getRightCameraId();
                LOG_INFO(QString("从HomePage获取相机ID - 左: %1, 右: %2").arg(m_leftCameraId).arg(m_rightCameraId));
            } else {
                LOG_WARNING("无法获取HomePage实例，无法获取最新相机ID");
            }
        } else {
            LOG_WARNING("无法获取PageManager实例，无法获取最新相机ID");
        }
        
        cv::Mat leftRaw, rightRaw;
        int64_t leftTimestamp = 0, rightTimestamp = 0;
        
        // 获取左相机图像
        if (!m_leftCameraId.isEmpty()) {
            cameraManager.getLatestFrame(m_leftCameraId.toStdString(), leftRaw, leftTimestamp);
        } else {
            LOG_ERROR("左相机ID为空，无法获取图像");
            showToast(this, "左相机ID无效，请检查相机配置", 2000);
            return;
        }
        
        // 获取右相机图像 - 尽管不显示，但仍需要获取用于立体匹配
        if (!m_rightCameraId.isEmpty()) {
            cameraManager.getLatestFrame(m_rightCameraId.toStdString(), rightRaw, rightTimestamp);
        } else {
            LOG_ERROR("右相机ID为空，无法获取图像");
            showToast(this, "右相机ID无效，请检查相机配置", 2000);
            return;
        }
        
        // 处理图像
        if (!leftRaw.empty() || !rightRaw.empty()) {
            // 先旋转图像
            if (!leftRaw.empty()) {
                // 向右旋转90度（顺时针）
                cv::rotate(leftRaw, m_leftImage, cv::ROTATE_90_CLOCKWISE);
                LOG_INFO(QString("左相机图像旋转后: %1x%2").arg(m_leftImage.cols).arg(m_leftImage.rows));
            }
            
            if (!rightRaw.empty()) {
                // 向右旋转90度（顺时针）
                cv::rotate(rightRaw, m_rightImage, cv::ROTATE_90_CLOCKWISE);
                LOG_INFO(QString("右相机图像旋转后: %1x%2").arg(m_rightImage.cols).arg(m_rightImage.rows));
            }
            
            m_imagesReady = true;
            
            // 保存原始图像尺寸，用于深度图缩放计算
            if (!m_leftImage.empty()) {
                m_originalImageSize = QSize(m_leftImage.cols, m_leftImage.rows);
                LOG_INFO(QString("保存原始图像尺寸: %1x%2").arg(m_originalImageSize.width()).arg(m_originalImageSize.height()));
            }
            
            // --- 修改：使用 Camera Correction Manager 进行校正 ---
            if (m_correctionManager && m_correctionManager->isInitialized()) {
                LOG_INFO("对捕获的图像进行畸变校正 (通过 Correction Manager)");
                cv::Mat leftToCorrect = m_leftImage.clone();
                cv::Mat rightToCorrect = m_rightImage.clone();
                
                auto result = m_correctionManager->correctImages(
                    leftToCorrect, rightToCorrect, 
                    SmartScope::Core::CorrectionType::DISTORTION | SmartScope::Core::CorrectionType::STEREO_RECTIFICATION
                );
                
                if (result.success) {
                    LOG_INFO("捕获图像畸变校正成功 (通过 Correction Manager)");
                    result.correctedLeftImage.copyTo(m_leftImage);
                    result.correctedRightImage.copyTo(m_rightImage);
                } else {
                    LOG_WARNING(QString("捕获图像畸变校正失败: %1，使用未校正的图像").arg(result.errorMessage));
                }
            } else {
                LOG_WARNING("相机校正管理器未初始化，跳过畸变校正");
            }
            // --- 修改结束 ---
            
            // 显示校正裁剪后的左相机图像（推理输入图像）
            if (!m_inferenceInputLeftImage.empty() && m_leftImageLabel) {
                QImage leftQImage = ImageProcessor::matToQImage(m_inferenceInputLeftImage);
                if (!leftQImage.isNull()) {
                    // 设置原始图像尺寸用于坐标转换 - 使用裁剪后图像的尺寸
                    m_leftImageLabel->setOriginalImageSize(QSize(m_inferenceInputLeftImage.cols, m_inferenceInputLeftImage.rows));

                    m_leftImageLabel->setPixmap(QPixmap::fromImage(leftQImage.scaled(
                        m_leftImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                    LOG_INFO(QString("推理完成后显示校正裁剪后的左相机图像，尺寸: %1x%2")
                            .arg(m_inferenceInputLeftImage.cols).arg(m_inferenceInputLeftImage.rows));
                }
            } else if (!m_leftImage.empty() && m_leftImageLabel) {
                // 备用方案：如果没有推理输入图像，显示原始图像
                QImage leftQImage = ImageProcessor::matToQImage(m_leftImage);
                if (!leftQImage.isNull()) {
                    m_leftImageLabel->setOriginalImageSize(QSize(m_leftImage.cols, m_leftImage.rows));
                    m_leftImageLabel->setPixmap(QPixmap::fromImage(leftQImage.scaled(
                        m_leftImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                    LOG_WARNING(QString("推理完成后显示原始左相机图像（应显示裁剪后图像），尺寸: %1x%2")
                            .arg(m_leftImage.cols).arg(m_leftImage.rows));
                }
            }
            
            // 右相机图像不再显示 (已隐藏控件)
            
            // 如果图像就绪并且推理服务已初始化，执行深度推理
            if (m_imagesReady && m_inferenceInitialized && !m_leftImage.empty() && !m_rightImage.empty()) {
                LOG_INFO("准备执行深度推理...");
                performDepthInference(m_leftImage, m_rightImage);
            }
            
            showToast(this, "图像捕获成功", 2000);
        } else {
            LOG_ERROR("左右相机图像均为空");
            showToast(this, "图像捕获失败", 2000);
        }
    } catch (const cv::Exception& e) {
        LOG_ERROR(QString("OpenCV异常: %1").arg(e.what()));
        showToast(this, "捕获图像失败: OpenCV异常", 2000);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("标准异常: %1").arg(e.what()));
        showToast(this, "捕获图像失败: 标准异常", 2000);
    } catch (...) {
        LOG_ERROR("捕获图像时发生未知异常");
        showToast(this, "捕获图像失败: 未知异常", 2000);
    }
}

void MeasurementPage::resetMeasurement()
{
    // 清空图像
    m_leftImage.release();
    m_rightImage.release();
    m_imagesReady = false;
    
    // 清除显示
    if (m_leftImageLabel) { // 检查指针是否有效
        m_leftImageLabel->clear();
        m_leftImageLabel->setText("等待捕获图像...");
    }
    
    showToast(this, "已重置3D测量", 2000);
    LOG_INFO("已重置3D测量");
    
    // 清空测量点数据
    m_measurementPoints.clear();
    m_originalClickPoints.clear();
    LOG_INFO("已清空测量点数据");

    // --- 添加：清除当前剖面图关联 ---
    // m_currentProfileForChart = nullptr; 
    // -------------------------------
    
    // 使用状态管理器取消操作
    if (m_stateManager) { // 检查指针有效性
        m_stateManager->cancelOperation(); 
        m_stateManager->setMeasurementMode(MeasurementMode::View); // 确保回到查看模式
    }
    
    // 重新绘制以清除显示的测量点
    redrawMeasurementsOnLabel(); // 会处理基础图像显示
    
	// 清空点云（保留显示，避免页面切换后白屏）
	// if (m_pointCloudWidget) {
	//     m_pointCloudWidget->clearPointCloud();
	//     m_pointCloudWidget->clearGeometryObjects();
	//     m_pointCloudWidget->update();
	// }
    
    // 重置状态标志
    m_imagesReady = false;
    m_measurementState = MeasurementState::Idle;
    
    // 清空测量对象
    if (m_measurementManager) {
        m_measurementManager->clearMeasurements(); // 会触发 signalsChanged -> 更新UI
    }
    
    // 确保图表和按钮也隐藏
    if (m_profileChartPlot) {
        m_profileChartPlot->graph(0)->data()->clear();
        m_profileChartPlot->replot();
        m_profileChartPlot->setVisible(false);
    }
    updateProfileControlsVisibility(); // 更新按钮状态
    
    updateUIBasedOnMeasurementState();
}

void MeasurementPage::startMeasurement()
{
    if (!m_imagesReady) {
        showToast(this, "请先捕获图像", 2000);
        LOG_WARNING("未捕获图像，无法开始测量");
        return;
    }
    
    // 这里应该启动3D测量功能
    showToast(this, "3D测量功能开发中...", 2000);
    LOG_INFO("开始3D测量");
}

void MeasurementPage::exportModel()
{
    showToast(this, "导出3D模型功能开发中...", 2000);
    LOG_INFO("导出3D模型");
}

void MeasurementPage::updateMeasurementState()
{
    // 将变量声明移到switch外面，避免case标签问题
    auto stereoHelper = m_correctionManager ? m_correctionManager->getStereoCalibrationHelper() : nullptr;
    
    // 根据当前状态更新UI显示
    switch (m_measurementState) {
        case MeasurementState::Idle:
            LOG_INFO("3D测量状态：空闲");
            break;

        case MeasurementState::Ready:
            LOG_INFO("3D测量状态：就绪");
            if (m_leftImageLabel && m_leftImageLabel->pixmap(Qt::ReturnByValue).isNull()) {
                // ...
                m_measurementState = MeasurementState::Idle;
                break;
            }
            // --- 修改：检查 Helper 状态 ---
            if (!stereoHelper || !stereoHelper->isRemapInitialized()) { // Check helper state
                LOG_WARNING("相机校正未初始化 (Helper)，无法进行测量");
                showToast(this, "相机校正未准备好", 2000);
                m_measurementState = MeasurementState::Idle;
                break;
            }
            // --- 修改结束 ---
            if (!m_inferenceInitialized) {
                // ...
                m_measurementState = MeasurementState::Idle;
                break;
            }
            break;

        case MeasurementState::Processing:
            // 正在处理
            LOG_INFO("3D测量状态：处理中");
            break;

        case MeasurementState::Completed:
            // 测量完成
            LOG_INFO("3D测量状态：完成");
            // 检查是否有深度图
            if (m_depthImageLabel && m_depthImageLabel->pixmap(Qt::ReturnByValue).isNull()) {
                LOG_WARNING("深度图生成失败");
                m_measurementState = MeasurementState::Ready;
            }
            break;

        case MeasurementState::Error:
            // 错误状态
            LOG_ERROR("3D测量状态：错误");
            break;
    }
}

void MeasurementPage::updateUIBasedOnMeasurementState()
{
    MeasurementMode mode = m_stateManager->getMeasurementMode();
    
    // 更新结果面板可见性 - 始终隐藏，不再显示测量结果面板
    m_resultsPanel->setVisible(false);
    
    // 更新按钮可见性
    m_addMeasurementButton->setVisible(mode == MeasurementMode::View);
    
    // 更新完成按钮的可见性 - 只在添加模式下显示
    if (m_finishButton) {
        m_finishButton->setVisible(mode == MeasurementMode::Add);
        LOG_INFO(QString("完成按钮可见性设置为：%1").arg(mode == MeasurementMode::Add ? "显示" : "隐藏"));
    }
    
    // 更新点云窗口的鼠标光标
    switch (mode) {
        case MeasurementMode::Add:
        case MeasurementMode::Edit:
            m_pointCloudWidget->setCursor(Qt::CrossCursor);
            break;
        case MeasurementMode::Delete:
            m_pointCloudWidget->setCursor(Qt::ForbiddenCursor);
            break;
        case MeasurementMode::View:
        default:
            m_pointCloudWidget->setCursor(Qt::ArrowCursor);
            break;
    }
}

void MeasurementPage::generatePointCloud(const cv::Mat& depthMap, const cv::Mat& normalMap /* normalMap not used currently */)
{
    LOG_INFO("MeasurementPage::generatePointCloud called.");

    if (!m_pointCloudWidget) {
        LOG_ERROR("PointCloudGLWidget is not initialized!");
        return;
    }
    
    // 根据当前深度模式/融合结果选择正确的深度图
    cv::Mat finalDepthMap = depthMap;
    
    LOG_INFO(QString("当前深度模式: %1").arg(static_cast<int>(m_depthMode)));
    LOG_INFO(QString("传入深度图尺寸: %1x%2").arg(depthMap.cols).arg(depthMap.rows));
    LOG_INFO(QString("校准后单目深度图尺寸: %1x%2").arg(m_monoDepthCalibrated.cols).arg(m_monoDepthCalibrated.rows));
    
	// 确保使用正确的深度图
	if (m_depthMode == SmartScope::InferenceService::DepthMode::MONO_CALIBRATED) {
		// 单目校准模式：优先使用已保存的校准后单目深度图
        if (!m_monoDepthCalibrated.empty()) {
            finalDepthMap = m_monoDepthCalibrated;
			LOG_INFO("单目校准模式：使用已保存的校准后单目深度图生成点云");
        } else {
            // 如果没有保存的校准深度图，尝试从处理器获取
            SmartScope::InferenceService& inferenceService = SmartScope::InferenceService::instance();
            auto* processor = inferenceService.getComprehensiveProcessor();
            if (processor) {
                cv::Mat calibratedMonoDepth = processor->getIntermediateResult("mono_depth_calibrated");
                if (!calibratedMonoDepth.empty()) {
                    finalDepthMap = calibratedMonoDepth;
					LOG_INFO("单目校准模式：从处理器获取校准后的单目深度生成点云");
                } else {
					LOG_WARNING("单目校准模式：校准后的单目深度为空，使用传入的深度图");
                }
            } else {
				LOG_WARNING("单目校准模式：综合深度处理器不可用，使用传入的深度图");
            }
        }
	} else { /* 双目点云功能已移除，保持单目校准点云 */ }
    
    // 若有融合深度，优先使用融合结果
    {
        SmartScope::InferenceService& inferenceService = SmartScope::InferenceService::instance();
        auto* processor = inferenceService.getComprehensiveProcessor();
        if (processor) {
            cv::Mat fused = processor->getIntermediateResult("fused");
            if (!fused.empty()) {
                finalDepthMap = fused;
                LOG_INFO("点云生成优先使用融合深度(final_fused_depth)");
            }
        }
    }

    LOG_INFO(QString("最终使用深度图尺寸: %1x%2").arg(finalDepthMap.cols).arg(finalDepthMap.rows));
    
    if (finalDepthMap.empty()) {
        LOG_WARNING("最终深度图为空，跳过点云生成");
        return;
    }

    // 确定点云生成使用的颜色图像 - 必须使用校正裁剪后的图像
    cv::Mat colorImageForPCL;
    if (!m_inferenceInputLeftImage.empty()) {
        colorImageForPCL = m_inferenceInputLeftImage;
        LOG_INFO(QString("点云生成使用校正裁剪后的左图像作为颜色源，尺寸: %1x%2")
                 .arg(colorImageForPCL.cols).arg(colorImageForPCL.rows));
    } else {
        // 如果没有推理输入图像，这是一个错误状态
        LOG_ERROR("缺少校正裁剪后的图像，点云将无法正确着色");
        // colorImageForPCL保持为空，生成器将使用伪彩色
    }

	// 简化版：直接从深度图生成点云（不使用PLY与外部生成器）
	LOG_INFO("使用简化实现直接从深度图生成点云");

	// 确保深度为CV_32F（单位：毫米）
	cv::Mat depthF;
	if (finalDepthMap.type() != CV_32F) {
		finalDepthMap.convertTo(depthF, CV_32F);
	} else {
		depthF = finalDepthMap;
	}

	// 预分配容器
    std::vector<QVector3D> points;
    std::vector<QVector3D> colors;
	m_pointCloudPixelCoords.clear();
	points.reserve(static_cast<size_t>(depthF.total()));
	colors.reserve(static_cast<size_t>(depthF.total()));
	m_pointCloudPixelCoords.reserve(static_cast<size_t>(depthF.total()));

	// 基于局部中值残差与梯度的离群点过滤（去除过渡区域的可疑点）
	cv::Mat validMask = (depthF > 0) & (depthF < 1e7f);
	double minObservedDepth = 0.0, maxObservedDepth = 0.0;
	cv::minMaxLoc(depthF, &minObservedDepth, &maxObservedDepth, nullptr, nullptr, validMask);

	// 3x3 中值残差
	cv::Mat median3;
	cv::medianBlur(depthF, median3, 3);
	cv::Mat residual;
	cv::absdiff(depthF, median3, residual);
	float residualThreshMm = std::max(20.0f, 0.02f * static_cast<float>(maxObservedDepth));
	cv::Mat residualMask = residual <= residualThreshMm;

	// Sobel 梯度阈值（毫米/像素）
	cv::Mat gradX, gradY, gradMag;
	cv::Sobel(depthF, gradX, CV_32F, 1, 0, 3);
	cv::Sobel(depthF, gradY, CV_32F, 0, 1, 3);
	cv::magnitude(gradX, gradY, gradMag);
	float gradThreshMm = std::max(30.0f, 0.01f * static_cast<float>(maxObservedDepth));
	cv::Mat gradMask = gradMag <= gradThreshMm;

	// 最终有效掩码
	cv::Mat finalValidMask = validMask & residualMask & gradMask;
	int initialValidCount = cv::countNonZero(validMask);
	int finalValidCount = cv::countNonZero(finalValidMask);
	LOG_INFO(QString("点云离群点过滤: 初始=%1, 保留=%2, 移除=%3 (%4%)")
			 .arg(initialValidCount)
			 .arg(finalValidCount)
			 .arg(initialValidCount - finalValidCount)
			 .arg(initialValidCount > 0 ? (float)(initialValidCount - finalValidCount) * 100.0f / initialValidCount : 0.0f));

	// 获取相机内参，用于反投影（确保与finalDepthMap尺寸对应）
	cv::Mat K;
	bool usedP1 = false;
	auto stereoHelper = m_correctionManager ? m_correctionManager->getStereoCalibrationHelper() : nullptr;
	if (stereoHelper) {
		cv::Mat P1 = stereoHelper->getP1();
		if (!P1.empty() && P1.rows == 3 && P1.cols == 4) {
			K = cv::Mat::eye(3, 3, CV_64F);
			K.at<double>(0, 0) = P1.at<double>(0, 0);
			K.at<double>(1, 1) = P1.at<double>(1, 1);
			K.at<double>(0, 2) = P1.at<double>(0, 2);
			K.at<double>(1, 2) = P1.at<double>(1, 2);
			usedP1 = true;
			LOG_INFO(QString("点云使用P1构造K: fx=%1, fy=%2, cx=%3, cy=%4")
					 .arg(K.at<double>(0,0),0,'f',2)
					 .arg(K.at<double>(1,1),0,'f',2)
					 .arg(K.at<double>(0,2),0,'f',2)
					 .arg(K.at<double>(1,2),0,'f',2));
		}
	}
	if (K.empty()) {
		K = stereoHelper ? stereoHelper->getCameraMatrixLeft().clone() : cv::Mat();
		LOG_WARNING("点云P1不可用，回退使用原始K");
	}
	if (K.empty() || K.cols < 3 || K.rows < 3) {
		LOG_ERROR("相机内参无效，无法进行反投影生成点云");
		return;
	}
	// 先应用rectify阶段ROI偏移
	try {
		cv::Rect roi1 = stereoHelper ? stereoHelper->getRoi1() : cv::Rect();
		if (roi1.width > 0 && roi1.height > 0) {
			K.at<double>(0, 2) -= roi1.x;
			K.at<double>(1, 2) -= roi1.y;
			LOG_INFO(QString("点云应用rectify ROI偏移: roi1=(%1,%2), cx=%3, cy=%4")
					 .arg(roi1.x).arg(roi1.y)
					 .arg(K.at<double>(0,2),0,'f',2)
					 .arg(K.at<double>(1,2),0,'f',2));
		}
	} catch (...) {}
	// 再应用3:4裁剪偏移
	if (m_cropROI.width > 0 && m_cropROI.height > 0) {
		K.at<double>(0, 2) -= m_cropROI.x;
		K.at<double>(1, 2) -= m_cropROI.y;
		LOG_INFO(QString("点云应用3:4裁剪偏移: crop=(%1,%2), 最终cx=%3, cy=%4")
				 .arg(m_cropROI.x).arg(m_cropROI.y)
				 .arg(K.at<double>(0,2),0,'f',2)
				 .arg(K.at<double>(1,2),0,'f',2));
	}
	const double fx = K.at<double>(0, 0);
	const double fy = K.at<double>(1, 1);
	const double cx = K.at<double>(0, 2);
	const double cy = K.at<double>(1, 2);
	
	const int step = 1; // 全采样
	for (int y = 0; y < depthF.rows; y += step) {
		const float* dptr = depthF.ptr<float>(y);
		for (int x = 0; x < depthF.cols; x += step) {
			if (!finalValidMask.at<uchar>(y, x)) continue; // 应用离群点过滤
			float depth_mm = dptr[x];

			// 反投影到相机坐标系（米）
			const float Zm = depth_mm / 1000.0f;
			const float X  = static_cast<float>((static_cast<double>(x) - cx) * Zm / fx);
			const float Y  = static_cast<float>((static_cast<double>(y) - cy) * Zm / fy);

			// 世界坐标语义：+Z 为前方（远离相机）
			QVector3D qPoint(X, -Y, Zm);
			points.push_back(qPoint);

			// 颜色：若有左图则取其BGR→RGB，否则使用白色
			QVector3D qColor(1.0f, 1.0f, 1.0f);
			if (!colorImageForPCL.empty() && colorImageForPCL.type() == CV_8UC3) {
				cv::Vec3b bgr = colorImageForPCL.at<cv::Vec3b>(y, x);
				qColor = QVector3D(bgr[2] / 255.0f, bgr[1] / 255.0f, bgr[0] / 255.0f);
			}
			colors.push_back(qColor);
			m_pointCloudPixelCoords.push_back(cv::Point2i(x, y));
		}
	}

	// 更新到OpenGL控件
	m_pointCloudWidget->updatePointCloud(points, colors);
	LOG_INFO(QString("简化点云生成完成: %1 点").arg(points.size()));

	// 视图设置
	m_pointCloudWidget->set2DImageView();
	m_pointCloudWidget->setShowAxes(false);
	showToast(this, "点云生成成功", 2000);
}

void MeasurementPage::completeReset()
{
    LOG_INFO("执行完整重置...");
    
    // 移除相机引用计数，确保释放相机资源
    using namespace SmartScope::Core::CameraUtils;
    MultiCameraManager& cameraManager = MultiCameraManager::instance();
    int refCount = cameraManager.removeReference(CLIENT_ID);
    LOG_INFO(QString("移除相机引用计数，当前计数: %1").arg(refCount));
    
    // 断开推理结果信号，避免接收旧的推理结果
    disconnect(&m_inferenceService, &SmartScope::InferenceService::inferenceCompleted,
              this, &MeasurementPage::handleInferenceResult);
    LOG_INFO("已断开推理服务信号连接");
    
    // 彻底重置推理服务状态，清空所有请求和结果
    m_inferenceService.resetService();
    LOG_INFO("已完全重置推理服务");
    
    // 获取新的会话ID
    qint64 newSessionId = m_inferenceService.getCurrentSessionId();
    LOG_INFO(QString("新的会话ID: %1").arg(newSessionId));
    
    // 重置所有图像数据
    m_leftImage.release();
    m_rightImage.release();
    m_leftRectifiedImage.release();
    m_rightRectifiedImage.release();
    m_depthMap.release();
    m_disparityMap.release(); // 清空视差图
    m_monoDepthCalibrated.release(); // 清空校准后的单目深度图
    m_inferenceInputLeftImage.release();
    m_displayImage.release();
    
    // 重置UI显示
    if (m_leftImageLabel) {
        m_leftImageLabel->clear();
        m_leftImageLabel->setText("等待捕获图像...");
    }
    
    if (m_depthImageLabel) {
        m_depthImageLabel->clear();
        m_depthImageLabel->setText("等待深度推理...");
    }
    
    // 重置视差图显示
    if (m_disparityImageLabel) {
        m_disparityImageLabel->clear();
        m_disparityImageLabel->setText("等待视差图...");
    }
    
	// 清空点云（保留显示，避免页面切换后白屏）
	// if (m_pointCloudWidget) {
	//     m_pointCloudWidget->clearPointCloud();
	//     m_pointCloudWidget->clearGeometryObjects();  // 使用正确的方法清除所有几何对象
	//     m_pointCloudWidget->update();
	// }
    
    // 重置测量点数据
    m_measurementPoints.clear();
    m_originalClickPoints.clear();
    m_pointCloudPixelCoords.clear();
    
    // 重置状态标志
    m_imagesReady = false;
    m_measurementState = MeasurementState::Idle;
    
    // 重置测量状态
    if (m_stateManager) {
        m_stateManager->setMeasurementMode(MeasurementMode::View);
    }
    
    // 清空测量对象
    if (m_measurementManager) {
        m_measurementManager->clearMeasurements();
    }
    
    // 隐藏测量相关界面
    if (m_typeSelectionPage) {
        m_typeSelectionPage->setVisible(false);
    }
    
    if (m_resultsPanel) {
        m_resultsPanel->setVisible(false);
    }
    
    // 清空结果面板内容
    if (m_resultsLayout) {
        // 移除所有小部件，除了标题
        QLayoutItem* item;
        while ((item = m_resultsLayout->takeAt(1)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
    }
    
    // 重置原始图像尺寸
    m_originalImageSize = QSize(0, 0);
    
    // 重新连接推理服务信号
    bool connected = connect(&m_inferenceService, &SmartScope::InferenceService::inferenceCompleted,
                           this, &MeasurementPage::handleInferenceResult, 
                           Qt::QueuedConnection);
    
    if (connected) {
        LOG_INFO("推理服务信号已重新连接");
    } else {
        LOG_ERROR("推理服务信号重新连接失败");
    }
    
    // 清空后重绘，应该显示没有测量的基础图像
    redrawMeasurementsOnLabel();
    
    // 清空点云中的几何对象
    updatePointCloudMeasurements();
    
    LOG_INFO("完整重置完成，所有状态和测量结果已清除");
}

void MeasurementPage::initMeasurementFeatures()
{
    LOG_INFO("初始化3D测量功能");
    
    // 初始化相机校正管理器
    initializeCorrectionManager();
    
    // 创建测量管理器
    m_measurementManager = new MeasurementManager(this);
    
    // 创建测量状态管理器
    m_stateManager = new MeasurementStateManager(this);
    
    // 创建图像交互管理器
    m_imageInteractionManager = new SmartScope::App::Ui::ImageInteractionManager(this);
    
    // 使用ImageInteractionManager替代直接处理图像交互
    if (m_imageInteractionManager) {
        LOG_INFO("初始化图像交互管理器");
        bool success = m_imageInteractionManager->initialize(
            m_leftImageLabel,
            m_stateManager,
            m_measurementManager,
            m_measurementRenderer,
            m_measurementCalculator,
            m_correctionManager
        );
        
        if (success) {
            LOG_INFO("图像交互管理器初始化成功");
            // 连接信号
            connect(m_imageInteractionManager, &SmartScope::App::Ui::ImageInteractionManager::measurementCompleted,
                    this, [this](MeasurementObject* obj) {
                        if (m_measurementManager && obj) {
                            m_measurementManager->addMeasurement(obj, true);
                        }
                    });
                    
            connect(m_imageInteractionManager, &SmartScope::App::Ui::ImageInteractionManager::updateUI,
                    this, &MeasurementPage::redrawMeasurementsOnLabel);
                    
            connect(m_imageInteractionManager, &SmartScope::App::Ui::ImageInteractionManager::showToastMessage,
                    this, [this](const QString& message, int duration) {
                        showToast(this, message, duration);
                    });
        } else {
            LOG_ERROR("图像交互管理器初始化失败");
            delete m_imageInteractionManager;
            m_imageInteractionManager = nullptr;
        }
    }
    
    // 明确设置只在左图区域启用点击事件，并默认禁用
    if (m_leftImageLabel) {
        m_leftImageLabel->setClickEnabled(false);
        LOG_INFO("初始化：默认禁用左图区域点击，仅在长度测量模式时启用");
    }
    
    // 创建测量类型选择页面
    m_typeSelectionPage = new MeasurementTypeSelectionPage(this);
    m_typeSelectionPage->setVisible(false); // 初始隐藏
    
    // 设置测量类型选择页面的位置和大小
    m_typeSelectionPage->setGeometry(this->rect());
    
    // 连接状态管理器信号
    connect(m_stateManager, &MeasurementStateManager::measurementModeChanged, 
            this, &MeasurementPage::handleMeasurementModeChanged);
    
    // 连接测量类型选择页面信号
    connect(m_typeSelectionPage, &MeasurementTypeSelectionPage::measurementTypeSelected, 
            this, &MeasurementPage::handleMeasurementTypeSelected);
    connect(m_typeSelectionPage, &MeasurementTypeSelectionPage::cancelSelection, 
            this, &MeasurementPage::handleMeasurementTypeSelectionCancelled);
    
    // 连接测量管理器信号
    connect(m_measurementManager, &MeasurementManager::measurementsChanged,
            this, [this]() {
                LOG_INFO("收到测量对象变更信号");
                // 更新点云
                updatePointCloudMeasurements();
                // 更新图像显示
                redrawMeasurementsOnLabel();
                // 更新剖面图控件的可见性
                updateProfileControlsVisibility(); // <-- 添加调用
            });
            
    connect(m_measurementManager, &MeasurementManager::measurementAdded,
            this, [this](MeasurementObject* obj) {
                LOG_INFO("收到测量对象添加信号");
                // 更新点云
                updatePointCloudMeasurements();
                // 更新图像显示
                redrawMeasurementsOnLabel();
                // 刷新UI
                update();
                if (m_leftImageLabel) {
                    m_leftImageLabel->update();
                }
                if (m_pointCloudWidget) {
                    m_pointCloudWidget->update();
                }
                
                // 如果是剖面测量对象，自动显示剖面图
                if (obj && obj->getType() == MeasurementType::Profile) {
                    LOG_INFO("检测到新添加的剖面测量对象，自动显示剖面图");
                    
                    if (m_profileChartPlot && m_profileChartManager) {
                        // 获取剖面数据
                        QVector<QPointF> profileData = obj->getProfileData();
                        
                        // 如果对象中没有剖面数据，尝试提取
                        if (profileData.isEmpty()) {
                            profileData = m_profileChartManager->extractProfileData(obj);
                        }
                        
                        // 如果获取到有效的剖面数据，更新图表并显示
                        if (!profileData.isEmpty()) {
                            QString title = QString("剖面图 - %1").arg(obj->getResult());
                            m_profileChartManager->updateChartData(profileData, title);
                            
                            // 确保图表可见
                            m_profileChartPlot->setVisible(true);
                            m_profileChartPlot->replot();
                            LOG_INFO("已自动显示剖面图");
                        }
                    }
                }
                
                updateProfileControlsVisibility(); // <-- 添加调用
            });
            
    connect(m_measurementManager, &MeasurementManager::measurementRemoved,
            this, [this](MeasurementObject* obj) {
                LOG_INFO("收到测量对象删除信号");
                // 更新点云
                updatePointCloudMeasurements();
                // 更新图像显示
                redrawMeasurementsOnLabel();
                // 刷新UI
                update();
                if (m_leftImageLabel) {
                    m_leftImageLabel->update();
                }
                if (m_pointCloudWidget) {
                    m_pointCloudWidget->update();
                }
                updateProfileControlsVisibility(); // <-- 添加调用
            });
    
    
    // 添加结果面板
    m_resultsPanel = new QWidget(this);
    m_resultsPanel->setObjectName("measurementResultsPanel");
    m_resultsPanel->setStyleSheet(
        "QWidget#measurementResultsPanel {"
        "   background-color: rgba(30, 30, 30, 220);"
        "   border-radius: 10px;"
        "   border: 1px solid #444444;"
        "}"
    );
    
    m_resultsLayout = new QVBoxLayout(m_resultsPanel);
    m_resultsLayout->setContentsMargins(10, 10, 10, 10);
    m_resultsLayout->setSpacing(5);
    
    QLabel* resultsTitle = new QLabel("测量结果", m_resultsPanel);
    resultsTitle->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: bold;"
        "}"
    );
    resultsTitle->setAlignment(Qt::AlignCenter);
    m_resultsLayout->addWidget(resultsTitle);
    
    // 初始默认隐藏
    // m_measurementControlsContainer->setVisible(false); // DELETE THIS LINE
    m_resultsPanel->setVisible(false);
    
    // 设置初始状态
    updateUIBasedOnMeasurementState();
    
    LOG_INFO("3D测量功能初始化完成");

    // 最后调用一次以设置初始状态
    updateProfileControlsVisibility(); 
}

void MeasurementPage::openMeasurementTypeSelection()
{
    LOG_INFO("打开测量类型选择页面");
    
    // 先隐藏菜单栏，防止界面元素重叠
    if (m_menuBar) {
        m_menuBar->setProperty("was_visible", m_menuBar->isVisible());
        m_menuBar->hide();
        LOG_INFO("暂时隐藏菜单栏");
    }
    
    // 显示测量类型选择页面
    m_typeSelectionPage->show();
    m_typeSelectionPage->raise();  // 确保在所有其他窗口部件之上
    m_typeSelectionPage->activateWindow();  // 激活窗口
    
    // 确保页面位置正确
    if (m_typeSelectionPage->parentWidget()) {
        QRect parentRect = m_typeSelectionPage->parentWidget()->rect();
        int statusBarHeight = 40; // 状态栏高度
        m_typeSelectionPage->setGeometry(
            parentRect.x(),
            parentRect.y() + statusBarHeight,
            parentRect.width(),
            parentRect.height() - statusBarHeight
        );
    }
}

void MeasurementPage::handleMeasurementTypeSelected(MeasurementType type)
{
    LOG_INFO(QString("用户选择测量类型: %1").arg(static_cast<int>(type)));
    
    // 使用状态管理器开始添加测量
    if (m_stateManager) {
        m_stateManager->startAddMeasurement(type);
    }
    
    // 显示对应的操作提示
    QString instructionText;
    switch (type) {
        case MeasurementType::Length:
            instructionText = "选择两个点";
            break;
        case MeasurementType::PointToLine:
            instructionText = "选择一个点和一条线";
            break;
        case MeasurementType::Depth:
            instructionText = "选择表面深度点";
            break;
        case MeasurementType::Area:
            instructionText = "选择多个点形成闭合区域";
            break;
        case MeasurementType::Polyline:
            instructionText = "选择多个点形成折线，点击完成按钮结束";
            break;
        case MeasurementType::Profile:
            instructionText = "选择两个点创建剖面线";
            break;
        case MeasurementType::MissingArea:
            instructionText = "选择4个点形成两条线段，自动计算交点，添加额外点后点击\"完成\"按钮完成绘制";
            break;
        default:
            instructionText = "点击图像或点云添加测量点";
            break;
    }
    
    // 显示操作提示 - 移除这段代码，因为没有m_measurementInstructionLabel成员变量
    // if (m_measurementInstructionLabel) {
    //     m_measurementInstructionLabel->setText(instructionText);
    // }
    
    // 关闭测量类型选择页面
    if (m_typeSelectionPage) {
        m_typeSelectionPage->hide();
    }
    
    // 确保菜单栏显示
    if (m_menuBar) {
        m_menuBar->show();
        m_menuBar->raise();
        m_menuBar->setVisible(true);
        LOG_INFO("测量类型选择后，显示菜单栏");
    }
    
    // 更新UI状态
    updateUIBasedOnMeasurementState();
}

void MeasurementPage::handleMeasurementTypeSelectionCancelled()
{
    LOG_INFO("取消测量类型选择");
    
    // 隐藏测量类型选择页面
    m_typeSelectionPage->setVisible(false);
    
    // 恢复菜单栏
    if (m_menuBar && m_menuBar->property("was_visible").toBool()) {
        m_menuBar->show();
        m_menuBar->setProperty("was_visible", false);
        LOG_INFO("恢复菜单栏显示");
    }
}

void MeasurementPage::handleMeasurementModeChanged(MeasurementMode mode)
{
    LOG_INFO(QString("测量模式变更为: %1").arg(static_cast<int>(mode)));
    
    // 更新UI状态
    updateUIBasedOnMeasurementState();
    
    // --- 移除检查和重置 m_currentProfileForChart 的逻辑 --- 
    // if (m_stateManager && m_stateManager->getActiveMeasurementType() != MeasurementType::Profile) {
    //     m_currentProfileForChart = nullptr;
    //     if (m_profileChartPlot) {
    //          m_profileChartPlot->setVisible(false);
    //     }
    // }
    // --- 结束移除 --- 
    
    // 确保图表在非 Profile 类型时仍然隐藏 (如果需要，保留这个逻辑)
    if (m_stateManager && m_stateManager->getActiveMeasurementType() != MeasurementType::Profile) {
         if (m_profileChartPlot) {
             m_profileChartPlot->setVisible(false); 
         }
    }
    
    // 根据模式设置指导文本和启用/禁用控件
    switch (mode) {
        case MeasurementMode::Add:
            {
                MeasurementType type = m_stateManager->getActiveMeasurementType();
                switch (type) {
                    case MeasurementType::Length:
                        // m_measurementInstructionLabel->setText("选择两个点测量长度");
                        // 清空测量点列表
                        m_measurementPoints.clear();
                        m_originalClickPoints.clear(); // 也清空2D点击点
                        // 只启用左图区域的点击功能用于添加测量点
                        if (m_leftImageLabel) {
                            m_leftImageLabel->setClickEnabled(true);
                            LOG_INFO("长度测量模式：启用左图区域的点击功能添加测量点");
                        }
                        break;
                    case MeasurementType::PointToLine:
                        // m_measurementInstructionLabel->setText("在图像上选择线的第一个端点"); // 修改提示文本
                        // 清空测量点列表
                        m_measurementPoints.clear();
                        m_originalClickPoints.clear(); // 也清空2D点击点
                        // --- 修改：启用图像点击 ---
                        if (m_leftImageLabel) {
                            m_leftImageLabel->setClickEnabled(true);
                            LOG_INFO("点到线测量模式：启用左图区域点击");
                        }
                        // --- 结束修改 ---
                        break;
                    case MeasurementType::Depth:
                        // m_measurementInstructionLabel->setText("在图像上选择平面第一个点"); // 修改提示
                        // 清空测量点列表
                        m_measurementPoints.clear();
                        m_originalClickPoints.clear(); 
                        // --- 修改：启用图像点击 ---
                        if (m_leftImageLabel) {
                            m_leftImageLabel->setClickEnabled(true);
                            LOG_INFO("深度(点到面)测量模式：启用左图区域点击");
                        }
                        // --- 结束修改 ---
                        break;
                    case MeasurementType::Area:
                        // m_measurementInstructionLabel->setText("选择多个点测量面积");
                        // 启用图像点击，允许在2D图像上选择点
                        if (m_leftImageLabel) {
                            m_leftImageLabel->setClickEnabled(true);
                        }
                        break;
                    case MeasurementType::Polyline:
                        // m_measurementInstructionLabel->setText("选择多个点测量折线长度");
                        // 修正：启用图像点击以在2D图上添加点
                        if (m_leftImageLabel) {
                            m_leftImageLabel->setClickEnabled(true); // <-- 改为 true
                             LOG_INFO("折线测量模式：启用左图区域点击");
                        }
                        break;
                    case MeasurementType::Profile:
                        // m_measurementInstructionLabel->setText("选择两个点创建剖面线");
                        if (m_leftImageLabel) {
                            m_leftImageLabel->setClickEnabled(true);
                        }
                        break;
                    case MeasurementType::MissingArea:
                        // m_measurementInstructionLabel->setText("选择两条线段的4个点和额外点");
                        // m_measurementInstructionLabel->setText("选择4个点形成两条线段，然后添加额外点，最后点击\"完成\"按钮");
                        if (m_leftImageLabel) {
                            m_leftImageLabel->setClickEnabled(true);
                            LOG_INFO("补缺测量模式：启用左图区域点击");
                        }
                        break;
                    default:
                        // m_measurementInstructionLabel->setText("点击点云上的点进行测量");
                        // 禁用图像点击，默认在点云中操作
                        if (m_leftImageLabel) {
                            m_leftImageLabel->setClickEnabled(true);
                        }
                        break;
                }
            }
            break;
        case MeasurementMode::Edit:
            // m_measurementInstructionLabel->setText("编辑测量点");
            // 禁用图像点击
            if (m_leftImageLabel) {
                m_leftImageLabel->setClickEnabled(false);
            }
            break;
        case MeasurementMode::Delete:
            // m_measurementInstructionLabel->setText("选择要删除的测量");
            // 禁用图像点击
            if (m_leftImageLabel) {
                m_leftImageLabel->setClickEnabled(false);
            }
            break;
        case MeasurementMode::View:
        default:
            // m_measurementInstructionLabel->setText("选择\"添加\"按钮开始测量");
            // 禁用图像点击
            if (m_leftImageLabel) {
                m_leftImageLabel->setClickEnabled(false);
            }
            break;
    }
    // 确保按钮可见性在模式切换后更新
    updateProfileControlsVisibility(); 
}

void MeasurementPage::cancelMeasurementOperation()
{
    LOG_INFO("取消当前测量操作");
    
    // 使用图像交互管理器清空测量点
    if (m_imageInteractionManager) {
        m_imageInteractionManager->clearCurrentMeasurementPoints();
    }
    
    // 使用状态管理器取消操作
    m_stateManager->cancelOperation();
    
    // 重新绘制以清除显示的测量点
    redrawMeasurementsOnLabel();
}

void MeasurementPage::completeMeasurementOperation()
{
    LOG_INFO("完成当前测量操作");

    MeasurementMode currentMode = m_stateManager->getMeasurementMode();
    MeasurementType currentType = m_stateManager->getActiveMeasurementType();

    // 检查是否处于添加模式
    if (currentMode == MeasurementMode::Add) {
        // 检查是否为折线测量
        if (currentType == MeasurementType::Polyline) {
            // 检查是否有足够的临时点
            QVector<QVector3D> tempPoints;
            QVector<QPoint> tempClickPoints;
            
            // 优先使用图像交互管理器中的临时点
            if (m_imageInteractionManager) {
                tempPoints = m_imageInteractionManager->getMeasurementPoints();
                tempClickPoints = m_imageInteractionManager->getOriginalClickPoints();
            }
            
            // 如果图像交互管理器中没有点，使用页面级别的临时点
            if (tempPoints.isEmpty() && !m_measurementPoints.isEmpty()) {
                tempPoints = m_measurementPoints;
                tempClickPoints = m_originalClickPoints;
            }
            
            if (tempPoints.size() >= 2) {
                LOG_INFO(QString("完成折线测量，点数: %1").arg(tempPoints.size()));
                
                // 计算总长度
                float totalLength = 0.0f;
                for (int i = 1; i < tempPoints.size(); i++) {
                    totalLength += (tempPoints[i] - tempPoints[i-1]).length();
                }
                
                // 创建永久测量对象
                MeasurementObject* measurement = new MeasurementObject(this);
                measurement->setType(MeasurementType::Polyline);
                measurement->setPoints(tempPoints);
                measurement->setOriginalClickPoints(tempClickPoints);
                measurement->setResult(QString("折线长度: %1 mm").arg(totalLength, 0, 'f', 2));
                
                LOG_INFO(QString("折线测量完成，总长度: %1 mm").arg(totalLength, 0, 'f', 2));
                
                // 添加到管理器
                if (m_measurementManager) {
                    m_measurementManager->addMeasurement(measurement);
                    showToast(this, QString("折线测量完成: %1 mm").arg(totalLength, 0, 'f', 2), 3000);
                } else {
                    LOG_ERROR("MeasurementManager 为空，无法添加测量对象");
                    delete measurement;
                    showToast(this, "保存测量失败", 2000);
                }
                
                // 清空临时点
                if (m_imageInteractionManager) {
                    m_imageInteractionManager->clearTemporaryPoints();
                }
                m_measurementPoints.clear();
                m_originalClickPoints.clear();
                
                // 切换回查看模式
                m_stateManager->completeOperation();
                updateUIBasedOnMeasurementState();
                redrawMeasurementsOnLabel();
                LOG_INFO("折线测量操作已完成并保存");
            } else {
                LOG_WARNING(QString("折线测量点数不足，当前点数: %1，需要至少2个点").arg(tempPoints.size()));
                showToast(this, "折线至少需要2个点，请继续添加点", 2000);
                // 不切换模式，允许用户继续添加点或取消
                return; // 提前返回，不执行后面的通用完成逻辑
            }
        } else if (currentType == MeasurementType::MissingArea) {
            // 检查图像交互管理器中的缺失面积测量状态
            if (!m_imageInteractionManager || !m_imageInteractionManager->hasMissingAreaIntersection()) {
                showToast(this, "请先完成前4个点的选择以计算交点", 2000);
                return;
            }

            // 获取多边形点（交点+后续点）
            QVector<QVector3D> polygonPoints = m_imageInteractionManager->getMissingAreaPolygonPoints();
            QVector<QPoint> polygonClickPoints = m_imageInteractionManager->getMissingAreaPolygonClickPoints();

            if (polygonPoints.size() < 3) {
                // 多边形至少需要3个点（交点+至少2个后续点）
                int additionalPointsNeeded = 3 - polygonPoints.size();
                showToast(this, QString("多边形至少需要3个点，请再添加%1个点").arg(additionalPointsNeeded), 2000);
                return;
            }

            // 有足够的点完成缺失面积测量
            LOG_INFO(QString("完成缺失面积测量，多边形有%1个点").arg(polygonPoints.size()));
            try {
                // 创建永久测量对象，只保存多边形点
                MeasurementObject* measurement = new MeasurementObject(this);
                measurement->setType(MeasurementType::MissingArea);
                measurement->setPoints(polygonPoints);  // 只保存多边形点
                measurement->setOriginalClickPoints(polygonClickPoints);  // 对应的2D点击点

                // 计算并设置结果
                if (m_measurementCalculator) {
                    m_measurementCalculator->calculateMeasurementResult(measurement);
                } else {
                    measurement->setResult("错误: 计算器无效");
                    LOG_ERROR("测量计算器无效，无法计算补缺面积");
                }

                // 添加到管理器
                if (m_measurementManager) {
                    m_measurementManager->addMeasurement(measurement);
                    LOG_INFO("补缺测量：对象已添加到管理器");
                    showToast(this, "补缺测量完成", 2000);
                } else {
                    LOG_ERROR("MeasurementManager 为空，无法添加测量对象");
                    delete measurement;
                    showToast(this, "创建测量失败", 2000);
                }

                // 清空临时点和缺失面积测量的专用变量
                m_measurementPoints.clear();
                m_originalClickPoints.clear();
                if (m_imageInteractionManager) {
                    m_imageInteractionManager->clearCurrentMeasurementPoints();
                }

                // 切换回查看模式
                m_stateManager->completeOperation();
                updateUIBasedOnMeasurementState();
                redrawMeasurementsOnLabel();
                LOG_INFO("缺失面积测量操作已完成并保存");
            } catch (...) {
                LOG_ERROR("创建补缺测量对象时发生异常");
                showToast(this, "创建测量失败", 2000);
                return;
            }
        } else if (currentType == MeasurementType::Area) {
            // 面积测量的完成逻辑 - 从图像交互管理器获取点
            if (!m_imageInteractionManager) {
                LOG_ERROR("图像交互管理器为空，无法完成面积测量");
                showToast(this, "系统错误，无法完成测量", 2000);
                return;
            }

            // 获取当前测量点
            QVector<QVector3D> currentPoints = m_imageInteractionManager->getCurrentMeasurementPoints();
            QVector<QPoint> currentClickPoints = m_imageInteractionManager->getOriginalClickPoints();

            if (currentPoints.size() >= 3) {
                LOG_INFO(QString("完成面积测量，点数: %1").arg(currentPoints.size()));
                try {
                    // 创建永久测量对象
                    MeasurementObject* measurement = new MeasurementObject(this);
                    measurement->setType(MeasurementType::Area);
                    measurement->setPoints(currentPoints);
                    measurement->setOriginalClickPoints(currentClickPoints);

                    // 计算并设置结果
                    if (m_measurementCalculator) {
                        m_measurementCalculator->calculateMeasurementResult(measurement);
                    } else {
                        measurement->setResult("错误: 计算器无效");
                        LOG_ERROR("测量计算器无效，无法计算面积");
                    }

                    // 添加到管理器
                    if (m_measurementManager) {
                        m_measurementManager->addMeasurement(measurement);
                        LOG_INFO("面积测量：对象已添加到管理器");
                        showToast(this, "面积测量完成", 2000);
                    } else {
                        LOG_ERROR("MeasurementManager 为空，无法添加测量对象");
                        delete measurement;
                        showToast(this, "创建测量失败", 2000);
                    }

                    // 清空临时点
                    m_measurementPoints.clear();
                    m_originalClickPoints.clear();
                    if (m_imageInteractionManager) {
                        m_imageInteractionManager->clearCurrentMeasurementPoints();
                    }

                    // 切换回查看模式
                    m_stateManager->completeOperation();
                    updateUIBasedOnMeasurementState();
                    redrawMeasurementsOnLabel();
                    LOG_INFO("面积测量操作已完成并保存");
                } catch (...) {
                    LOG_ERROR("创建面积测量对象时发生异常");
                    showToast(this, "创建测量失败", 2000);
                    return;
                }
            } else {
                LOG_WARNING(QString("面积测量点数不足，当前点数: %1，需要至少3个点").arg(currentPoints.size()));
                showToast(this, "面积至少需要3个点，请继续添加点", 2000);
                // 不切换模式，允许用户继续添加点或取消
                return; // 提前返回，不执行后面的通用完成逻辑
            }
        } else {
            // 对于其他测量类型，如果有点，可能也需要保存 (按需添加逻辑)
            LOG_INFO(QString("完成其他类型 (%1) 测量操作，临时点数: %2")
                      .arg(static_cast<int>(currentType)).arg(m_measurementPoints.size()));
             // 通用完成逻辑 (如果需要)
             if (m_measurementPoints.size() > 0) {
                // 例如，如果需要保存未完成的测量，可以在这里处理
                LOG_WARNING("完成操作时，非折线测量仍有临时点，这些点将被丢弃");
                m_measurementPoints.clear(); // 清除未保存的点
                m_originalClickPoints.clear();
             }
             m_stateManager->completeOperation();
             updateUIBasedOnMeasurementState();
             redrawMeasurementsOnLabel();
        }
    } else if (currentMode == MeasurementMode::Edit) {
        // 编辑模式的完成逻辑 (如果需要)
        LOG_INFO("完成编辑测量操作");
        m_stateManager->completeOperation();
        updateUIBasedOnMeasurementState();
        redrawMeasurementsOnLabel();
    } else {
        LOG_INFO("非添加或编辑模式，无需完成操作");
    }
}

void MeasurementPage::handleImageClicked(int imageX, int imageY, const QPoint& labelPoint)
{
    // 委托给图像交互管理器处理
    if (m_imageInteractionManager) {
        // 使用融合深度优先，其次当前深度图
        cv::Mat depthMap = m_depthMap;
        {
            SmartScope::InferenceService& inferenceService = SmartScope::InferenceService::instance();
            auto* processor = inferenceService.getComprehensiveProcessor();
            if (processor) {
                cv::Mat fused = processor->getIntermediateResult("fused");
                if (!fused.empty()) {
                    depthMap = fused;
                    LOG_INFO("测量点击：优先使用融合深度(final_fused_depth)");
                }
            }
        }
        
        // 使用校正裁剪后的图像进行测量
        cv::Mat displayImage;
        if (!m_inferenceInputLeftImage.empty()) {
            displayImage = m_inferenceInputLeftImage;
            m_imageInteractionManager->setDisplayImage(displayImage);
            LOG_DEBUG(QString("测量点击：使用校正裁剪后图像，尺寸: %1x%2")
                     .arg(displayImage.cols).arg(displayImage.rows));
        } else if (!m_leftImage.empty()) {
            displayImage = m_leftImage;
            m_imageInteractionManager->setDisplayImage(displayImage);
            LOG_WARNING(QString("测量点击：使用原始图像（应使用裁剪后图像），尺寸: %1x%2")
                       .arg(displayImage.cols).arg(displayImage.rows));
        } else {
            LOG_ERROR("无可用图像进行测量");
            showToast(this, "无可用图像", 2000);
            return;
        }
    
        // 调用图像交互管理器处理点击
        m_imageInteractionManager->handleImageClick(
            imageX, 
            imageY, 
            labelPoint, 
            depthMap, 
            m_pointCloudPixelCoords,
            [this](int x, int y, int radius) -> QVector3D {
                return findNearestPointInCloud(x, y, radius);
            }
        );
                     } else {
        LOG_ERROR("图像交互管理器未初始化，无法处理点击");
        showToast(this, "内部错误：图像交互管理器未初始化", 2000);
    }
}

// 在图像上绘制所有已保存的测量对象
QVector3D MeasurementPage::findNearestPointInCloud(int pixelX, int pixelY, int searchRadius)
{
    if (m_pointCloudWidget == nullptr || m_pointCloudPixelCoords.empty()) {
        LOG_ERROR("找不到点云数据，无法查找最近的3D点");
        return QVector3D(0, 0, 0);
    }

    LOG_INFO(QString("尝试在点云中查找像素(%1, %2)附近的点，搜索半径: %3")
            .arg(pixelX).arg(pixelY).arg(searchRadius));
    
    // 获取点云widget中的点数据
    size_t pointCount = m_pointCloudWidget->getPointCount();
    if (pointCount == 0 || pointCount != m_pointCloudPixelCoords.size()) {
        LOG_ERROR(QString("点云数据不一致: 点云控件中有 %1 个点，像素映射中有 %2 个点")
                .arg(pointCount).arg(m_pointCloudPixelCoords.size()));
        return QVector3D(0, 0, 0);
    }
    
    // 查找最近的点
    float minDistance = std::numeric_limits<float>::max();
    int nearestIndex = -1;
    
    for (size_t i = 0; i < m_pointCloudPixelCoords.size(); ++i) {
        const cv::Point2i& pix = m_pointCloudPixelCoords[i];
        float dx = pix.x - pixelX;
        float dy = pix.y - pixelY;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        if (distance < minDistance && distance <= searchRadius) {
            minDistance = distance;
            nearestIndex = i;
        }
    }
    
    if (nearestIndex >= 0) {
        // 获取点云中对应的3D点
        QVector3D point = m_pointCloudWidget->getPointAt(nearestIndex);
        
        // 获取点云居中时使用的偏移量，用于补偿坐标
        QVector3D cloudCenter = m_pointCloudWidget->getBoundingBoxCenter();
        
        // 返回带有补偿的点坐标
        QVector3D compensatedPoint = point + cloudCenter;
        
        LOG_INFO(QString("找到最近的点云点：索引=%1，像素坐标=(%2,%3)，原始3D坐标=(%4,%5,%6)，补偿后坐标=(%7,%8,%9)，距离=%10像素")
               .arg(nearestIndex)
               .arg(m_pointCloudPixelCoords[nearestIndex].x)
               .arg(m_pointCloudPixelCoords[nearestIndex].y)
               .arg(point.x(), 0, 'f', 4)
               .arg(point.y(), 0, 'f', 4)
               .arg(point.z(), 0, 'f', 4)
               .arg(compensatedPoint.x(), 0, 'f', 4)
               .arg(compensatedPoint.y(), 0, 'f', 4)
               .arg(compensatedPoint.z(), 0, 'f', 4)
               .arg(minDistance, 0, 'f', 2));
        
        return compensatedPoint;
    } else {
        LOG_WARNING(QString("在半径%1像素内找不到点云中的点").arg(searchRadius));
        return QVector3D(0, 0, 0);
    }
}

// 在点云中显示所有测量对象
void MeasurementPage::updatePointCloudMeasurements()
{
    // 确保点云控件存在
    if (!m_pointCloudWidget) {
        LOG_ERROR("点云控件不可用，无法显示测量对象");
        return;
    }
    
    // 确保渲染器存在
    if (!m_pointCloudRenderer) {
        LOG_ERROR("点云渲染器未初始化，无法显示测量对象");
        return;
    }
    
    // 确保点云控件可见
    m_pointCloudWidget->setVisible(true);
    
    // 获取所有测量对象
    if (m_measurementManager) {
        const QVector<MeasurementObject*>& measurements = m_measurementManager->getMeasurements();
        // 调用渲染器进行绘制
        m_pointCloudRenderer->renderMeasurements(measurements);
    } else {
        // 如果没有测量管理器，也应该清空旧的几何对象
        m_pointCloudRenderer->clearGeometryObjects();
        m_pointCloudRenderer->updateWidget();
    }
}

// 实现添加到历史记录方法
// 执行撤回操作
bool MeasurementPage::undoLastOperation()
{
    // 检查是否有可撤销的操作
    if (!m_measurementManager || !m_measurementManager->canUndo()) {
        LOG_INFO("没有可撤销的操作");
        return false;
    }
    
    LOG_INFO("执行撤销操作 (调用 MeasurementManager::undo)");
    
    // 使用MeasurementManager的undo方法进行标准撤销
    // UI 更新将由 measurementsChanged 信号触发
    bool result = m_measurementManager->undo();
    
    if (result) {
        LOG_INFO("MeasurementManager::undo() 返回成功");
        // UI 更新由信号槽处理，此处不再执行
    } else {
        LOG_INFO("MeasurementManager::undo() 返回失败");
    }
    
    return result;
}

// 辅助函数，检查两组点是否匹配
bool MeasurementPage::pointsMatch(const QVector<QVector3D>& points1, const QVector<QVector3D>& points2)
{
    if (points1.size() != points2.size()) {
        return false;
    }
    
    // 考虑误差范围，允许点的坐标有微小差异
    const float errorThreshold = 0.001f;
    
    for (int i = 0; i < points1.size(); ++i) {
        const QVector3D& p1 = points1[i];
        const QVector3D& p2 = points2[i];
        
        if (fabs(p1.x() - p2.x()) > errorThreshold ||
            fabs(p1.y() - p2.y()) > errorThreshold ||
            fabs(p1.z() - p2.z()) > errorThreshold) {
            return false;
        }
    }
    
    return true;
}

// 实现eventFilter方法，用于处理全局鼠标移动事件
bool MeasurementPage::eventFilter(QObject* watched, QEvent* event)
{
    // 禁用QSplitterHandle的鼠标事件，防止分隔条移动
    if (qobject_cast<QSplitterHandle*>(watched)) {
        // 阻止所有可能导致拖动的鼠标事件
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonRelease ||
            event->type() == QEvent::MouseMove ||
            event->type() == QEvent::MouseButtonDblClick) {
            return true; // 拦截鼠标事件
        }
    }
    
    // 优先检查是否与菜单按钮相关 - 如果是菜单按钮或其容器，直接返回false不处理
    if (m_menuBar) {
        bool isMenuButton = false;
        
        // 检查watched是否是菜单栏按钮或其容器
        if (watched == m_menuBar || watched == m_menuBar->backgroundPanel()) {
            isMenuButton = true;
        } else {
            // 检查是否是菜单栏的任何子按钮
            QList<MeasurementMenuButton*> buttons = m_menuBar->findChildren<MeasurementMenuButton*>();
            for (auto btn : buttons) {
                if (watched == btn) {
                    isMenuButton = true;
                    break;
                }
            }
        }
        
        // 如果是菜单按钮，直接交给默认处理器
        if (isMenuButton) {
            return false;
        }
    }
    
    // 检查页面可见性 - 如果页面不可见或不是当前活动页面，则不处理放大镜
    if (!isVisible() || !isActiveWindow()) {
        // 如果存在放大镜，则隐藏它
        if (m_magnifierManager) {
            m_magnifierManager->hideMagnifier();
            LOG_INFO("页面不可见，隐藏放大镜");
        }
        return QWidget::eventFilter(watched, event);
    }
    
    // 重要：过滤事件，只处理左图区域相关的对象
    // 检查是否为左图区域或其父容器，避免干扰按钮点击事件
    bool isLeftImageRelated = (watched == m_leftImageLabel || 
                             (m_leftImageLabel && watched == m_leftImageLabel->parentWidget()));
    
    // 如果不是左图相关的对象，直接传递给默认处理器
    if (!isLeftImageRelated) {
        return QWidget::eventFilter(watched, event);
    }
    
    // 处理鼠标事件
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        
        // 检查鼠标是否在左图像区域内
        bool isInLeftImage = false;
        
        if (m_leftImageLabel) {
            // 获取左图像标签的全局位置和大小
            QRect labelRect = m_leftImageLabel->geometry();
            QPoint globalPos = m_leftImageLabel->mapToGlobal(QPoint(0, 0));
            labelRect.moveTo(globalPos);
            
            // 检查是否在左图像区域内 - 确保是在图像区域内，而不是边缘
            isInLeftImage = labelRect.contains(mouseEvent->globalPos());
            
            // 获取图像像素
            QPixmap pixmap = m_leftImageLabel->pixmap(Qt::ReturnByValue);
            if (!pixmap.isNull()) {
                // 计算图像在标签中的位置
                QSize labelSize = m_leftImageLabel->size();
                QSize pixmapSize = pixmap.size();
                QSize scaledSize = pixmapSize.scaled(labelSize, Qt::KeepAspectRatio);
                
                // 图像在标签中的偏移（居中显示）
                int offsetX = (labelSize.width() - scaledSize.width()) / 2;
                int offsetY = (labelSize.height() - scaledSize.height()) / 2;
                
                // 图像区域
                QRect imgRect(offsetX, offsetY, scaledSize.width(), scaledSize.height());
                
                // 判断鼠标是否在图像区域内（而不是边框或标签的其他区域）
                QPoint localPos = m_leftImageLabel->mapFromGlobal(mouseEvent->globalPos());
                isInLeftImage = imgRect.contains(localPos);
            }
            
            LOG_INFO(QString("鼠标按下 - 是否在左图区域: %1, 位置: (%2, %3)")
                   .arg(isInLeftImage ? "是" : "否")
                   .arg(mouseEvent->pos().x()).arg(mouseEvent->pos().y()));
        }
        
        // 严格限制只有左图区域才触发放大镜，并且只在3D测量页面中
        if (mouseEvent->button() == Qt::LeftButton && isInLeftImage && isVisible() && isActiveWindow()) {
            // 创建或显示放大镜
            if (m_magnifierManager) {
                LOG_INFO("鼠标按下在左图区域，创建放大镜");
                m_magnifierManager->createMagnifier(m_contentWidget, m_leftImageLabel, m_leftAreaRatio);
                m_magnifierManager->setEnabled(true);
                m_magnifierManager->showMagnifier(); // 确保放大镜显示
                m_magnifierManager->updateMagnifierContent(m_leftImageLabel); // 立即更新内容
            }
        }
    } 
    else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            // 鼠标释放时隐藏放大镜
            if (m_magnifierManager) {
                m_magnifierManager->hideMagnifier();
                m_magnifierManager->setEnabled(false);
                LOG_INFO("鼠标释放，隐藏放大镜");
            }
        }
    }
    else if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        
        // 检查鼠标是否在左图像区域内
        bool isInLeftImage = false;
        if (m_leftImageLabel) {
            // 获取左图像标签的全局位置和大小
            QRect labelRect = m_leftImageLabel->geometry();
            QPoint globalPos = m_leftImageLabel->mapToGlobal(QPoint(0, 0));
            labelRect.moveTo(globalPos);
            
            // 检查鼠标是否在左图像区域内
            isInLeftImage = labelRect.contains(mouseEvent->globalPos());
            
            // 更精确地检查鼠标是否在图像区域内
            QPixmap pixmap = m_leftImageLabel->pixmap(Qt::ReturnByValue);
            if (!pixmap.isNull()) {
                // 计算图像在标签中的位置
                QSize labelSize = m_leftImageLabel->size();
                QSize pixmapSize = pixmap.size();
                QSize scaledSize = pixmapSize.scaled(labelSize, Qt::KeepAspectRatio);
                
                // 图像在标签中的偏移（居中显示）
                int offsetX = (labelSize.width() - scaledSize.width()) / 2;
                int offsetY = (labelSize.height() - scaledSize.height()) / 2;
                
                // 图像区域
                QRect imgRect(offsetX, offsetY, scaledSize.width(), scaledSize.height());
                
                // 判断鼠标是否在图像区域内（而不是边框或标签的其他区域）
                QPoint localPos = m_leftImageLabel->mapFromGlobal(mouseEvent->globalPos());
                isInLeftImage = imgRect.contains(localPos);
            }
        }
        
        // 检查是否按住左键
        bool leftButtonPressed = (mouseEvent->buttons() & Qt::LeftButton);
        
        // 只有在左图区域内且当前页面可见时，才显示放大镜
        if (leftButtonPressed && m_magnifierManager && m_magnifierManager->isEnabled() && 
            isInLeftImage && isVisible() && isActiveWindow()) {
            m_magnifierManager->updateMagnifierContent(m_leftImageLabel);
            m_magnifierManager->showMagnifier();
        } else {
            // 如果不在左图区域或没有按住鼠标左键，则隐藏放大镜
            if (m_magnifierManager) {
                m_magnifierManager->hideMagnifier();
            }
        }
    }
    else if (event->type() == QEvent::Leave) {
        // 鼠标离开窗口时隐藏放大镜
        if (m_magnifierManager) {
            m_magnifierManager->hideMagnifier();
            m_magnifierManager->setEnabled(false);
            LOG_INFO("鼠标离开窗口，隐藏放大镜");
        }
    }
    // 处理窗口激活/失活事件
    else if (event->type() == QEvent::WindowActivate) {
        // 窗口激活时，我们可能需要重新安装事件过滤器
        LOG_INFO("窗口激活，确保放大镜处于正确状态");
    }
    else if (event->type() == QEvent::WindowDeactivate) {
        // 窗口失活时隐藏放大镜
        if (m_magnifierManager) {
            m_magnifierManager->hideMagnifier();
            LOG_INFO("窗口失活，隐藏放大镜");
        }
    }
    // 处理应用程序状态变化
    else if (event->type() == QEvent::ApplicationStateChange) {
        QApplicationStateChangeEvent* stateEvent = static_cast<QApplicationStateChangeEvent*>(event);
        if (stateEvent->applicationState() != Qt::ApplicationActive) {
            // 应用程序非活动状态时隐藏放大镜
            if (m_magnifierManager) {
                m_magnifierManager->hideMagnifier();
                LOG_INFO("应用程序切换到非活动状态，隐藏放大镜");
            }
        }
    }
    
    // 默认行为：继续传递事件
    return QWidget::eventFilter(watched, event);
}

// 新增方法：创建放大镜
// void MeasurementPage::createMagnifier()
// {
//     // 整个方法实现删除
// }

// 新增：实现 resizeEvent 方法
void MeasurementPage::resizeEvent(QResizeEvent *event)
{
    BasePage::resizeEvent(event);
    
    // 更新菜单栏位置
    updateLayout();
    
    // 如果有图像，更新图像显示
    if (m_leftImageLabel) {
        QPixmap pixmap = m_leftImageLabel->pixmap(Qt::ReturnByValue);
        if (!pixmap.isNull()) {
            // 缩放图像以适应标签大小
            QPixmap scaledPixmap = pixmap.scaled(
                m_leftImageLabel->size(), 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation);
            m_leftImageLabel->setPixmap(scaledPixmap);
        }
    }
    
    // 如果有深度图，更新深度图显示
    if (m_depthImageLabel) {
        QPixmap pixmap = m_depthImageLabel->pixmap(Qt::ReturnByValue);
        if (!pixmap.isNull()) {
            // 缩放图像以适应标签大小
            QPixmap scaledPixmap = pixmap.scaled(
                m_depthImageLabel->size(), 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation);
            m_depthImageLabel->setPixmap(scaledPixmap);
        }
    }
    
    /* 视差图已移除
    // 如果有视差图，更新视差图显示
    if (m_disparityImageLabel) {
        QPixmap pixmap = m_disparityImageLabel->pixmap(Qt::ReturnByValue);
        if (!pixmap.isNull()) {
            // 缩放图像以适应标签大小
            QPixmap scaledPixmap = pixmap.scaled(
                m_disparityImageLabel->size(), 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation);
            m_disparityImageLabel->setPixmap(scaledPixmap);
        }
    }
    */
    
    // 更新测量对象的绘制
    redrawMeasurementsOnLabel();
    
    // 更新放大镜内容
    if (m_magnifierManager && m_magnifierManager->isEnabled() && m_leftImageLabel) {
        m_magnifierManager->updateMagnifierContent(m_leftImageLabel);
    }
}

// 更新放大镜内容
void MeasurementPage::updateMagnifierContent()
{
    // 检查放大镜管理器和左图像标签是否有效
    if (m_magnifierManager && m_leftImageLabel) {
        // 调用MagnifierManager的updateMagnifierContent方法
        m_magnifierManager->updateMagnifierContent(m_leftImageLabel);
        LOG_INFO("更新放大镜内容");
    }
}

// 显示消息提示框
void MeasurementPage::showToast(QWidget *parent, const QString &message, int duration)
{
    if (!parent) {
        parent = this;
    }
    
    LOG_INFO("显示提示消息: " + message);
    
    // 创建一个Toast通知并显示
    ToastNotification* toast = new ToastNotification(parent);
    toast->showMessage(message, duration);
}

/**
 * @brief 使用 MeasurementRenderer 重新绘制所有测量并更新左侧图像标签
 */
void MeasurementPage::redrawMeasurementsOnLabel()
{
    LOG_DEBUG("重绘所有测量标记");
    
    // 获取基础图像 - 必须使用校正裁剪后的图像（推理输入图像）
    cv::Mat baseImage;
    if (!m_inferenceInputLeftImage.empty()) {
        baseImage = m_inferenceInputLeftImage.clone();
        LOG_DEBUG(QString("重绘测量：使用校正裁剪后图像，尺寸: %1x%2")
                 .arg(baseImage.cols).arg(baseImage.rows));
    } else if (!m_leftImage.empty()) {
        baseImage = m_leftImage.clone();
        LOG_WARNING(QString("重绘测量：使用原始图像（应使用裁剪后图像），尺寸: %1x%2")
                   .arg(baseImage.cols).arg(baseImage.rows));
    } else {
        LOG_WARNING("无可用图像用于重绘测量");
        return;
    }

    // 使用图像交互管理器绘制测量
    if (m_imageInteractionManager) {
        // 设置当前显示图像
        m_imageInteractionManager->setDisplayImage(baseImage);
        
        // 重绘所有已保存的测量对象
        cv::Mat resultImage = m_imageInteractionManager->redrawMeasurements(
            baseImage,
            QSize(baseImage.cols, baseImage.rows)
        );
        
        // 绘制临时测量 (当前未完成的)
        m_imageInteractionManager->drawTemporaryMeasurement(resultImage);
        
        // 更新图像控件显示
        if (m_leftImageLabel && !resultImage.empty()) {
            QImage qImage = ImageProcessor::matToQImage(resultImage);
        if (!qImage.isNull()) {
                // 设置原始图像尺寸用于坐标转换
                m_leftImageLabel->setOriginalImageSize(QSize(baseImage.cols, baseImage.rows));
                
                m_leftImageLabel->setPixmap(QPixmap::fromImage(qImage.scaled(
                    m_leftImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                m_leftImageLabel->update();
            }
        }
        } else {
        LOG_WARNING("图像交互管理器未初始化，无法重绘测量");
        // 如果没有图像交互管理器，至少显示原始图像
        if (m_leftImageLabel && !baseImage.empty()) {
            QImage qImage = ImageProcessor::matToQImage(baseImage);
            if (!qImage.isNull()) {
                // 设置原始图像尺寸用于坐标转换
                m_leftImageLabel->setOriginalImageSize(QSize(baseImage.cols, baseImage.rows));
                
                m_leftImageLabel->setPixmap(QPixmap::fromImage(qImage.scaled(
                    m_leftImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                m_leftImageLabel->update();
            }
        }
    }
}


// --- 实现新的槽函数 ---
void MeasurementPage::openDeleteMeasurementDialog()
{
    if (!m_measurementManager) {
        LOG_ERROR("MeasurementManager 未初始化，无法打开删除对话框");
        return;
    }

    const auto& measurements = m_measurementManager->getMeasurements();
    if (measurements.isEmpty()) {
        this->showToast(this, "没有可删除的测量项"); // 调用本类的成员函数
        return;
    }

    // 如果对话框不存在，则创建它
    if (!m_deleteDialog) {
        m_deleteDialog = new SmartScope::App::Ui::MeasurementDeleteDialog(this); // 使用完整命名空间创建
        // 连接信号，当对话框发出删除请求时，调用本类的处理函数
        connect(m_deleteDialog, &SmartScope::App::Ui::MeasurementDeleteDialog::measurementToDelete,
                this, &MeasurementPage::handleDeleteMeasurementRequested);
    }

    // 填充或更新列表内容
    m_deleteDialog->populateList(measurements);

    // 调整对话框大小和位置（可选，可以根据需要添加）
    // m_deleteDialog->adjustSize(); 
    // m_deleteDialog->move(this->geometry().center() - m_deleteDialog->rect().center());

    // 以非模态方式显示对话框
    m_deleteDialog->show();
    m_deleteDialog->raise(); // 确保窗口在前面
    m_deleteDialog->activateWindow(); // 激活窗口以接收焦点
}

// 这个函数处理来自 MeasurementDeleteDialog 的删除请求信号
void MeasurementPage::handleDeleteMeasurementRequested(MeasurementObject* obj)
{
    if (obj && m_measurementManager) {
        // 简化日志，记录类型枚举值和结果
        LOG_INFO(QString("处理删除请求，对象类型: %1, 结果: %2")
                     .arg(static_cast<int>(obj->getType())) // 直接记录枚举整数值
                     .arg(obj->getResult()));
        // 调用管理器删除，它会发出 signalsChanged 信号，自动触发UI更新
        m_measurementManager->removeMeasurement(obj);
        
        // 如果对话框是打开的，从对话框列表中移除已删除项(或者重新填充)
        if (m_deleteDialog && m_deleteDialog->isVisible()) {
             m_deleteDialog->populateList(m_measurementManager->getMeasurements()); // 重新填充列表
        }
        
    } else {
        LOG_WARNING("收到无效的删除请求");
    }
}
// --- 结束实现新槽函数 ---

// --- 修改 resetMeasurementState() 函数实现 ---
void MeasurementPage::resetMeasurementState() {
    LOG_INFO("重置测量状态");

    // 使用图像交互管理器清空当前测量点
    if (m_imageInteractionManager) {
        m_imageInteractionManager->clearCurrentMeasurementPoints();
        LOG_INFO("已清空图像交互管理器中的测量点");
    }

    // 确保状态管理器存在
    if (!m_stateManager) {
        LOG_ERROR("状态管理器未初始化，无法完全重置状态");
        // 即使状态管理器不存在，也尝试重置其他部分
    } else {
         // 通知状态管理器回到 View 模式
        m_stateManager->setMeasurementMode(MeasurementMode::View);
        LOG_INFO("已通知状态管理器切换到 View 模式");
    }

    // 重置页面内部状态变量
    m_measurementState = MeasurementState::Idle;
    m_originalClickPoints.clear();
    m_measurementPoints.clear();

    // 重置 UI 交互状态
    if(m_leftImageLabel) m_leftImageLabel->setClickEnabled(false);
    setCursor(Qt::ArrowCursor);
    if(m_magnifierManager) {
      m_magnifierManager->setEnabled(false);
      m_magnifierManager->hideMagnifier();
    }
    if (m_typeSelectionPage && m_typeSelectionPage->isVisible()) {
        m_typeSelectionPage->hide();
    }

    // 强制 UI 刷新以清除临时绘制
    redrawMeasurementsOnLabel();
    updatePointCloudMeasurements();
    update();

    showToast(this, "测量已取消", 1500);
}

// --- 修改 handleIntelligentBack() 函数实现，调用新辅助函数 ---
void MeasurementPage::handleIntelligentBack() {
    LOG_INFO("处理智能返回按钮点击");

    if (!m_stateManager) { /* ... */ return; }

    if (m_stateManager->getMeasurementMode() != MeasurementMode::View) {
        LOG_INFO("当前模式非 View，中断测量操作");
        resetMeasurementState(); 
    } else {
        if (!m_measurementManager) { /* ... */ return; }
        
        if (!m_measurementManager->getMeasurements().isEmpty()) {
            LOG_INFO("View模式，有测量数据，询问是否清空");
            // 调用 DialogUtils 的静态方法
            QMessageBox::StandardButton reply = SmartScope::App::Ui::DialogUtils::showStyledConfirmationDialog( // <-- 使用完整命名空间
                this, // 传入父窗口指针
                "确认操作", 
                "当前页面有测量结果，确定要清空所有测量结果吗？",
                "清空",
                "取消"
            );
            
            if (reply == QMessageBox::Yes) { 
                LOG_INFO("用户确认清空测量结果");
                m_measurementManager->clearMeasurements(); 
                redrawMeasurementsOnLabel();
                updatePointCloudMeasurements();
                update();
                showToast(this, "测量结果已清空", 1500);
            } else {
                LOG_INFO("用户取消清空操作");
            }
        } else {
             LOG_INFO("View模式，无测量数据，询问是否返回主页");
             // 调用 DialogUtils 的静态方法
            QMessageBox::StandardButton reply = SmartScope::App::Ui::DialogUtils::showStyledConfirmationDialog( // <-- 使用完整命名空间
                this, // 传入父窗口指针
                "确认返回", 
                "确定要返回主页吗？",
                "返回主页",
                "取消"
            );

            if (reply == QMessageBox::Yes) {
                LOG_INFO("用户确认返回主页");
                PageManager *pageManager = qobject_cast<PageManager*>(parentWidget());
                if (pageManager) {
                    pageManager->switchToPage(PageType::Home);
                } else {
                    LOG_ERROR("无法获取 PageManager 实例！");
                }
            } else {
                LOG_INFO("用户取消返回主页");
            }
        }
    }
}

// 添加初始化工具栏按钮的方法
void MeasurementPage::initToolBarButtons()
{
    // 获取主窗口
    MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
    if (!mainWindow) {
        LOG_WARNING("无法获取主窗口，无法初始化工具栏按钮");
        return;
    }
    
    // 获取工具栏 - 明确使用 SmartScope 命名空间
    SmartScope::ToolBar* toolBar = mainWindow->getToolBar();
    if (!toolBar) {
        LOG_WARNING("无法获取工具栏，无法初始化工具栏按钮");
        return;
    }
    
    // 先检查按钮是否已存在，如果存在则先移除
    if (toolBar->getButton("profileChartButton")) {
        LOG_INFO("截面图表按钮已存在，先移除");
        toolBar->removeButton("profileChartButton");
    }
    
    if (toolBar->getButton("screenshotButton")) {
        LOG_INFO("截图按钮已存在，先移除");
        toolBar->removeButton("screenshotButton");
    }
    
    // 添加截图按钮到主工具栏，index=2（第三个位置）
    QPushButton* screenshotBtn = toolBar->addButton("screenshotButton", ":/icons/screenshot.svg", "截图", 2);
    connect(screenshotBtn, &QPushButton::clicked, this, &MeasurementPage::onScreenshot);
    LOG_INFO("截图按钮已添加到工具栏");
    
    // 添加截面图表按钮，index=8
    m_profileChartButton = toolBar->addButton("profileChartButton", ":/icons/measurement/profile.svg", "截面图表", 8);
    if (m_profileChartButton) {
        connect(m_profileChartButton, &QPushButton::clicked, this, &MeasurementPage::handleProfileButtonClick);
        
        m_profileChartButton->hide(); // 初始隐藏按钮
        LOG_INFO("截面图表按钮已添加到工具栏并初始隐藏");
    } else {
        LOG_ERROR("添加截面图表按钮失败");
        return; // 如果添加失败，后续逻辑无意义
    }

    // 新增：添加剖面旋转按钮，样式与现有按钮一致
    m_profileRotateLeftButton = toolBar->addButton("profileRotateLeftButton", ":/icons/turn_left.svg", "左旋剖面", 6);
    m_profileRotateRightButton = toolBar->addButton("profileRotateRightButton", ":/icons/turn_right.svg", "右旋剖面", 7);
    if (m_profileRotateLeftButton && m_profileRotateRightButton) {
        m_profileRotateLeftButton->hide();
        m_profileRotateRightButton->hide();
        connect(m_profileRotateLeftButton, &QPushButton::clicked, this, &MeasurementPage::rotateProfileLeft);
        connect(m_profileRotateRightButton, &QPushButton::clicked, this, &MeasurementPage::rotateProfileRight);
        LOG_INFO("剖面旋转按钮已添加并初始隐藏");
    } else {
        LOG_ERROR("添加剖面旋转按钮失败");
    }
    
    // 重新连接 PageManager 控制按钮可见性的逻辑
    PageManager* pageManager = mainWindow->findChild<PageManager*>();
    if (pageManager) {
        // 先断开可能的旧连接，避免重复
        disconnect(pageManager, &PageManager::pageChanged, this, nullptr);
        
        // 连接页面变化信号 - 捕获 toolBar 变量
        connect(pageManager, &PageManager::pageChanged, this, [this, toolBar](PageType pageType) {
            if (!toolBar) return; // 防御性检查
            if (pageType == PageType::Measurement) {
                LOG_INFO("切换到测量页，显示截图按钮并更新截面图表按钮可见性");
                toolBar->showButton("screenshotButton");
                updateProfileControlsVisibility(); 
            } else {
                LOG_INFO("切换到其他页面，隐藏截图按钮和截面图表按钮");
                toolBar->hideButton("screenshotButton");
                toolBar->hideButton("profileChartButton");
                toolBar->hideButton("profileRotateLeftButton");
                toolBar->hideButton("profileRotateRightButton");
                if (m_profileChartPlot) {
                    m_profileChartPlot->setVisible(false);
                }
            }
        });
        LOG_INFO("已重新连接页面变化信号以控制工具栏按钮可见性");
    } else {
        LOG_WARNING("无法获取页面管理器，无法连接页面变化信号");
    }
    
    // 确保当前在3D测量页面时，立即显示截图按钮
    PageType currentPage = PageType::Home; // 默认使用Home作为初始值
    if (pageManager) {
        currentPage = pageManager->getCurrentPageType();
    }
    if (currentPage == PageType::Measurement) {
        LOG_INFO("当前在3D测量页面，立即显示截图按钮");
        toolBar->showButton("screenshotButton");
        updateProfileControlsVisibility();
    }
}

void MeasurementPage::onScreenshot()
{
    // 直接截屏保存，走 ScreenshotManager 统一路径（~/data/Screenshots）
    bool success = m_screenshotManager && m_screenshotManager->captureFullScreen();
    QString path = m_screenshotManager ? m_screenshotManager->getLastScreenshotPath() : QString();
    if (success) {
        showToast(this, QString("屏幕截图已保存至: %1").arg(path), 2000);
    } else {
        showToast(this, "截图保存失败", 2000);
    }
}

void MeasurementPage::handleProfileButtonClick()
{
    LOG_INFO("处理剖面图表按钮点击事件");
    if (!m_profileChartManager || !m_measurementManager) return;
    MeasurementObject* selectedProfile = nullptr;
    const auto& measurements = m_measurementManager->getMeasurements();
    for (auto* measurement : measurements) {
        if (measurement && measurement->getType() == MeasurementType::Profile) { selectedProfile = measurement; break; }
    }
    if (!selectedProfile) { showToast(this, "未找到剖面测量对象"); return; }
    QVector<QPointF> profileData = selectedProfile->getProfileData();
    if (profileData.isEmpty() && m_profileChartManager) {
        profileData = m_profileChartManager->extractProfileData(selectedProfile);
        if (profileData.isEmpty()) { showToast(this, "无法提取剖面数据"); return; }
    }
    // 按当前累计旋转角度渲染
    if (m_profileRotationAngleDeg != 0.0) {
        double angle = m_profileRotationAngleDeg * M_PI / 180.0;
        QVector<QPointF> rotated; rotated.reserve(profileData.size());
        for (const auto& p : profileData) {
            double x = p.x(); double y = p.y();
            double rx = x * cos(angle) - y * sin(angle);
            double ry = x * sin(angle) + y * cos(angle);
            rotated.append(QPointF(rx, ry));
        }
        profileData.swap(rotated);
    }
    QString title = QString("剖面图 - %1").arg(selectedProfile->getResult());
    if (m_profileChartPlot && m_profileChartManager) {
        bool currentlyVisible = m_profileChartPlot->isVisible();
        if (!currentlyVisible) {
            m_profileChartManager->updateChartData(profileData, title);
            m_profileChartPlot->setVisible(true);
            showToast(this, "剖面图已显示");
        } else {
            m_profileChartPlot->setVisible(false);
            showToast(this, "剖面图已隐藏");
        }
        // 每次显示或隐藏时，不重置累计角度，便于连续微调
    }
}

void MeasurementPage::setDepthMode(SmartScope::InferenceService::DepthMode mode)
{
    m_depthMode = mode;
    SmartScope::InferenceService::instance().setDepthMode(mode);
    updateDepthModeUI();
    if (!m_depthMap.empty()) {
        generatePointCloud(m_depthMap, cv::Mat());
    }
}

void MeasurementPage::updateProfileControlsVisibility()
{
    bool profileMeasurementExists = false;
    MeasurementObject* profileObj = nullptr;
    if (m_profileChartManager && m_measurementManager) {
        profileMeasurementExists = m_profileChartManager->updateControlsVisibility(
            m_measurementManager->getMeasurements());
        if (profileMeasurementExists) {
            const auto& measurements = m_measurementManager->getMeasurements();
            for (auto* measurement : measurements) {
                if (measurement && measurement->getType() == MeasurementType::Profile) { profileObj = measurement; break; }
            }
        }
    }
    QPushButton* profileButton = nullptr;
    MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
    if (mainWindow) {
        SmartScope::ToolBar* toolBar = mainWindow->getToolBar();
        if (toolBar) profileButton = toolBar->getButton("profileChartButton");
        if (toolBar && m_profileRotateLeftButton == nullptr) m_profileRotateLeftButton = toolBar->getButton("profileRotateLeftButton");
        if (toolBar && m_profileRotateRightButton == nullptr) m_profileRotateRightButton = toolBar->getButton("profileRotateRightButton");
    }
    if (profileButton) {
        profileButton->setVisible(profileMeasurementExists);
        profileButton->setText("");
        if (m_profileRotateLeftButton) m_profileRotateLeftButton->setVisible(profileMeasurementExists);
        if (m_profileRotateRightButton) m_profileRotateRightButton->setVisible(profileMeasurementExists);
        if (profileMeasurementExists && m_profileChartPlot && !m_profileChartPlot->isVisible() && profileObj) {
            QVector<QPointF> profileData = profileObj->getProfileData();
            if (profileData.isEmpty() && m_profileChartManager) {
                profileData = m_profileChartManager->extractProfileData(profileObj);
            }
            // 按当前累计旋转角度渲染
            if (!profileData.isEmpty() && m_profileRotationAngleDeg != 0.0) {
                double angle = m_profileRotationAngleDeg * M_PI / 180.0;
                QVector<QPointF> rotated; rotated.reserve(profileData.size());
                for (const auto& p : profileData) {
                    double x = p.x(); double y = p.y();
                    double rx = x * cos(angle) - y * sin(angle);
                    double ry = x * sin(angle) + y * cos(angle);
                    rotated.append(QPointF(rx, ry));
                }
                profileData.swap(rotated);
            }
            if (!profileData.isEmpty() && m_profileChartManager) {
                QString title = QString("剖面图 - %1").arg(profileObj->getResult());
                m_profileChartManager->updateChartData(profileData, title);
                m_profileChartPlot->setVisible(true);
            }
        }
    }
}

void MeasurementPage::invokeBackConfirmationFromNav()
{
    handleIntelligentBack();
    }
    
void MeasurementPage::setDebugModeFromSettings(bool enabled)
{
	setDebugControlsEnabled(enabled);
}

void MeasurementPage::updateDepthModeUI()
{
	// 深度模式切换按钮已移除，当前无需更新任何按钮UI
	// 保留占位以维持接口一致性
}

void MeasurementPage::setDebugControlsEnabled(bool enabled)
{
	if (m_debugButton) {
		m_debugButton->setVisible(enabled);
	}
}

void MeasurementPage::rotateProfileLeft()
{
	MeasurementObject* selectedProfile = nullptr;
	if (m_measurementManager) {
		const auto& measurements = m_measurementManager->getMeasurements();
		for (auto* measurement : measurements) {
			if (measurement && measurement->getType() == MeasurementType::Profile) { selectedProfile = measurement; break; }
		}
	}
	if (!selectedProfile || !m_profileChartManager) return;
	QVector<QPointF> data = selectedProfile->getProfileData();
	if (data.isEmpty()) data = m_profileChartManager->extractProfileData(selectedProfile);
	if (data.isEmpty()) { showToast(this, "无法提取剖面数据"); return; }
	// 累计角度并旋转
	m_profileRotationAngleDeg -= 1.0;
	double angle = m_profileRotationAngleDeg * M_PI / 180.0;
	QVector<QPointF> rotated; rotated.reserve(data.size());
	for (const auto& p : data) {
		double x = p.x(); double y = p.y();
		double rx = x * cos(angle) - y * sin(angle);
		double ry = x * sin(angle) + y * cos(angle);
		rotated.append(QPointF(rx, ry));
	}

	// 旋转完成后重新计算起伏数值并更新测量结果
	updateProfileElevationResult(selectedProfile, rotated);

	m_profileChartManager->updateChartData(rotated, "剖面图");
}

void MeasurementPage::rotateProfileRight()
{
	MeasurementObject* selectedProfile = nullptr;
	if (m_measurementManager) {
		const auto& measurements = m_measurementManager->getMeasurements();
		for (auto* measurement : measurements) {
			if (measurement && measurement->getType() == MeasurementType::Profile) { selectedProfile = measurement; break; }
		}
	}
	if (!selectedProfile || !m_profileChartManager) return;
	QVector<QPointF> data = selectedProfile->getProfileData();
	if (data.isEmpty()) data = m_profileChartManager->extractProfileData(selectedProfile);
	if (data.isEmpty()) { showToast(this, "无法提取剖面数据"); return; }
	// 累计角度并旋转
	m_profileRotationAngleDeg += 1.0;
	double angle = m_profileRotationAngleDeg * M_PI / 180.0;
	QVector<QPointF> rotated; rotated.reserve(data.size());
	for (const auto& p : data) {
		double x = p.x(); double y = p.y();
		double rx = x * cos(angle) - y * sin(angle);
		double ry = x * sin(angle) + y * cos(angle);
		rotated.append(QPointF(rx, ry));
	}

	// 旋转完成后重新计算起伏数值并更新测量结果
	updateProfileElevationResult(selectedProfile, rotated);

	m_profileChartManager->updateChartData(rotated, "剖面图");
}

/**
 * @brief 更新剖面起伏测量结果
 * @param measurement 测量对象
 * @param profileData 旋转后的剖面数据
 */
void MeasurementPage::updateProfileElevationResult(MeasurementObject* measurement, const QVector<QPointF>& profileData)
{
	if (!measurement || profileData.isEmpty()) {
		return;
	}

	// 计算起伏统计信息
	float minElevation = std::numeric_limits<float>::max();
	float maxElevation = std::numeric_limits<float>::lowest();

	for (const QPointF& point : profileData) {
		float elevation = point.y();
		if (elevation < minElevation) minElevation = elevation;
		if (elevation > maxElevation) maxElevation = elevation;
	}

	float elevationRange = maxElevation - minElevation;

	// 更新测量结果
	QString resultText;
	if (elevationRange < 0.01f) {
		resultText = "表面平坦，起伏<0.01mm";
	} else {
		resultText = QString("起伏: %1 mm").arg(elevationRange, 0, 'f', 2);
	}

	measurement->setResult(resultText);

	// 确保与图表最值保持一致
	if (m_profileChartManager) {
		// 通知图表管理器更新显示
		m_profileChartManager->updateElevationStats(minElevation, maxElevation, elevationRange);
	}

	LOG_INFO(QString("剖面旋转后更新起伏结果: 最小高程=%1mm, 最大高程=%2mm, 起伏范围=%3mm")
			 .arg(minElevation, 0, 'f', 2)
			 .arg(maxElevation, 0, 'f', 2)
			 .arg(elevationRange, 0, 'f', 2));
}

// 相机校正管理器相关函数实现
// 这些函数将被添加到measurement_page.cpp文件末尾

void MeasurementPage::initializeCorrectionManager()
{
    LOG_INFO("初始化相机校正管理器...");

    // 创建校正配置
    SmartScope::Core::CorrectionConfig config;
    config.cameraParametersPath = QCoreApplication::applicationDirPath() + "/camera_parameters";
    config.imageSize = cv::Size(1280, 720);
    config.enableDistortionCorrection = true;
    config.enableStereoRectification = true;
    config.enableDepthCalibration = true;
    config.enableImageTransform = true;
    config.useHardwareAcceleration = true;
    config.precomputeMaps = true;

    // 创建校正管理器
    m_correctionManager = SmartScope::Core::CameraCorrectionFactory::createCustomCorrectionManager(config, this);
    
    if (m_correctionManager) {
        // 连接信号
        connect(m_correctionManager.get(), &SmartScope::Core::CameraCorrectionManager::correctionCompleted,
                this, &MeasurementPage::onCorrectionCompleted);
        connect(m_correctionManager.get(), &SmartScope::Core::CameraCorrectionManager::correctionError,
                this, &MeasurementPage::onCorrectionError);

        LOG_INFO("相机校正管理器初始化成功");
    } else {
        LOG_ERROR("相机校正管理器初始化失败");
    }
}

void MeasurementPage::onCorrectionCompleted(const SmartScope::Core::CorrectionResult& result)
{
    LOG_DEBUG(QString("校正完成: 成功=%1, 时间=%2ms, 校正类型=%3")
        .arg(result.success)
        .arg(result.processingTimeMs)
        .arg(static_cast<int>(result.appliedCorrections)));
}

void MeasurementPage::onCorrectionError(const QString& errorMessage)
{
    LOG_ERROR(QString("校正错误: %1").arg(errorMessage));
}
