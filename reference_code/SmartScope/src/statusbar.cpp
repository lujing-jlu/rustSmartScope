#include "statusbar.h"
#include <QStyleOption>
#include <QDebug>
#include <QSizePolicy>
#include <QApplication>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputDialog>
#include <algorithm>
#include "infrastructure/logging/logger.h"
#include "app/ui/temperature_icon.h"
#include "app/utils/device_controller.h"

// 使用日志宏
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

namespace SmartScope {

// 自定义电池图标实现
BatteryIcon::BatteryIcon(QWidget *parent)
    : QWidget(parent), m_level(0.0f), m_hasDecimal(false), m_notDetected(true), m_color(Qt::white)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void BatteryIcon::setBatteryLevel(int level)
{
    m_level = qBound(0, level, 100);
    m_hasDecimal = false;
    m_notDetected = false;

    // 根据电量设置颜色
    if (m_level > 60) {
        m_color = QColor("#4CAF50"); // 绿色
    } else if (m_level > 20) {
        m_color = QColor("#FFC107"); // 黄色
    } else {
        m_color = QColor("#F44336"); // 红色
    }

    update(); // 触发重绘
}

void BatteryIcon::setDecimalBatteryLevel(float level)
{
    m_level = qBound(0.0f, level, 100.0f);
    m_hasDecimal = true;
    m_notDetected = false;

    // 根据电量设置颜色
    if (m_level > 60) {
        m_color = QColor("#4CAF50"); // 绿色
    } else if (m_level > 20) {
        m_color = QColor("#FFC107"); // 黄色
    } else {
        m_color = QColor("#F44336"); // 红色
    }

    update(); // 触发重绘
}

void BatteryIcon::setNotDetected()
{
    m_notDetected = true;
    m_level = 0.0f;
    m_hasDecimal = false;
    m_color = Qt::white; // 白色表示未检测到

    update(); // 触发重绘
}

QSize BatteryIcon::sizeHint() const
{
    // 增加电池图标的尺寸，适应更高的状态栏
    return QSize(200, 60);  // 从180x60增加到200x60，确保有足够空间显示100%
}

void BatteryIcon::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 计算电池图标和文本的布局
    int batteryWidth = 70;
    int batteryHeight = height() - 24;
    int textWidth = width() - batteryWidth - 15;  // 增加文本区域宽度
    
    if (m_notDetected) {
        // 未检测到状态：绘制带问号的电池图标（白色）
        QRectF batteryBody(2, 12, batteryWidth - 8, batteryHeight);
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::transparent);
        painter.drawRoundedRect(batteryBody, 4, 4);

        // 电池正极（白色）
        QRectF batteryTip(batteryWidth - 6, batteryHeight / 2 - 4 + 12, 4, 8);
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawRect(batteryTip);

        // 在电池中心绘制问号（白色）
        painter.setPen(Qt::white);
        painter.setFont(QFont("WenQuanYi Zen Hei", 16, QFont::Bold));
        painter.drawText(batteryBody, Qt::AlignCenter, "?");
    } else {
        // 正常状态：绘制电池图标
        QRectF batteryBody(2, 12, batteryWidth - 8, batteryHeight);
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::transparent);
        painter.drawRoundedRect(batteryBody, 4, 4);

        // 电池正极
        QRectF batteryTip(batteryWidth - 6, batteryHeight / 2 - 4 + 12, 4, 8);
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawRect(batteryTip);

        // 电池电量
        if (m_level > 0) {
            QRectF levelRect(5, 15, (batteryWidth - 14) * m_level / 100.0, batteryHeight - 6);
            painter.setBrush(m_color);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(levelRect, 2, 2);
        }
    }
    
    // 绘制电量百分比文本或未检测到状态
    QString displayText;
    if (m_notDetected) {
        displayText = "未检测到";
        painter.setPen(Qt::white); // 白色文字
        painter.setFont(QFont("WenQuanYi Zen Hei", 20, QFont::Normal)); // 稍小的字体
    } else {
        if (m_hasDecimal) {
            // 带小数点的电量显示，保留一位小数
            displayText = QString::number(m_level, 'f', 1) + "%";
        } else {
            // 整数电量显示
            displayText = QString::number(static_cast<int>(m_level)) + "%";
        }
        painter.setPen(Qt::white);
        painter.setFont(QFont("WenQuanYi Zen Hei", 24, QFont::Bold)); // 保持字体大小
    }

    // 在电池图标右侧绘制文本，确保有足够空间
    QRectF textRect(batteryWidth + 8, 0, textWidth, height());  // 微调文本位置
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, displayText);
}

// 安卓风格文件选择对话框实现
AndroidStyleFileDialog::AndroidStyleFileDialog(const QString &rootDir, const QString &currentDir, QWidget *parent)
    : QDialog(parent), m_rootDirectory(rootDir), m_selectedPath(currentDir)
{
    // 计算相对路径用于显示在标题中
    QString displayPath;
    if (currentDir == rootDir) {
        displayPath = "/";
    } else if (currentDir.startsWith(rootDir)) {
        displayPath = currentDir.mid(rootDir.length());
        if (!displayPath.startsWith("/")) {
            displayPath = "/" + displayPath;
        }
    } else {
        displayPath = currentDir;
    }
    
    // 设置窗口标题和大小
    setWindowTitle("选择文件夹 - " + displayPath);
    resize(900, 800);  // 从700x600增加到900x800
    
    // 设置无边框窗口
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowOpacity(0.0); // 初始透明度为0
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20); // 增加边距以容纳阴影
    mainLayout->setSpacing(0);
    
    // 创建主容器，用于设置圆角和背景
    QWidget *container = new QWidget(this);
    container->setObjectName("dialogContainer");
    container->setStyleSheet(
        "QWidget#dialogContainer {"
        "   background-color: rgba(30, 30, 30, 240);"
        "   border-radius: 15px;"
        "   border: 1px solid rgba(80, 80, 80, 200);"
        "}"
    );
    
    // 添加阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 180));
    shadowEffect->setOffset(0, 0);
    container->setGraphicsEffect(shadowEffect);
    
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(15, 15, 15, 15);
    containerLayout->setSpacing(10);
    
    // 创建标题栏
    QWidget *titleBar = new QWidget(container);
    titleBar->setObjectName("titleBar");
    titleBar->setFixedHeight(50);
    titleBar->setStyleSheet(
        "QWidget#titleBar {"
        "   background-color: rgba(40, 40, 40, 200);"
        "   border-radius: 10px;"
        "}"
    );
    
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 0, 15, 0);
    
    // 创建标题标签
    QLabel *titleLabel = new QLabel("选择文件夹 - " + displayPath, titleBar);
    titleLabel->setStyleSheet(
        "color: white;"
        "font-size: 24px;"  // 从18px增加到24px
        "font-weight: bold;"
    );
    
    // 创建关闭按钮
    QPushButton *closeButton = new QPushButton(titleBar);
    closeButton->setFixedSize(60, 60);  // 增大按钮大小
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setIcon(QIcon(":/icons/close.svg"));
    closeButton->setIconSize(QSize(30, 30));  // 增大图标大小
    closeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #D9534F;"
        "   border-radius: 20px;"  // 设置为宽度的一半,确保是圆形
        "   border: none;"
        "   padding: 0px;"
        "   margin: 10px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #C9302C;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #B92C28;"
        "}"
    );
    
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(closeButton);
    
    containerLayout->addWidget(titleBar);
    
    // 创建文件系统模型
    m_model = new QFileSystemModel(this);
    m_model->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    m_model->setRootPath(rootDir);
    
    // 创建树视图
    m_treeView = new QTreeView(container);
    m_treeView->setModel(m_model);
    m_treeView->setRootIndex(m_model->index(rootDir));
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(20);
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(0, Qt::AscendingOrder);
    m_treeView->setStyleSheet(
        "QTreeView {"
        "   background-color: rgba(40, 40, 40, 200);"
        "   border-radius: 10px;"
        "   border: none;"
        "   color: white;"
        "   font-size: 24px;"  // 从16px增加到24px
        "   padding: 5px;"
        "}"
        "QTreeView::item {"
        "   height: 45px;"  // 从30px增加到45px
        "   padding: 5px;"
        "   border-radius: 5px;"
        "}"
        "QTreeView::item:hover {"
        "   background-color: rgba(60, 60, 60, 200);"
        "}"
        "QTreeView::item:selected {"
        "   background-color: rgba(0, 120, 215, 150);"
        "}"
        "QTreeView::branch {"
        "   background-color: transparent;"
        "}"
        "QTreeView::branch:has-children:!has-siblings:closed,"
        "QTreeView::branch:closed:has-children:has-siblings {"
        "   image: url(:/icons/right_arrow.svg);"
        "   color: white;"
        "}"
        "QTreeView::branch:open:has-children:!has-siblings,"
        "QTreeView::branch:open:has-children:has-siblings {"
        "   image: url(:/icons/down_arrow.svg);"
        "   color: white;"
        "}"
    );
    
    // 隐藏不需要的列
    m_treeView->hideColumn(1); // 大小
    m_treeView->hideColumn(2); // 类型
    m_treeView->hideColumn(3); // 修改日期
    
    // 设置当前选中的目录
    QModelIndex currentIndex = m_model->index(currentDir);
    m_treeView->setCurrentIndex(currentIndex);
    m_treeView->scrollTo(currentIndex);
    m_treeView->expand(currentIndex);
    
    // 连接信号
    connect(m_treeView, &QTreeView::doubleClicked, this, &AndroidStyleFileDialog::onItemDoubleClicked);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged, 
            this, &AndroidStyleFileDialog::onSelectionChanged);
    
    containerLayout->addWidget(m_treeView);
    
    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);
    
    // 创建新建文件夹按钮
    m_createFolderButton = new QPushButton("新建文件夹", container);
    m_createFolderButton->setCursor(Qt::PointingHandCursor);
    QString buttonStyle = 
        "QPushButton {"
        "   background-color: #2D2D2D;"
        "   color: white;"
        "   border-radius: 6px;"  // 从4px增加到6px
        "   padding: 12px 24px;"  // 从8px 16px增加到12px 24px
        "   font-size: 24px;"  // 从16px增加到24px
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3D3D3D;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4D4D4D;"
        "}";
    m_createFolderButton->setStyleSheet(buttonStyle);
    connect(m_createFolderButton, &QPushButton::clicked, this, &AndroidStyleFileDialog::onCreateFolder);
    
    // 创建重命名文件夹按钮
    m_renameFolderButton = new QPushButton("重命名", container);
    m_renameFolderButton->setCursor(Qt::PointingHandCursor);
    m_renameFolderButton->setStyleSheet(buttonStyle);
    connect(m_renameFolderButton, &QPushButton::clicked, this, &AndroidStyleFileDialog::onRenameFolder);
    
    // 创建删除文件夹按钮
    m_deleteFolderButton = new QPushButton("删除文件夹", container);
    m_deleteFolderButton->setCursor(Qt::PointingHandCursor);
    m_deleteFolderButton->setStyleSheet(buttonStyle);
    connect(m_deleteFolderButton, &QPushButton::clicked, this, &AndroidStyleFileDialog::onDeleteFolder);
    
    // 添加按钮到布局
    buttonLayout->addWidget(m_createFolderButton);
    buttonLayout->addWidget(m_renameFolderButton);
    buttonLayout->addWidget(m_deleteFolderButton);
    
    containerLayout->addLayout(buttonLayout);
    
    // 创建对话框按钮
    QDialogButtonBox *dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, container);
    dialogButtons->button(QDialogButtonBox::Ok)->setText("确定");
    dialogButtons->button(QDialogButtonBox::Cancel)->setText("取消");
    dialogButtons->button(QDialogButtonBox::Ok)->setCursor(Qt::PointingHandCursor);
    dialogButtons->button(QDialogButtonBox::Cancel)->setCursor(Qt::PointingHandCursor);
    dialogButtons->button(QDialogButtonBox::Ok)->setStyleSheet(buttonStyle);
    dialogButtons->button(QDialogButtonBox::Cancel)->setStyleSheet(buttonStyle);
    
    connect(dialogButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    containerLayout->addWidget(dialogButtons);
    
    // 添加主容器到主布局
    mainLayout->addWidget(container);
    
    // 设置对话框可拖动
    m_isDragging = false;
    titleBar->installEventFilter(this);
    
    // 设置滚动条样式
    setStyleSheet(
        "QScrollBar:vertical {"
        "   background-color: rgba(40, 40, 40, 100);"
        "   width: 12px;"
        "   margin: 0px;"
        "   border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background-color: rgba(80, 80, 80, 200);"
        "   min-height: 30px;"
        "   border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background-color: rgba(100, 100, 100, 200);"
        "}"
        "QScrollBar::handle:vertical:pressed {"
        "   background-color: rgba(120, 120, 120, 200);"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "   background: none;"
        "}"
    );
    
    // 添加淡入动画
    QTimer::singleShot(0, this, [this]() {
        QPropertyAnimation *animation = new QPropertyAnimation(this, "windowOpacity");
        animation->setDuration(300);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

bool AndroidStyleFileDialog::eventFilter(QObject *obj, QEvent *event)
{
    // 处理标题栏的鼠标事件，实现窗口拖动
    if (obj == findChild<QWidget*>("titleBar")) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragPosition = mouseEvent->globalPos() - frameGeometry().topLeft();
                m_isDragging = true;
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_isDragging && (mouseEvent->buttons() & Qt::LeftButton)) {
                move(mouseEvent->globalPos() - m_dragPosition);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_isDragging = false;
            return true;
        }
    }
    
    return QDialog::eventFilter(obj, event);
}

QString AndroidStyleFileDialog::selectedPath() const
{
    return m_selectedPath;
}

void AndroidStyleFileDialog::onItemDoubleClicked(const QModelIndex &index)
{
    QString path = m_model->filePath(index);
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir()) {
        m_selectedPath = path;
        accept();
    }
}

void AndroidStyleFileDialog::onCreateFolder()
{
    // 获取当前选中的目录
    QModelIndex currentIndex = m_treeView->currentIndex();
    QString currentPath = m_model->filePath(currentIndex);

    // 创建自定义输入对话框，保持无边框风格
    QDialog inputDialog(this);
    // 设置无边框但保持输入法兼容的窗口标志
    inputDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    inputDialog.setAttribute(Qt::WA_TranslucentBackground);
    inputDialog.setModal(true);
    inputDialog.setFixedSize(400, 200);
    inputDialog.setWindowOpacity(0.0); // 初始透明度为0

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(&inputDialog);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(0);

    // 创建主容器
    QWidget *container = new QWidget(&inputDialog);
    container->setObjectName("dialogContainer");
    container->setStyleSheet(
        "QWidget#dialogContainer {"
        "   background-color: rgba(30, 30, 30, 240);"
        "   border-radius: 15px;"
        "   border: 1px solid rgba(80, 80, 80, 200);"
        "}"
    );

    // 添加阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(&inputDialog);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 180));
    shadowEffect->setOffset(0, 0);
    container->setGraphicsEffect(shadowEffect);

    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(15, 15, 15, 15);
    containerLayout->setSpacing(15);

    // 创建标题标签
    QLabel *titleLabel = new QLabel("新建文件夹", container);
    titleLabel->setStyleSheet(
        "color: white;"
        "font-size: 18px;"
        "font-weight: bold;"
    );
    titleLabel->setAlignment(Qt::AlignCenter);

    // 创建输入框
    QLineEdit *folderNameEdit = new QLineEdit(container);
    folderNameEdit->setPlaceholderText("请输入文件夹名称");

    // 强化输入法支持 - 关键修复
    folderNameEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
    folderNameEdit->setInputMethodHints(Qt::ImhNone);
    folderNameEdit->setFocusPolicy(Qt::StrongFocus);
    folderNameEdit->setAttribute(Qt::WA_AcceptTouchEvents, true);
    folderNameEdit->setAcceptDrops(false);

    // 关键：设置输入框为可接收所有输入事件
    folderNameEdit->setAttribute(Qt::WA_KeyCompression, false);
    folderNameEdit->setAttribute(Qt::WA_InputMethodEnabled, true);

    folderNameEdit->setStyleSheet(
        "QLineEdit {"
        "   background-color: rgba(50, 50, 50, 200);"
        "   color: white;"
        "   border-radius: 8px;"
        "   padding: 10px;"
        "   font-size: 16px;"
        "   border: 1px solid rgba(80, 80, 80, 200);"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid rgba(0, 120, 215, 200);"
        "}"
    );
    folderNameEdit->setMinimumHeight(40);

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    // 创建确定按钮
    QPushButton *okButton = new QPushButton("确定", container);
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2D2D2D;"
        "   color: white;"
        "   border-radius: 4px;"
        "   padding: 8px 16px;"
        "   font-size: 16px;"
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3D3D3D;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4D4D4D;"
        "}"
    );

    // 创建取消按钮
    QPushButton *cancelButton = new QPushButton("取消", container);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2D2D2D;"
        "   color: white;"
        "   border-radius: 4px;"
        "   padding: 8px 16px;"
        "   font-size: 16px;"
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3D3D3D;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4D4D4D;"
        "}"
    );

    // 添加按钮到布局
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);

    // 添加组件到容器布局
    containerLayout->addWidget(titleLabel);
    containerLayout->addWidget(folderNameEdit);
    containerLayout->addLayout(buttonLayout);

    // 添加容器到主布局
    mainLayout->addWidget(container);

    // 连接信号
    connect(okButton, &QPushButton::clicked, &inputDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &inputDialog, &QDialog::reject);
    connect(folderNameEdit, &QLineEdit::returnPressed, &inputDialog, &QDialog::accept);

    // 显示对话框并添加淡入动画
    inputDialog.show();
    QPropertyAnimation *animation = new QPropertyAnimation(&inputDialog, "windowOpacity");
    animation->setDuration(300);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    // 关键修复：延迟设置焦点并强制激活输入法
    QTimer::singleShot(400, [folderNameEdit, &inputDialog]() {
        if (folderNameEdit && inputDialog.isVisible()) {
            // 确保对话框获得焦点
            inputDialog.activateWindow();
            inputDialog.raise();

            // 设置输入框焦点
            folderNameEdit->setFocus();
            folderNameEdit->activateWindow();

            // 强制显示输入法
            QInputMethod *inputMethod = QGuiApplication::inputMethod();
            if (inputMethod) {
                // 重要：先隐藏再显示，确保输入法重新初始化
                inputMethod->hide();
                QTimer::singleShot(100, [inputMethod, folderNameEdit]() {
                    if (inputMethod && folderNameEdit) {
                        inputMethod->show();
                        qDebug() << "强制激活输入法 - 状态:" << inputMethod->isVisible()
                                 << "输入框焦点:" << folderNameEdit->hasFocus();

                        // 如果还是不行，尝试发送一个虚拟的点击事件
                        QPoint center = folderNameEdit->rect().center();
                        QMouseEvent clickEvent(QEvent::MouseButtonPress, center, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QApplication::sendEvent(folderNameEdit, &clickEvent);
                        QMouseEvent releaseEvent(QEvent::MouseButtonRelease, center, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QApplication::sendEvent(folderNameEdit, &releaseEvent);
                    }
                });
            } else {
                qDebug() << "无法获取输入法实例";
            }
        }
    });

    // 执行对话框
    if (inputDialog.exec() == QDialog::Accepted) {
        QString folderName = folderNameEdit->text().trimmed();
        if (!folderName.isEmpty()) {
            // 创建新文件夹
            QString newFolderPath = currentPath + "/" + folderName;
            QDir dir;
            if (dir.mkpath(newFolderPath)) {
                LOG_INFO(QString("成功创建文件夹: %1").arg(newFolderPath));
                // 刷新视图
                m_model->setRootPath(m_model->rootPath());
            } else {
                LOG_ERROR(QString("创建文件夹失败: %1").arg(newFolderPath));
            }
        }
    }
}

void AndroidStyleFileDialog::onDeleteFolder()
{
    // 获取当前选中的目录
    QModelIndex currentIndex = m_treeView->currentIndex();
    QString currentPath = m_model->filePath(currentIndex);
    
    // 检查是否选中了根目录
    if (currentPath == m_rootDirectory) {
        // 创建自定义错误对话框
        QDialog errorDialog(this);
        errorDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        errorDialog.setAttribute(Qt::WA_TranslucentBackground);
        errorDialog.setFixedSize(400, 200);
        errorDialog.setWindowOpacity(0.0); // 初始透明度为0
        
        // 创建主布局
        QVBoxLayout *mainLayout = new QVBoxLayout(&errorDialog);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(0);
        
        // 创建主容器
        QWidget *container = new QWidget(&errorDialog);
        container->setObjectName("dialogContainer");
        container->setStyleSheet(
            "QWidget#dialogContainer {"
            "   background-color: rgba(30, 30, 30, 240);"
            "   border-radius: 15px;"
            "   border: 1px solid rgba(80, 80, 80, 200);"
            "}"
        );
        
        // 添加阴影效果
        QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(&errorDialog);
        shadowEffect->setBlurRadius(20);
        shadowEffect->setColor(QColor(0, 0, 0, 180));
        shadowEffect->setOffset(0, 0);
        container->setGraphicsEffect(shadowEffect);
        
        QVBoxLayout *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(15, 15, 15, 15);
        containerLayout->setSpacing(15);
        
        // 创建标题标签
        QLabel *titleLabel = new QLabel("错误", container);
        titleLabel->setStyleSheet(
            "color: #f44336;"
            "font-size: 18px;"
            "font-weight: bold;"
        );
        titleLabel->setAlignment(Qt::AlignCenter);
        
        // 创建消息标签
        QLabel *messageLabel = new QLabel("不能删除根目录！", container);
        messageLabel->setStyleSheet(
            "color: white;"
            "font-size: 16px;"
        );
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLabel->setWordWrap(true);
        
        // 创建按钮
        QPushButton *okButton = new QPushButton("确定", container);
        okButton->setCursor(Qt::PointingHandCursor);
        okButton->setStyleSheet(
            "QPushButton {"
            "   background-color: #2D2D2D;"
            "   color: white;"
            "   border-radius: 4px;"
            "   padding: 8px 16px;"
            "   font-size: 16px;"
            "   border: none;"
            "}"
            "QPushButton:hover {"
            "   background-color: #3D3D3D;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #4D4D4D;"
            "}"
        );
        
        // 添加组件到容器布局
        containerLayout->addWidget(titleLabel);
        containerLayout->addWidget(messageLabel);
        containerLayout->addWidget(okButton, 0, Qt::AlignCenter);
        
        // 添加容器到主布局
        mainLayout->addWidget(container);
        
        // 连接信号
        connect(okButton, &QPushButton::clicked, &errorDialog, &QDialog::accept);
        
        // 显示对话框并添加淡入动画
        errorDialog.show();
        QPropertyAnimation *animation = new QPropertyAnimation(&errorDialog, "windowOpacity");
        animation->setDuration(300);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->start(QAbstractAnimation::DeleteWhenStopped);
        
        // 执行对话框
        errorDialog.exec();
        return;
    }
    
    // 创建自定义确认对话框
    QDialog confirmDialog(this);
    confirmDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    confirmDialog.setAttribute(Qt::WA_TranslucentBackground);
    confirmDialog.setFixedSize(450, 200);
    confirmDialog.setWindowOpacity(0.0); // 初始透明度为0
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(&confirmDialog);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(0);
    
    // 创建主容器
    QWidget *container = new QWidget(&confirmDialog);
    container->setObjectName("dialogContainer");
    container->setStyleSheet(
        "QWidget#dialogContainer {"
        "   background-color: rgba(30, 30, 30, 240);"
        "   border-radius: 15px;"
        "   border: 1px solid rgba(80, 80, 80, 200);"
        "}"
    );
    
    // 添加阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(&confirmDialog);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 180));
    shadowEffect->setOffset(0, 0);
    container->setGraphicsEffect(shadowEffect);
    
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(15, 15, 15, 15);
    containerLayout->setSpacing(15);
    
    // 创建标题标签
    QLabel *titleLabel = new QLabel("确认删除", container);
    titleLabel->setStyleSheet(
        "color: #f44336;"
        "font-size: 18px;"
        "font-weight: bold;"
    );
    titleLabel->setAlignment(Qt::AlignCenter);
    
    // 获取文件夹名称
    QFileInfo fileInfo(currentPath);
    QString folderName = fileInfo.fileName();
    
    // 创建消息标签
    QLabel *messageLabel = new QLabel(QString("确定要删除文件夹 \"%1\" 吗？\n此操作不可恢复！").arg(folderName), container);
    messageLabel->setStyleSheet(
        "color: white;"
        "font-size: 16px;"
    );
    messageLabel->setAlignment(Qt::AlignCenter);
    messageLabel->setWordWrap(true);
    
    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);
    
    // 创建取消按钮
    QPushButton *cancelButton = new QPushButton("取消", container);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2D2D2D;"
        "   color: white;"
        "   border-radius: 4px;"
        "   padding: 8px 16px;"
        "   font-size: 16px;"
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3D3D3D;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4D4D4D;"
        "}"
    );
    
    // 创建确定按钮
    QPushButton *okButton = new QPushButton("删除", container);
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2D2D2D;"
        "   color: white;"
        "   border-radius: 4px;"
        "   padding: 8px 16px;"
        "   font-size: 16px;"
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3D3D3D;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4D4D4D;"
        "}"
    );
    
    // 添加按钮到布局
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    
    // 添加组件到容器布局
    containerLayout->addWidget(titleLabel);
    containerLayout->addWidget(messageLabel);
    containerLayout->addLayout(buttonLayout);
    
    // 添加容器到主布局
    mainLayout->addWidget(container);
    
    // 连接信号
    connect(okButton, &QPushButton::clicked, &confirmDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &confirmDialog, &QDialog::reject);
    
    // 显示对话框并添加淡入动画
    confirmDialog.show();
    QPropertyAnimation *animation = new QPropertyAnimation(&confirmDialog, "windowOpacity");
    animation->setDuration(300);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    
    // 执行对话框
    if (confirmDialog.exec() == QDialog::Accepted) {
        // 删除文件夹
        QDir dir(currentPath);
        if (dir.removeRecursively()) {
            // 刷新模型
            m_model->setRootPath(m_model->rootPath());
            
            // 选中父目录
            QModelIndex parentIndex = currentIndex.parent();
            m_treeView->setCurrentIndex(parentIndex);
            m_treeView->scrollTo(parentIndex);
            
            // 更新选中的路径
            m_selectedPath = m_model->filePath(parentIndex);
        } else {
            // 创建自定义错误对话框
            QDialog errorDialog(this);
            errorDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
            errorDialog.setAttribute(Qt::WA_TranslucentBackground);
            errorDialog.setFixedSize(400, 200);
            errorDialog.setWindowOpacity(0.0); // 初始透明度为0
            
            // 创建主布局
            QVBoxLayout *errorMainLayout = new QVBoxLayout(&errorDialog);
            errorMainLayout->setContentsMargins(20, 20, 20, 20);
            errorMainLayout->setSpacing(0);
            
            // 创建主容器
            QWidget *errorContainer = new QWidget(&errorDialog);
            errorContainer->setObjectName("dialogContainer");
            errorContainer->setStyleSheet(
                "QWidget#dialogContainer {"
                "   background-color: rgba(30, 30, 30, 240);"
                "   border-radius: 15px;"
                "   border: 1px solid rgba(80, 80, 80, 200);"
                "}"
            );
            
            // 添加阴影效果
            QGraphicsDropShadowEffect *errorShadowEffect = new QGraphicsDropShadowEffect(&errorDialog);
            errorShadowEffect->setBlurRadius(20);
            errorShadowEffect->setColor(QColor(0, 0, 0, 180));
            errorShadowEffect->setOffset(0, 0);
            errorContainer->setGraphicsEffect(errorShadowEffect);
            
            QVBoxLayout *errorContainerLayout = new QVBoxLayout(errorContainer);
            errorContainerLayout->setContentsMargins(15, 15, 15, 15);
            errorContainerLayout->setSpacing(15);
            
            // 创建标题标签
            QLabel *errorTitleLabel = new QLabel("错误", errorContainer);
            errorTitleLabel->setStyleSheet(
                "color: #f44336;"
                "font-size: 18px;"
                "font-weight: bold;"
            );
            errorTitleLabel->setAlignment(Qt::AlignCenter);
            
            // 创建消息标签
            QLabel *errorMessageLabel = new QLabel("无法删除文件夹，请检查权限或文件夹是否被占用。", errorContainer);
            errorMessageLabel->setStyleSheet(
                "color: white;"
                "font-size: 16px;"
            );
            errorMessageLabel->setAlignment(Qt::AlignCenter);
            errorMessageLabel->setWordWrap(true);
            
            // 创建按钮
            QPushButton *errorOkButton = new QPushButton("确定", errorContainer);
            errorOkButton->setCursor(Qt::PointingHandCursor);
            errorOkButton->setStyleSheet(
                "QPushButton {"
                "   background-color: #2D2D2D;"
                "   color: white;"
                "   border-radius: 4px;"
                "   padding: 8px 16px;"
                "   font-size: 16px;"
                "   border: none;"
                "}"
                "QPushButton:hover {"
                "   background-color: #3D3D3D;"
                "}"
                "QPushButton:pressed {"
                "   background-color: #4D4D4D;"
                "}"
            );
            
            // 添加组件到容器布局
            errorContainerLayout->addWidget(errorTitleLabel);
            errorContainerLayout->addWidget(errorMessageLabel);
            errorContainerLayout->addWidget(errorOkButton, 0, Qt::AlignCenter);
            
            // 添加容器到主布局
            errorMainLayout->addWidget(errorContainer);
            
            // 连接信号
            connect(errorOkButton, &QPushButton::clicked, &errorDialog, &QDialog::accept);
            
            // 显示对话框并添加淡入动画
            errorDialog.show();
            QPropertyAnimation *errorAnimation = new QPropertyAnimation(&errorDialog, "windowOpacity");
            errorAnimation->setDuration(300);
            errorAnimation->setStartValue(0.0);
            errorAnimation->setEndValue(1.0);
            errorAnimation->setEasingCurve(QEasingCurve::OutCubic);
            errorAnimation->start(QAbstractAnimation::DeleteWhenStopped);
            
            // 执行对话框
            errorDialog.exec();
        }
    }
}

void AndroidStyleFileDialog::onSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    
    if (current.isValid()) {
        m_selectedPath = m_model->filePath(current);
    }
}

void AndroidStyleFileDialog::onRenameFolder()
{
    // 获取当前选中的目录
    QModelIndex currentIndex = m_treeView->currentIndex();
    QString currentPath = m_model->filePath(currentIndex);
    
    // 检查是否选中了根目录
    if (currentPath == m_rootDirectory) {
        // 创建自定义错误对话框
        QDialog errorDialog(this);
        errorDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        errorDialog.setAttribute(Qt::WA_TranslucentBackground);
        errorDialog.setFixedSize(400, 200);
        errorDialog.setWindowOpacity(0.0); // 初始透明度为0
        
        // 创建主布局
        QVBoxLayout *mainLayout = new QVBoxLayout(&errorDialog);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(0);
        
        // 创建主容器
        QWidget *container = new QWidget(&errorDialog);
        container->setObjectName("dialogContainer");
        container->setStyleSheet(
            "QWidget#dialogContainer {"
            "   background-color: rgba(30, 30, 30, 240);"
            "   border-radius: 15px;"
            "   border: 1px solid rgba(80, 80, 80, 200);"
            "}"
        );
        
        // 添加阴影效果
        QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(&errorDialog);
        shadowEffect->setBlurRadius(20);
        shadowEffect->setColor(QColor(0, 0, 0, 180));
        shadowEffect->setOffset(0, 0);
        container->setGraphicsEffect(shadowEffect);
        
        QVBoxLayout *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(15, 15, 15, 15);
        containerLayout->setSpacing(15);
        
        // 创建标题标签
        QLabel *titleLabel = new QLabel("错误", container);
        titleLabel->setStyleSheet(
            "color: #f44336;"
            "font-size: 18px;"
            "font-weight: bold;"
        );
        titleLabel->setAlignment(Qt::AlignCenter);
        
        // 创建消息标签
        QLabel *messageLabel = new QLabel("不能重命名根目录！", container);
        messageLabel->setStyleSheet(
            "color: white;"
            "font-size: 16px;"
        );
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLabel->setWordWrap(true);
        
        // 创建按钮
        QPushButton *okButton = new QPushButton("确定", container);
        okButton->setCursor(Qt::PointingHandCursor);
        okButton->setStyleSheet(
            "QPushButton {"
            "   background-color: #2D2D2D;"
            "   color: white;"
            "   border-radius: 4px;"
            "   padding: 8px 16px;"
            "   font-size: 16px;"
            "   border: none;"
            "}"
            "QPushButton:hover {"
            "   background-color: #3D3D3D;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #4D4D4D;"
            "}"
        );
        
        // 添加组件到容器布局
        containerLayout->addWidget(titleLabel);
        containerLayout->addWidget(messageLabel);
        containerLayout->addWidget(okButton, 0, Qt::AlignCenter);
        
        // 添加容器到主布局
        mainLayout->addWidget(container);
        
        // 连接信号
        connect(okButton, &QPushButton::clicked, &errorDialog, &QDialog::accept);
        
        // 显示对话框并添加淡入动画
        errorDialog.show();
        QPropertyAnimation *animation = new QPropertyAnimation(&errorDialog, "windowOpacity");
        animation->setDuration(300);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->start(QAbstractAnimation::DeleteWhenStopped);
        
        // 执行对话框
        errorDialog.exec();
        return;
    }
    
    // 获取当前文件夹名称
    QFileInfo fileInfo(currentPath);
    QString oldFolderName = fileInfo.fileName();
    QString parentPath = fileInfo.path();
    
    // 创建自定义输入对话框
    QDialog inputDialog(this);
    // 修复：设置正确的窗口标志，确保输入法可以正常工作
    inputDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    inputDialog.setAttribute(Qt::WA_TranslucentBackground);
    inputDialog.setModal(true);  // 设置为模态对话框
    inputDialog.setFixedSize(400, 200);
    inputDialog.setWindowOpacity(0.0); // 初始透明度为0
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(&inputDialog);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(0);
    
    // 创建主容器
    QWidget *container = new QWidget(&inputDialog);
    container->setObjectName("dialogContainer");
    container->setStyleSheet(
        "QWidget#dialogContainer {"
        "   background-color: rgba(30, 30, 30, 240);"
        "   border-radius: 15px;"
        "   border: 1px solid rgba(80, 80, 80, 200);"
        "}"
    );
    
    // 添加阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(&inputDialog);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 180));
    shadowEffect->setOffset(0, 0);
    container->setGraphicsEffect(shadowEffect);
    
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(15, 15, 15, 15);
    containerLayout->setSpacing(15);
    
    // 创建标题标签
    QLabel *titleLabel = new QLabel("重命名文件夹", container);
    titleLabel->setStyleSheet(
        "color: white;"
        "font-size: 18px;"
        "font-weight: bold;"
    );
    titleLabel->setAlignment(Qt::AlignCenter);
    
    // 创建输入框
    QLineEdit *folderNameEdit = new QLineEdit(container);
    folderNameEdit->setText(oldFolderName); // 设置当前文件夹名称为默认值
    // 设置输入法支持
    folderNameEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
    folderNameEdit->setInputMethodHints(Qt::ImhNone);  // 允许所有输入法
    folderNameEdit->setAttribute(Qt::WA_KeyCompression, false);  // 禁用按键压缩
    folderNameEdit->setFocusPolicy(Qt::StrongFocus);  // 强焦点策略
    // 确保输入框可以接收所有事件
    folderNameEdit->setAttribute(Qt::WA_AcceptTouchEvents, true);
    folderNameEdit->setAcceptDrops(false);  // 禁用拖放，避免事件冲突
    folderNameEdit->setStyleSheet(
        "QLineEdit {"
        "   background-color: rgba(50, 50, 50, 200);"
        "   color: white;"
        "   border-radius: 8px;"
        "   padding: 10px;"
        "   font-size: 16px;"
        "   border: 1px solid rgba(80, 80, 80, 200);"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid rgba(0, 120, 215, 200);"
        "}"
    );
    folderNameEdit->setMinimumHeight(40);
    
    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    // 创建确定按钮
    QPushButton *okButton = new QPushButton("确定", container);
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2D2D2D;"
        "   color: white;"
        "   border-radius: 4px;"
        "   padding: 8px 16px;"
        "   font-size: 16px;"
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3D3D3D;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4D4D4D;"
        "}"
    );
    
    // 创建取消按钮
    QPushButton *cancelButton = new QPushButton("取消", container);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2D2D2D;"
        "   color: white;"
        "   border-radius: 4px;"
        "   padding: 8px 16px;"
        "   font-size: 16px;"
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3D3D3D;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4D4D4D;"
        "}"
    );
    
    // 添加按钮到布局
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    
    // 添加组件到容器布局
    containerLayout->addWidget(titleLabel);
    containerLayout->addWidget(folderNameEdit);
    containerLayout->addLayout(buttonLayout);
    
    // 添加容器到主布局
    mainLayout->addWidget(container);
    
    // 连接信号
    connect(okButton, &QPushButton::clicked, &inputDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &inputDialog, &QDialog::reject);
    connect(folderNameEdit, &QLineEdit::returnPressed, &inputDialog, &QDialog::accept);
    
    // 设置焦点到输入框并选中所有文本
    folderNameEdit->setFocus();
    folderNameEdit->selectAll();

    // 显示对话框并添加淡入动画
    inputDialog.show();
    QPropertyAnimation *animation = new QPropertyAnimation(&inputDialog, "windowOpacity");
    animation->setDuration(300);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    // 动画完成后重新设置焦点，确保输入法正常工作
    connect(animation, &QPropertyAnimation::finished, [folderNameEdit]() {
        if (folderNameEdit) {
            folderNameEdit->setFocus();
            folderNameEdit->selectAll();
            folderNameEdit->activateWindow();
            // 使用正确的方式激活输入法
            QInputMethod *inputMethod = QGuiApplication::inputMethod();
            if (inputMethod) {
                inputMethod->show();
            }
        }
    });
    
    // 执行对话框
    if (inputDialog.exec() == QDialog::Accepted) {
        QString newFolderName = folderNameEdit->text().trimmed();
        if (!newFolderName.isEmpty() && newFolderName != oldFolderName) {
            // 构建新路径
            QString newPath = parentPath + "/" + newFolderName;
            
            // 检查新名称是否已存在
            if (QFile::exists(newPath)) {
                // 创建自定义错误对话框
                QDialog errorDialog(this);
                errorDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
                errorDialog.setAttribute(Qt::WA_TranslucentBackground);
                errorDialog.setFixedSize(400, 200);
                errorDialog.setWindowOpacity(0.0); // 初始透明度为0
                
                // 创建主布局
                QVBoxLayout *errorMainLayout = new QVBoxLayout(&errorDialog);
                errorMainLayout->setContentsMargins(20, 20, 20, 20);
                errorMainLayout->setSpacing(0);
                
                // 创建主容器
                QWidget *errorContainer = new QWidget(&errorDialog);
                errorContainer->setObjectName("dialogContainer");
                errorContainer->setStyleSheet(
                    "QWidget#dialogContainer {"
                    "   background-color: rgba(30, 30, 30, 240);"
                    "   border-radius: 15px;"
                    "   border: 1px solid rgba(80, 80, 80, 200);"
                    "}"
                );
                
                // 添加阴影效果
                QGraphicsDropShadowEffect *errorShadowEffect = new QGraphicsDropShadowEffect(&errorDialog);
                errorShadowEffect->setBlurRadius(20);
                errorShadowEffect->setColor(QColor(0, 0, 0, 180));
                errorShadowEffect->setOffset(0, 0);
                errorContainer->setGraphicsEffect(errorShadowEffect);
                
                QVBoxLayout *errorContainerLayout = new QVBoxLayout(errorContainer);
                errorContainerLayout->setContentsMargins(15, 15, 15, 15);
                errorContainerLayout->setSpacing(15);
                
                // 创建标题标签
                QLabel *errorTitleLabel = new QLabel("错误", errorContainer);
                errorTitleLabel->setStyleSheet(
                    "color: #f44336;"
                    "font-size: 18px;"
                    "font-weight: bold;"
                );
                errorTitleLabel->setAlignment(Qt::AlignCenter);
                
                // 创建消息标签
                QLabel *errorMessageLabel = new QLabel("该名称已存在，请使用其他名称。", errorContainer);
                errorMessageLabel->setStyleSheet(
                    "color: white;"
                    "font-size: 16px;"
                );
                errorMessageLabel->setAlignment(Qt::AlignCenter);
                errorMessageLabel->setWordWrap(true);
                
                // 创建按钮
                QPushButton *errorOkButton = new QPushButton("确定", errorContainer);
                errorOkButton->setCursor(Qt::PointingHandCursor);
                errorOkButton->setStyleSheet(
                    "QPushButton {"
                    "   background-color: #2D2D2D;"
                    "   color: white;"
                    "   border-radius: 4px;"
                    "   padding: 8px 16px;"
                    "   font-size: 16px;"
                    "   border: none;"
                    "}"
                    "QPushButton:hover {"
                    "   background-color: #3D3D3D;"
                    "}"
                    "QPushButton:pressed {"
                    "   background-color: #4D4D4D;"
                    "}"
                );
                
                // 添加组件到容器布局
                errorContainerLayout->addWidget(errorTitleLabel);
                errorContainerLayout->addWidget(errorMessageLabel);
                errorContainerLayout->addWidget(errorOkButton, 0, Qt::AlignCenter);
                
                // 添加容器到主布局
                errorMainLayout->addWidget(errorContainer);
                
                // 连接信号
                connect(errorOkButton, &QPushButton::clicked, &errorDialog, &QDialog::accept);
                
                // 显示对话框并添加淡入动画
                errorDialog.show();
                QPropertyAnimation *errorAnimation = new QPropertyAnimation(&errorDialog, "windowOpacity");
                errorAnimation->setDuration(300);
                errorAnimation->setStartValue(0.0);
                errorAnimation->setEndValue(1.0);
                errorAnimation->setEasingCurve(QEasingCurve::OutCubic);
                errorAnimation->start(QAbstractAnimation::DeleteWhenStopped);
                
                // 执行对话框
                errorDialog.exec();
                return;
            }
            
            // 重命名文件夹
            QDir dir;
            if (dir.rename(currentPath, newPath)) {
                // 刷新模型
                m_model->setRootPath(m_model->rootPath());
                
                // 选中重命名后的文件夹
                QModelIndex newFolderIndex = m_model->index(newPath);
                m_treeView->setCurrentIndex(newFolderIndex);
                m_treeView->scrollTo(newFolderIndex);
                
                // 更新选中的路径
                m_selectedPath = newPath;
            } else {
                // 创建自定义错误对话框
                QDialog errorDialog(this);
                errorDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
                errorDialog.setAttribute(Qt::WA_TranslucentBackground);
                errorDialog.setFixedSize(400, 200);
                errorDialog.setWindowOpacity(0.0); // 初始透明度为0
                
                // 创建主布局
                QVBoxLayout *errorMainLayout = new QVBoxLayout(&errorDialog);
                errorMainLayout->setContentsMargins(20, 20, 20, 20);
                errorMainLayout->setSpacing(0);
                
                // 创建主容器
                QWidget *errorContainer = new QWidget(&errorDialog);
                errorContainer->setObjectName("dialogContainer");
                errorContainer->setStyleSheet(
                    "QWidget#dialogContainer {"
                    "   background-color: rgba(30, 30, 30, 240);"
                    "   border-radius: 15px;"
                    "   border: 1px solid rgba(80, 80, 80, 200);"
                    "}"
                );
                
                // 添加阴影效果
                QGraphicsDropShadowEffect *errorShadowEffect = new QGraphicsDropShadowEffect(&errorDialog);
                errorShadowEffect->setBlurRadius(20);
                errorShadowEffect->setColor(QColor(0, 0, 0, 180));
                errorShadowEffect->setOffset(0, 0);
                errorContainer->setGraphicsEffect(errorShadowEffect);
                
                QVBoxLayout *errorContainerLayout = new QVBoxLayout(errorContainer);
                errorContainerLayout->setContentsMargins(15, 15, 15, 15);
                errorContainerLayout->setSpacing(15);
                
                // 创建标题标签
                QLabel *errorTitleLabel = new QLabel("错误", errorContainer);
                errorTitleLabel->setStyleSheet(
                    "color: #f44336;"
                    "font-size: 18px;"
                    "font-weight: bold;"
                );
                errorTitleLabel->setAlignment(Qt::AlignCenter);
                
                // 创建消息标签
                QLabel *errorMessageLabel = new QLabel("无法重命名文件夹，请检查权限或文件夹是否被占用。", errorContainer);
                errorMessageLabel->setStyleSheet(
                    "color: white;"
                    "font-size: 16px;"
                );
                errorMessageLabel->setAlignment(Qt::AlignCenter);
                errorMessageLabel->setWordWrap(true);
                
                // 创建按钮
                QPushButton *errorOkButton = new QPushButton("确定", errorContainer);
                errorOkButton->setCursor(Qt::PointingHandCursor);
                errorOkButton->setStyleSheet(
                    "QPushButton {"
                    "   background-color: #2D2D2D;"
                    "   color: white;"
                    "   border-radius: 4px;"
                    "   padding: 8px 16px;"
                    "   font-size: 16px;"
                    "   border: none;"
                    "}"
                    "QPushButton:hover {"
                    "   background-color: #3D3D3D;"
                    "}"
                    "QPushButton:pressed {"
                    "   background-color: #4D4D4D;"
                    "}"
                );
                
                // 添加组件到容器布局
                errorContainerLayout->addWidget(errorTitleLabel);
                errorContainerLayout->addWidget(errorMessageLabel);
                errorContainerLayout->addWidget(errorOkButton, 0, Qt::AlignCenter);
                
                // 添加容器到主布局
                errorMainLayout->addWidget(errorContainer);
                
                // 连接信号
                connect(errorOkButton, &QPushButton::clicked, &errorDialog, &QDialog::accept);
                
                // 显示对话框并添加淡入动画
                errorDialog.show();
                QPropertyAnimation *errorAnimation = new QPropertyAnimation(&errorDialog, "windowOpacity");
                errorAnimation->setDuration(300);
                errorAnimation->setStartValue(0.0);
                errorAnimation->setEndValue(1.0);
                errorAnimation->setEasingCurve(QEasingCurve::OutCubic);
                errorAnimation->start(QAbstractAnimation::DeleteWhenStopped);
                
                // 执行对话框
                errorDialog.exec();
            }
        }
    }
}

// 路径选择器实现
PathSelector::PathSelector(QWidget *parent)
    : QPushButton(parent)
{
    // 从配置文件获取根目录
    m_rootDirectory = SmartScope::Infrastructure::ConfigManager::instance().getValue("app/root_directory", QDir::homePath() + "/data").toString();
    
    // 确保根目录存在
    QDir rootDir(m_rootDirectory);
    if (!rootDir.exists()) {
        rootDir.mkpath(".");
    }
    
    // 构建默认路径（根目录/Pictures）
    QString defaultPath = m_rootDirectory + "/Pictures";
    {
        QDir picturesDir(defaultPath);
        if (!picturesDir.exists()) {
            picturesDir.mkpath(".");
            LOG_INFO(QString("创建图片目录: %1").arg(defaultPath));
        }
    }
    
    // 初始化当前路径为默认路径
    m_currentPath = defaultPath;
    
    // 设置按钮文本为空，我们将在 paintEvent 中绘制内容
    setText("");
    
    // 设置按钮样式 - 更改为完全透明背景，只在paintEvent中绘制半透明背景
    setStyleSheet(
        "QPushButton {"
        "   background-color: transparent;"  // 背景透明，完全由paintEvent处理
        "   color: white;"
        "   border: none;"  // 去掉边框
        "   padding: 8px 15px;"  // 增大内边距
        "   font-size: 20px;"    // 增大字体大小
        "}"
        "QPushButton:hover {"
        "   background-color: transparent;"  // 保持透明
        "}"
        "QPushButton:pressed {"
        "   background-color: transparent;"  // 保持透明
        "}"
    );
    
    // 设置固定大小 - 增大按钮尺寸
    setFixedSize(240, 60);
    
    // 设置鼠标样式
    setCursor(Qt::PointingHandCursor);
    
    // 连接点击信号到槽函数
    connect(this, &QPushButton::clicked, this, &PathSelector::showFileDialog);
    
    LOG_INFO(QString("路径选择器初始化完成，根目录: %1，默认路径: %2").arg(m_rootDirectory).arg(m_currentPath));
}

void PathSelector::paintEvent(QPaintEvent *event)
{
    // 不调用基类的绘制事件，而是自己完全控制绘制
    Q_UNUSED(event);
    
    // 创建绘图对象
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制半透明背景
    QColor bgColor = isDown() ? QColor(100, 100, 100, 180) : // 按下状态
                   underMouse() ? QColor(80, 80, 80, 180) :  // 悬停状态
                                QColor(60, 60, 60, 180);    // 普通状态
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawRoundedRect(rect(), 15, 15);  // 使用15px圆角，与风格一致
    
    // 设置画笔和画刷绘制图标
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 220));  // 更亮的白色，增加可见度
    
    // 计算图标位置
    int iconSize = 20;
    int iconX = 10;
    int iconY = (height() - iconSize) / 2;
    
    // 绘制文件夹底部
    QRect folderBase(iconX, iconY + 4, iconSize, iconSize - 4);
    painter.drawRoundedRect(folderBase, 3, 3);  // 增大圆角
    
    // 绘制文件夹顶部
    QRect folderTop(iconX, iconY, iconSize * 0.7, 4);
    painter.drawRoundedRect(folderTop, 2, 2);  // 增大圆角
    
    // 绘制路径文本
    painter.setPen(QColor(255, 255, 255, 230));  // 更亮的白色文本
    painter.setFont(QFont("WenQuanYi Zen Hei", 20, QFont::Medium));  // 增大字体大小
    QRect textRect(iconX + iconSize + 10, 0, width() - iconX - iconSize - 20, height());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, getDisplayPath(m_currentPath));
}

QString PathSelector::getCurrentPath() const
{
    return m_currentPath;
}

QString PathSelector::getDisplayPath(const QString &path) const
{
    if (path == m_rootDirectory) {
        return "/"; // 根目录显示为"/"
    }
    
    // 确保路径在根目录下
    if (!path.startsWith(m_rootDirectory)) {
        return path; // 如果不在根目录下，显示完整路径
    }
    
    // 计算相对路径
    QString relativePath = path.mid(m_rootDirectory.length());
    
    // 确保相对路径以"/"开头
    if (!relativePath.startsWith("/")) {
        relativePath = "/" + relativePath;
    }
    
    return relativePath;
}

void PathSelector::setCurrentPath(const QString &path)
{
    // 确保路径在根目录下
    if (!path.startsWith(m_rootDirectory)) {
        LOG_WARNING(QString("尝试设置的路径不在根目录下: %1").arg(path));
        return;
    }
    
    m_currentPath = path;
    
    // 更新按钮显示
    update();
    
    emit pathChanged(path);
    
    LOG_INFO(QString("当前路径已更改为: %1").arg(path));
}

void PathSelector::showFileDialog()
{
    // 创建安卓风格文件选择对话框
    AndroidStyleFileDialog dialog(m_rootDirectory, m_currentPath, this);
    
    // 显示对话框并获取选择结果
    if (dialog.exec() == QDialog::Accepted) {
        QString selectedPath = dialog.selectedPath();
        if (!selectedPath.isEmpty()) {
            setCurrentPath(selectedPath);
        }
    }
}

// 状态栏组件实现
StatusBar::StatusBar(QWidget *parent) : 
    QWidget(parent),
      m_appNameLabel(nullptr),
      m_dateTimeLabel(nullptr),
      m_batteryIcon(nullptr),
      m_temperatureIconLabel(nullptr),
      m_temperatureTextLabel(nullptr),
      m_fpsLabel(nullptr),
      m_pathSelector(nullptr),
      m_dateTimeTimer(nullptr),
      m_batteryTimer(nullptr)
{
    // 设置独立组件所需的属性
    setObjectName("statusBar");
    
    // 确保状态栏可以接收鼠标事件
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);
    
    // 设置固定高度
    setFixedHeight(80);
    
    // 设置状态栏为完全透明
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    // 设置样式表 - 移除背景和边框设置，只保留基本样式
    setStyleSheet(
        "QWidget#statusBar {"
        "   color: white;"
        "   padding: 8px;"
        "}"
    );
    
    setupUI();
    startTimers();

    // 初始化设备控制器
    initDeviceController();

    // 检查是否需要显示帧率
    bool showFps = CONFIG_GET_VALUE("ui/show_fps", false).toBool();
    if (m_fpsLabel) {
        m_fpsLabel->setVisible(showFps);
        m_fpsLabel->parentWidget()->setVisible(showFps);
    }
}

int StatusBar::calculateOptimalWidth() const
{
    // 获取屏幕宽度
    QScreen *screen = QGuiApplication::primaryScreen();
    int screenWidth = screen ? screen->geometry().width() : 1920; // 默认值为1920
    
    // 状态栏占满屏幕宽度
    int totalWidth = screenWidth;
    
    LOG_INFO(QString("计算状态栏最佳宽度 (占满屏幕): %1").arg(totalWidth));
    
    return totalWidth;
}

void StatusBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    // 当状态栏显示时，调整大小
    adjustSizeToContent();
    
    // 确保状态栏可见
    raise();
    
    LOG_INFO("状态栏显示事件处理");
}

void StatusBar::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    
    LOG_INFO("状态栏隐藏事件处理");
}

void StatusBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // 状态栏大小变化时记录日志
    LOG_INFO(QString("状态栏大小变化: %1x%2").arg(width()).arg(height()));
}

void StatusBar::adjustSizeToContent()
{
    // 设置合适的宽度
    int optimalWidth = calculateOptimalWidth();
    setFixedWidth(optimalWidth);
    
    LOG_INFO(QString("调整状态栏大小: %1x%2").arg(width()).arg(height()));
}

StatusBar::~StatusBar()
{
    // 停止定时器
    if (m_dateTimeTimer) {
        m_dateTimeTimer->stop();
    }
    
    if (m_batteryTimer) {
        m_batteryTimer->stop();
    }
}

void StatusBar::setupUI()
{
    // 创建水平布局
    QHBoxLayout *layout = new QHBoxLayout(this);
    // 调整边距，适应更高的状态栏
    layout->setContentsMargins(16, 5, 16, 5);
    layout->setSpacing(16); // 保持组件间距
    
    // ==== 左侧区域：LOGO ====
    
    // 创建左侧布局（用于 LOGO 和 PathSelector，靠左对齐）
    QHBoxLayout *leftLayout = new QHBoxLayout();
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(16);
    
    // 创建应用名称标签
    m_appNameLabel = new QLabel("LOGO", this);
    
    // 创建应用名称面板（LOGO）
    QWidget *appNamePanel = new QWidget(this);
    appNamePanel->setStyleSheet("background-color: rgba(40, 40, 40, 180); border-radius: 30px;"); // 增大圆角为30px
    appNamePanel->setFixedHeight(60);
    appNamePanel->setMinimumWidth(150);
    
    QHBoxLayout *appNameLayout = new QHBoxLayout(appNamePanel);
    appNameLayout->setContentsMargins(15, 5, 15, 5);
    appNameLayout->addWidget(m_appNameLabel);
    
    m_appNameLabel->setStyleSheet(
        "color: white; "
        "font-size: 40px; "
        "font-weight: bold; "
        "font-family: 'WenQuanYi Zen Hei'; "
        "background: transparent; "
        "border: none;"
    );
    
    // 加载LOGO图片
    QPixmap logoPixmap(":/icons/EDDYSUN-logo.png");
    if (!logoPixmap.isNull()) {
        // 计算最大可用高度（考虑边距）
        int maxHeight = appNamePanel->height() - 20; // 增加上下边距到20像素
        // 根据高度计算等比例宽度
        int scaledWidth = (logoPixmap.width() * maxHeight) / logoPixmap.height();
        // 设置最小宽度，确保LOGO区域不会太窄，并增加左右边距
        appNamePanel->setMinimumWidth(scaledWidth + 40); // 40是左右边距的总和
        // 缩放图片
        m_appNameLabel->setPixmap(logoPixmap.scaled(scaledWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        LOG_WARNING("无法加载LOGO图片");
        m_appNameLabel->setText("LOGO");
    }
    
    // 添加LOGO到左侧布局
    leftLayout->addWidget(appNamePanel);
    
    // ==== 左侧区域：路径选择器 ====
    
    // 创建路径选择器
    m_pathSelector = new PathSelector(this);
    m_pathSelector->setObjectName("pathSelector");
    m_pathSelector->setCursor(Qt::PointingHandCursor);
    m_pathSelector->setFocusPolicy(Qt::StrongFocus);
    m_pathSelector->setMinimumWidth(240);
    m_pathSelector->setMaximumWidth(480);
    m_pathSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_pathSelector->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(60, 60, 60, 150);"
        "   color: white;"
        "   border-radius: 20px;"     // 增大圆角以匹配更大的按钮
        "   padding: 8px 20px;"       // 增大内边距
        "   font-size: 18px;"         // 增大字体大小
        "   text-align: left;"
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(80, 80, 80, 200);"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(100, 100, 100, 250);"
        "}"
    );
    
    // 创建路径选择器背景面板
    QWidget *pathSelectorPanel = new QWidget(this);
    // 使用半透明背景，移除固定的背景色，保留圆角
    pathSelectorPanel->setStyleSheet(
        "background-color: rgba(30, 30, 30, 160); " // 略微降低不透明度
        "border-radius: 35px; "                     // 增大圆角以匹配更大的按钮
        "border: none;"                             // 移除边框
    );
    pathSelectorPanel->setObjectName("pathSelectorPanel");
    pathSelectorPanel->setAutoFillBackground(false); // 不自动填充背景，使用样式表
    
    QHBoxLayout *pathSelectorLayout = new QHBoxLayout(pathSelectorPanel);
    pathSelectorLayout->setContentsMargins(20, 8, 20, 8);  // 增大内边距
    pathSelectorLayout->addWidget(m_pathSelector);
    
    // 添加路径选择器到左侧布局
    leftLayout->addWidget(pathSelectorPanel);
    
    // 将左侧布局添加到主布局
    layout->addLayout(leftLayout);
    
    // 添加弹性空间，推动剩余元素到右侧
    layout->addStretch(1);
    
    // ==== 右侧区域：帧率显示、时间和电量 ====
    
    // 创建右侧布局（用于帧率显示、时间和电池，靠右对齐）
    QHBoxLayout *rightLayout = new QHBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(16);
    
    // ==== 帧率显示 ====
    
    // 创建帧率标签
    m_fpsLabel = new QLabel("左: 0 FPS | 右: 0 FPS", this);
    
    // 创建帧率显示面板
    QWidget *fpsPanel = new QWidget(this);
    fpsPanel->setStyleSheet("background-color: rgba(40, 40, 40, 180); border-radius: 30px;"); // 增大圆角为30px
    fpsPanel->setObjectName("fpsPanel");
    fpsPanel->setAutoFillBackground(true);
    
    QHBoxLayout *fpsLayout = new QHBoxLayout(fpsPanel);
    fpsLayout->setContentsMargins(15, 5, 15, 5);
    fpsLayout->addWidget(m_fpsLabel);
    
    m_fpsLabel->setStyleSheet(
        "color: #CCCCCC; "
        "font-size: 28px; "
        "font-family: 'WenQuanYi Zen Hei'; "
        "background: transparent; "
        "border: none;"
    );
    
    // 设置最小宽度，确保有足够空间显示帧率
    fpsPanel->setMinimumWidth(300);
    
    // 添加帧率面板到右侧布局
    rightLayout->addWidget(fpsPanel);
    
    // ==== 右侧区域：时间 ====
    
    // 创建日期时间标签
    m_dateTimeLabel = new QLabel(this);
    
    // 创建日期时间面板
    QWidget *dateTimePanel = new QWidget(this);
    dateTimePanel->setStyleSheet("background-color: rgba(40, 40, 40, 180); border-radius: 30px;"); // 增大圆角为30px
    dateTimePanel->setObjectName("dateTimePanel");
    dateTimePanel->setAutoFillBackground(true);
    
    QHBoxLayout *dateTimeLayout = new QHBoxLayout(dateTimePanel);
    dateTimeLayout->setContentsMargins(15, 5, 15, 5);
    dateTimeLayout->addWidget(m_dateTimeLabel);
    
    m_dateTimeLabel->setStyleSheet(
        "color: #CCCCCC; "
        "font-size: 32px; "
        "font-family: 'WenQuanYi Zen Hei'; "
        "background: transparent; "
        "border: none;"
    );
    
    // 设置最小宽度，确保有足够空间显示日期时间
    dateTimePanel->setMinimumWidth(280);
    
    // 添加日期时间面板到右侧布局
    rightLayout->addWidget(dateTimePanel);
    
    // ==== 右侧区域：温度 ====

    // 创建温度状态面板（与电量面板相同的背景底纹）
    QWidget *temperaturePanel = new QWidget(this);
    temperaturePanel->setStyleSheet("background-color: rgba(40, 40, 40, 180); border-radius: 30px;"); // 与电量面板相同的背景
    temperaturePanel->setObjectName("temperaturePanel");
    temperaturePanel->setAutoFillBackground(true);

    QHBoxLayout *temperatureLayout = new QHBoxLayout(temperaturePanel);
    temperatureLayout->setContentsMargins(15, 5, 15, 5);
    temperatureLayout->setSpacing(10);

    // 创建温度SVG图标（无背景）
    m_temperatureIconLabel = new QLabel(this);
    m_temperatureIconLabel->setStyleSheet("background: transparent;"); // 去掉图标背景
    QPixmap tempPixmap(":/icons/temperature.svg");
    if (!tempPixmap.isNull()) {
        // 将图标转换为白色
        QPixmap whiteIcon(tempPixmap.size());
        whiteIcon.fill(Qt::transparent);
        QPainter painter(&whiteIcon);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawPixmap(0, 0, tempPixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(whiteIcon.rect(), QColor(255, 255, 255, 255));
        painter.end();

        QPixmap scaledIcon = whiteIcon.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_temperatureIconLabel->setPixmap(scaledIcon);
    }
    m_temperatureIconLabel->setFixedSize(40, 40);

    // 创建温度文本标签（与电量区域样式一致）
    m_temperatureTextLabel = new QLabel("未检测到", this);
    m_temperatureTextLabel->setStyleSheet(
        "background: transparent; "  // 去掉文字背景
        "color: white; "
        "font-size: 24px; "  // 与电量区域一致的字体大小
        "font-weight: bold; "
        "font-family: 'WenQuanYi Zen Hei';"
    );

    temperatureLayout->addWidget(m_temperatureIconLabel);
    temperatureLayout->addWidget(m_temperatureTextLabel);

    // 确保温度面板有足够的空间显示温度值
    temperaturePanel->setMinimumWidth(180);

    // 添加温度面板到右侧布局
    rightLayout->addWidget(temperaturePanel);
    
    // ==== 右侧区域：电量 ====
    
    // 创建自定义电池图标
    m_batteryIcon = new BatteryIcon(this);
    
    // 创建电池状态面板
    QWidget *batteryPanel = new QWidget(this);
    batteryPanel->setStyleSheet("background-color: rgba(40, 40, 40, 180); border-radius: 30px;"); // 增大圆角为30px
    batteryPanel->setObjectName("batteryPanel");
    batteryPanel->setAutoFillBackground(true);
    
    QHBoxLayout *batteryLayout = new QHBoxLayout(batteryPanel);
    batteryLayout->setContentsMargins(15, 5, 15, 5);
    batteryLayout->addWidget(m_batteryIcon);
    
    // 确保电池图标有足够的空间显示百分比
    batteryPanel->setMinimumWidth(100);
    
    // 添加电池面板到右侧布局
    rightLayout->addWidget(batteryPanel);
    
    // 将右侧布局添加到主布局
    layout->addLayout(rightLayout);
}

void StatusBar::startTimers()
{
    // 初始化并启动定时器
    m_dateTimeTimer = new QTimer(this);
    connect(m_dateTimeTimer, &QTimer::timeout, this, &StatusBar::updateDateTime);
    m_dateTimeTimer->start(1000); // 每秒更新一次时间
    
    // 初始化电池图标为未检测到状态
    if (m_batteryIcon) {
        m_batteryIcon->setNotDetected();
    }

    // 初始化温度显示为未检测到状态
    if (m_temperatureTextLabel) {
        m_temperatureTextLabel->setText("未检测到");
        m_temperatureTextLabel->setStyleSheet(
            "background: transparent; "
            "color: white; "  // 与电量区域一致，使用白色
            "font-size: 24px; "  // 与电量区域一致的字体大小
            "font-weight: bold; "
            "font-family: 'WenQuanYi Zen Hei';"
        );
    }

    // 不再创建模拟电池定时器，只依赖硬件数据
    m_batteryTimer = nullptr;
}

void StatusBar::updateDateTime()
{
    // 获取当前日期时间
    QDateTime now = QDateTime::currentDateTime();
    
    // 格式化为更友好的格式，使用斜线代替年月日
    QString dateText = now.toString("yyyy/MM/dd");
    QString timeText = now.toString("HH:mm:ss");
    
    // 更新标签
    m_dateTimeLabel->setText(dateText + " " + timeText);
}

void StatusBar::updateBatteryStatus()
{
    // 此方法已废弃，不再使用模拟数据
    // 电池状态现在只通过硬件设备控制器更新
    LOG_DEBUG("updateBatteryStatus 方法被调用，但已废弃（不再使用模拟数据）");
}

void StatusBar::updateFpsDisplay(float leftFps, float rightFps)
{
    // 检查是否需要显示帧率
    bool showFps = SmartScope::Infrastructure::ConfigManager::instance().getValue("ui/show_fps", false).toBool();
    
    if (m_fpsLabel) {
        if (showFps) {
    // 更新帧率显示
    QString fpsText = QString("左: %1 FPS | 右: %2 FPS").arg(leftFps, 0, 'f', 1).arg(rightFps, 0, 'f', 1);
    m_fpsLabel->setText(fpsText);
            m_fpsLabel->setVisible(true);
            m_fpsLabel->parentWidget()->setVisible(true);  // 显示帧率面板
        } else {
            // 隐藏帧率显示
            m_fpsLabel->setVisible(false);
            m_fpsLabel->parentWidget()->setVisible(false);  // 隐藏帧率面板
        }
    
    // 记录日志
        LOG_DEBUG(QString("更新帧率显示 - 左: %1 FPS, 右: %2 FPS, 显示状态: %3")
                 .arg(leftFps, 0, 'f', 1)
                 .arg(rightFps, 0, 'f', 1)
                 .arg(showFps ? "显示" : "隐藏"));
    }
} 

PathSelector* StatusBar::getPathSelector() const
{
    return m_pathSelector;
}

void SmartScope::StatusBar::updateTemperatureDisplay(float temperature)
{
    // 更新温度显示
    if (m_temperatureTextLabel) {
        // 如果温度值为负数，显示为未检测到
        if (temperature < 0) {
            m_temperatureTextLabel->setText("未检测到");
            m_temperatureTextLabel->setStyleSheet(
                "background: transparent; "
                "color: white; "  // 与电量区域一致，使用白色
                "font-size: 24px; "  // 与电量区域一致的字体大小
                "font-weight: bold; "
                "font-family: 'WenQuanYi Zen Hei';"
            );
        } else {
            // 显示温度值
            m_temperatureTextLabel->setText(QString::number(temperature, 'f', 1) + "°C");

            // 保持与电量区域一致的白色文字样式
            m_temperatureTextLabel->setStyleSheet(
                "background: transparent; "
                "color: white; "  // 与电量区域一致，使用白色
                "font-size: 24px; "  // 与电量区域一致的字体大小
                "font-weight: bold; "
                "font-family: 'WenQuanYi Zen Hei';"
            );
        }

        LOG_DEBUG(QString("更新温度显示: %1°C").arg(temperature, 0, 'f', 1));
    }
}

void SmartScope::StatusBar::onDeviceStatusUpdated(const DeviceController::DeviceStatus& status)
{
    if (!status.isValid) {
        return;
    }
    
    // 更新温度显示
    updateTemperatureDisplay(status.temperature);
    
    // 更新电量显示（使用带小数的电量值）
    if (m_batteryIcon) {
        m_batteryIcon->setDecimalBatteryLevel(status.batteryValue);
        LOG_DEBUG(QString("更新设备状态显示 - 温度: %1°C, 电量: %2%")
                 .arg(status.temperature, 0, 'f', 1)
                 .arg(status.batteryValue, 0, 'f', 1));
    }
}

void SmartScope::StatusBar::initDeviceController()
{
    LOG_INFO("初始化设备控制器连接...");
    
    // 连接设备状态更新信号
    connect(&DeviceController::instance(), &DeviceController::deviceStatusUpdated,
            this, &StatusBar::onDeviceStatusUpdated);
    
    // 连接温度变化信号
    connect(&DeviceController::instance(), &DeviceController::temperatureChanged,
            this, &StatusBar::updateTemperatureDisplay);
    
    // 连接电量变化信号
    connect(&DeviceController::instance(), &DeviceController::batteryLevelChanged,
            this, [this](int level) {
                if (m_batteryIcon) {
                    m_batteryIcon->setDecimalBatteryLevel(static_cast<float>(level));
                }
            });
    
    // 尝试初始化设备控制器
    if (DeviceController::instance().initialize()) {
        LOG_INFO("设备控制器初始化成功，启动定期状态更新");
        
        // 启动定期状态更新（每5秒更新一次）
        DeviceController::instance().startPeriodicUpdate(5000);
        
        LOG_INFO("设备控制器初始化成功，将使用硬件电池数据");
    } else {
        LOG_WARNING("设备控制器初始化失败，电池和温度状态将显示为未检测到");
        // 确保电池图标显示为未检测到状态
        if (m_batteryIcon) {
            m_batteryIcon->setNotDetected();
        }
        // 确保温度显示为未检测到状态
        if (m_temperatureTextLabel) {
            m_temperatureTextLabel->setText("未检测到");
            m_temperatureTextLabel->setStyleSheet(
                "background: transparent; "
                "color: white; "  // 与电量区域一致，使用白色
                "font-size: 24px; "  // 与电量区域一致的字体大小
                "font-weight: bold; "
                "font-family: 'WenQuanYi Zen Hei';"
            );
        }
    }
}

} // namespace SmartScope 

// 添加 StatusBar 的 paintEvent 方法实现
void SmartScope::StatusBar::paintEvent(QPaintEvent *event)
{
    // 仅绘制必要的内容，不绘制整体背景
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 不绘制半透明背景，让状态栏整体透明
    // 只保留各个区域的独立底纹
    
    // 调用基类的 paintEvent 以绘制子控件
    QWidget::paintEvent(event);
}