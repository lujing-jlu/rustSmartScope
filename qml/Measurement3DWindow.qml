import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.15
import "pages"
import "components"
import RustSmartScope.Logger 1.0

ApplicationWindow {
    id: measurement3DWindow
    visible: false
    title: "SmartScope Industrial - 3D测量"

    // 字体加载器
    FontLoader {
        id: interRegular
        source: "qrc:/fonts/Inter-Regular.ttf"
    }

    FontLoader {
        id: interMedium
        source: "qrc:/fonts/Inter-Medium.ttf"
    }

    // 中文字体加载器 - 思源黑体（只保留一个）
    FontLoader {
        id: sourceHanRegular
        source: "qrc:/fonts/SourceHanSansSC-Regular.otf"
    }

    // 简化的混合字体方案：英文使用Inter，中文使用思源黑体
    property string mixedFontRegular: sourceHanRegular.name + ", " + interRegular.name
    property string mixedFontMedium: sourceHanRegular.name + ", " + interMedium.name
    property string mixedFontBold: sourceHanRegular.name + ", " + interMedium.name

    // 目标分辨率 - 7寸1920x1200屏幕
    property int targetWidth: 1920
    property int targetHeight: 1200

    // 无边框窗口，占满屏幕尺寸
    width: screenWidth
    height: screenHeight
    flags: Qt.FramelessWindowHint | Qt.Window

    // 确保窗口在屏幕顶部左侧
    x: 0
    y: 0

    // DPI和缩放检测 - 使用Screen附加属性
    property real devicePixelRatio: Screen.devicePixelRatio || 1.0
    property real logicalDpi: Screen.logicalPixelDensity * 25.4 || 96

    // 获取实际屏幕尺寸 (设备无关像素)
    property real screenWidth: Screen.width || targetWidth
    property real screenHeight: Screen.height || targetHeight

    // 计算基于逻辑分辨率的缩放调整
    property real widthRatio: screenWidth / targetWidth
    property real heightRatio: screenHeight / targetHeight
    property real screenRatio: Math.min(widthRatio, heightRatio)

    // macOS Retina调整 - 考虑设备像素比
    property real hiDPIAdjustment: devicePixelRatio >= 2.0 ? 0.75 : 1.0

    // 最终尺寸调整系数
    property real sizeAdjustment: screenRatio * hiDPIAdjustment

    // 2024现代化设计系统 - 受启发于iOS 17 & Material You
    property color backgroundColor: "#0A0A0F"
    property color backgroundSecondary: "#121218"
    property color backgroundTertiary: "#1C1C24"
    property color surfaceColor: "#1E1E28"
    property color surfaceElevated: "#2A2A36"
    property color cardColor: "#242432"

    // 现代化红色主题色系统
    property color accentPrimary: "#0EA5E9"       // 专业蓝色 - 工业感
    property color accentSecondary: "#38BDF8"     // 亮蓝色 - 现代感
    property color accentTertiary: "#67E8F9"      // 青色 - 科技感
    property color accentSuccess: "#10B981"       // 保持绿色表示成功
    property color accentWarning: "#F59E0B"       // 保持橙色表示警告
    property color accentError: "#DC2626"         // 深红色表示错误

    // 文字色彩系统
    property color textPrimary: "#FFFFFF"
    property color textSecondary: "#E2E8F0"
    property color textTertiary: "#94A3B8"
    property color textMuted: "#64748B"

    // 边框和分割线
    property color borderPrimary: "#334155"
    property color borderSecondary: "#1E293B"
    property color separatorColor: "#475569"

    // HiDPI自适应百分比布局
    property real baseStatusBarRatio: 0.05          // 减小状态栏比例
    property real baseNavigationBarRatio: 0.16      // 基础导航栏比例 (增加1.6倍)
    property real baseButtonRatio: 0.04             // 基础按钮比例
    property real baseIconRatio: 0.025              // 基础图标比例
    property real baseFontRatio: 0.016              // 基础字体比例
    property real baseTitleRatio: 0.02              // 基础标题比例
    property real baseSpacingRatio: 0.0052          // 基础间距比例 (10px间距)
    property real baseMarginsRatio: 0.015           // 基础边距比例
    property real baseCornerRatio: 0.006            // 基础圆角比例 (减半)

    // 固定单排导航布局计算
    property int totalButtons: 6  // 主页、预览、报告、设置、3D测量、退出
    property real availableNavWidth: Math.min(width - margins * 4, 1400)
    property real buttonMaxWidth: 234  // 限制按钮最大宽度 (1.3倍)
    property real buttonMinWidth: 156  // 确保文字显示完整的最小宽度 (1.3倍)

    // 计算实际按钮宽度，优先保证文字完整显示
    property real dynamicButtonWidth: Math.max(
        buttonMinWidth,
        Math.min(
            buttonMaxWidth,
            (availableNavWidth - (totalButtons - 1) * spacing) / totalButtons
        )
    )

    // DPI调整后的响应式比例
    property real statusBarHeightRatio: baseStatusBarRatio * sizeAdjustment
    property real navigationBarHeightRatio: baseNavigationBarRatio * sizeAdjustment
    property real buttonHeightRatio: baseButtonRatio * sizeAdjustment
    property real iconSizeRatio: baseIconRatio * sizeAdjustment
    property real fontSizeRatio: baseFontRatio * sizeAdjustment
    property real titleSizeRatio: baseTitleRatio * sizeAdjustment
    property real spacingRatio: baseSpacingRatio * sizeAdjustment
    property real marginsRatio: baseMarginsRatio * sizeAdjustment
    property real cornerRadiusRatio: baseCornerRatio * sizeAdjustment

    // 现代化尺寸系统 - 适配HiDPI
    property int statusBarHeight: Math.max(50, height * statusBarHeightRatio)
    property int navigationBarHeight: Math.max(100, height * navigationBarHeightRatio)
    property int buttonHeight: Math.max(56, height * buttonHeightRatio)
    property int iconSize: Math.max(36, Math.min(width, height) * iconSizeRatio * 1.8)
    property int fontSize: Math.max(18, Math.min(width, height) * fontSizeRatio * 1.4)
    property int titleSize: Math.max(20, Math.min(width, height) * titleSizeRatio * 1.2)
    property int spacing: Math.max(16, Math.min(width, height) * spacingRatio * 2)
    property int margins: Math.max(12, Math.min(width, height) * marginsRatio * 1.5)
    property int cornerRadius: Math.max(16, Math.min(width, height) * cornerRadiusRatio * 2)
    property int shadowRadius: Math.max(8, Math.min(width, height) * cornerRadiusRatio)
    property int elevationLow: 4
    property int elevationMedium: 8
    property int elevationHigh: 16

    // 毛玻璃效果属性 - 根据性能调整
    property real glassOpacity: devicePixelRatio > 2.0 ? 0.7 : 0.8
    property real blurRadius: devicePixelRatio > 2.0 ? 30 : 40

    // 当前页面状态
    property string currentPage: "measurement"  // 默认显示3D测量页面

    // 背景
    Rectangle {
        anchors.fill: parent
        color: backgroundColor
    }

    // 主容器
    Rectangle {
        anchors.fill: parent
        color: "transparent"

        // 状态栏
        Rectangle {
            id: statusBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: statusBarHeight
            color: backgroundSecondary
            z: 100

            RowLayout {
                anchors.fill: parent
                anchors.margins: margins

                // 左侧 - 当前时间和日期
                RowLayout {
                    Layout.fillWidth: true
                    spacing: margins

                    Text {
                        id: timeText
                        text: new Date().toLocaleTimeString(Qt.locale(), "hh:mm:ss")
                        font.family: mixedFontMedium
                        font.pixelSize: fontSize
                        color: textPrimary
                        Layout.alignment: Qt.AlignVCenter

                        Timer {
                            interval: 1000
                            running: true
                            repeat: true
                            onTriggered: {
                                timeText.text = new Date().toLocaleTimeString(Qt.locale(), "hh:mm:ss")
                            }
                        }
                    }

                    Text {
                        text: new Date().toLocaleDateString(Qt.locale(), "yyyy-MM-dd")
                        font.family: mixedFontRegular
                        font.pixelSize: fontSize * 0.9
                        color: textSecondary
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                // 右侧 - 窗口标题
                Text {
                    text: "3D测量 - SmartScope Industrial"
                    font.family: mixedFontMedium
                    font.pixelSize: fontSize * 1.1
                    color: textPrimary
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        // 主内容区域
        Item {
            anchors.top: statusBar.bottom
            anchors.bottom: navigationContainer.top
            anchors.left: parent.left
            anchors.right: toolBarContainer.left
            anchors.margins: margins

            // 3D测量页面内容
            Rectangle {
                anchors.fill: parent
                color: surfaceColor
                radius: cornerRadius

                // 3D测量页面内容占位符
                Text {
                    anchors.centerIn: parent
                    text: "3D测量功能\n\n此处将显示3D测量相关功能界面"
                    font.family: mixedFontMedium
                    font.pixelSize: titleSize
                    color: textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.5
                }
            }
        }

        // 右侧3D测量专用工具栏
        Item {
            id: toolBarContainer
            anchors.right: parent.right
            anchors.top: statusBar.bottom
            anchors.bottom: navigationContainer.top
            anchors.margins: margins
            width: 100
            z: 90

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: margins
                spacing: margins
                anchors.topMargin: margins * 2

                // 3D校准按钮
                UniversalButton {
                    id: calibrateButton
                    text: "3D校准"
                    buttonStyle: "toolbar"
                    iconSource: "qrc:/icons/config.svg"
                    onClicked: {
                        Logger.info("3D calibration clicked")
                    }
                }

                // 点云生成按钮
                UniversalButton {
                    id: pointCloudButton
                    text: "点云生成"
                    buttonStyle: "toolbar"
                    iconSource: "qrc:/icons/view.svg"
                    onClicked: {
                        Logger.info("Point cloud generation clicked")
                    }
                }

                // 深度测量按钮
                UniversalButton {
                    id: depthMeasureButton
                    text: "深度测量"
                    buttonStyle: "toolbar"
                    iconSource: "qrc:/icons/measurement/depth.svg"
                    onClicked: {
                        Logger.info("Depth measurement clicked")
                        depthMeasureButton.isActive = !depthMeasureButton.isActive
                    }
                }

                // 长度测量按钮
                UniversalButton {
                    id: lengthMeasureButton
                    text: "长度测量"
                    buttonStyle: "toolbar"
                    iconSource: "qrc:/icons/measurement/length.svg"
                    onClicked: {
                        Logger.info("Length measurement clicked")
                        lengthMeasureButton.isActive = !lengthMeasureButton.isActive
                    }
                }

                // 面积测量按钮
                UniversalButton {
                    id: areaMeasureButton
                    text: "面积测量"
                    buttonStyle: "toolbar"
                    iconSource: "qrc:/icons/measurement/area.svg"
                    onClicked: {
                        Logger.info("Area measurement clicked")
                        areaMeasureButton.isActive = !areaMeasureButton.isActive
                    }
                }

                // 测量数据导出按钮
                UniversalButton {
                    id: exportButton
                    text: "数据导出"
                    buttonStyle: "toolbar"
                    iconSource: "qrc:/icons/save.svg"
                    onClicked: {
                        Logger.info("Export measurement data clicked")
                    }
                }

                // 弹性空间
                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // 底部导航栏容器
        Item {
            id: navigationContainer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: navigationBarHeight
            z: 95

            // 导航栏背景
            Rectangle {
                anchors.fill: parent
                color: backgroundSecondary
                border.color: borderPrimary
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: margins
                    spacing: spacing

                    // 关闭窗口按钮
                    Rectangle {
                        Layout.preferredWidth: dynamicButtonWidth
                        Layout.fillHeight: true
                        color: Qt.rgba(0.7, 0.2, 0.2, 0.8)
                        radius: cornerRadius
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.1)

                        property bool hovered: false

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                measurement3DWindow.close()
                            }
                            onEntered: parent.hovered = true
                            onExited: parent.hovered = false
                        }

                        Row {
                            anchors.centerIn: parent
                            spacing: spacing * 0.6

                            Item {
                                width: iconSize
                                height: iconSize
                                anchors.verticalCenter: parent.verticalCenter

                                Image {
                                    id: closeIcon
                                    source: "qrc:/icons/close.svg"
                                    anchors.fill: parent
                                    anchors.margins: parent.width * 0.15
                                    fillMode: Image.PreserveAspectFit
                                    visible: false
                                }

                                ColorOverlay {
                                    anchors.fill: closeIcon
                                    source: closeIcon
                                    color: "#FFFFFF"
                                }
                            }

                            Text {
                                text: "关闭"
                                color: "#FFFFFF"
                                font.pixelSize: fontSize * 1.5
                                font.weight: Font.Medium
                                font.family: mixedFontRegular
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    // 弹性空间
                    Item {
                        Layout.fillWidth: true
                    }

                    // 最小化按钮
                    Rectangle {
                        Layout.preferredWidth: dynamicButtonWidth
                        Layout.fillHeight: true
                        color: Qt.rgba(0.12, 0.12, 0.12, 0.6)
                        radius: cornerRadius
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.1)

                        property bool hovered: false

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                measurement3DWindow.showMinimized()
                            }
                            onEntered: parent.hovered = true
                            onExited: parent.hovered = false
                        }

                        Row {
                            anchors.centerIn: parent
                            spacing: spacing * 0.6

                            Item {
                                width: iconSize
                                height: iconSize
                                anchors.verticalCenter: parent.verticalCenter

                                Image {
                                    id: minimizeIcon
                                    source: "qrc:/icons/down_arrow.svg"
                                    anchors.fill: parent
                                    anchors.margins: parent.width * 0.15
                                    fillMode: Image.PreserveAspectFit
                                    visible: false
                                }

                                ColorOverlay {
                                    anchors.fill: minimizeIcon
                                    source: minimizeIcon
                                    color: "#FFFFFF"
                                }
                            }

                            Text {
                                text: "最小化"
                                color: "#FFFFFF"
                                font.pixelSize: fontSize * 1.5
                                font.weight: Font.Medium
                                font.family: mixedFontRegular
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }

    // 窗口关闭时的清理
    onClosing: {
        Logger.info("3D测量窗口关闭")
    }
}