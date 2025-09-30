import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.15
import "pages"
import "components"

ApplicationWindow {
    id: mainWindow
    visible: true
    title: "SmartScope Industrial"

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
    property real glassBlur: devicePixelRatio > 2.0 ? 45 : 60

    // 基础时间和电池状态
    property string currentTime: Qt.formatTime(new Date(), "HH:mm")
    property string currentDate: Qt.formatDate(new Date(), "MM/dd")
    property int batteryLevel: 85  // 85% 电量
    property bool isCharging: false  // 充电状态

    // 实时更新时间的Timer
    Timer {
        id: timeUpdateTimer
        interval: 1000  // 每秒更新
        running: true
        repeat: true

        onTriggered: {
            var now = new Date()
            currentTime = Qt.formatTime(now, "HH:mm")
            currentDate = Qt.formatDate(now, "MM/dd")
        }
    }

    // 2024现代化动态背景
    Rectangle {
        id: backgroundGradient
        anchors.fill: parent

        // 主渐变背景
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: backgroundColor }
            GradientStop { position: 0.25; color: backgroundSecondary }
            GradientStop { position: 0.75; color: backgroundTertiary }
            GradientStop { position: 1.0; color: surfaceColor }
        }

        // 现代化装饰光理
        Rectangle {
            id: accentGlow1
            width: 600
            height: 600
            radius: width / 2
            x: parent.width * 0.05
            y: parent.height * 0.1
            opacity: 0.08
            color: accentPrimary

            PropertyAnimation on scale {
                running: true
                loops: Animation.Infinite
                from: 0.7; to: 1.1
                duration: 8000
                easing.type: Easing.InOutSine
            }

            PropertyAnimation on opacity {
                running: true
                loops: Animation.Infinite
                from: 0.05; to: 0.12
                duration: 5000
                easing.type: Easing.InOutSine
            }
        }

        Rectangle {
            id: accentGlow2
            width: 400
            height: 400
            radius: width / 2
            x: parent.width * 0.75
            y: parent.height * 0.65
            opacity: 0.06
            color: accentSecondary

            PropertyAnimation on scale {
                running: true
                loops: Animation.Infinite
                from: 1.0; to: 0.6
                duration: 6000
                easing.type: Easing.InOutSine
            }

            PropertyAnimation on opacity {
                running: true
                loops: Animation.Infinite
                from: 0.03; to: 0.09
                duration: 7000
                easing.type: Easing.InOutSine
            }
        }
    }

    Rectangle {
        id: rootContainer
        anchors.fill: parent
        color: "transparent"

        // 现代化毛玻璃状态栏
        Rectangle {
            id: statusBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: statusBarHeight
            color: Qt.rgba(0.1, 0.1, 0.15, 0.8)
            radius: 0
            z: 100

            // 高级毛玻璃效果
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.1)
                radius: parent.radius
            }

            // 底部阴影
            Rectangle {
                anchors.top: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.2) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 0

                // 左侧：Logo区域
                Rectangle {
                    Layout.preferredWidth: fontSize * 8
                    Layout.fillHeight: true
                    color: "transparent"

                    Image {
                        id: appIcon
                        source: "qrc:/icons/EDDYSUN-logo.png"
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        height: parent.height * 0.7
                        fillMode: Image.PreserveAspectFit
                        visible: true

                        Component.onCompleted: {
                            console.log("main.qml 图标组件加载完成, 源: " + source)
                            console.log("Logo容器尺寸: " + parent.width + "x" + parent.height)
                            console.log("Logo图片尺寸: " + width + "x" + height)
                            console.log("Logo原始尺寸: " + implicitWidth + "x" + implicitHeight)
                        }

                        onStatusChanged: {
                            console.log("main.qml 图标状态: " + status + ", 源: " + source)
                            if (status === Image.Error) {
                                console.log("main.qml 图标加载失败: " + source)
                            } else if (status === Image.Ready) {
                                console.log("main.qml 图标加载成功: " + source)
                            }
                        }
                    }
                }

                // 中间：时间区域
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"

                    Text {
                        text: currentTime
                        color: "#FFFFFF"
                        font.pixelSize: fontSize * 1.6
                        font.weight: Font.Medium
                        font.family: mixedFontMedium
                        anchors.centerIn: parent
                    }
                }

                // 右侧：电池区域
                Rectangle {
                    Layout.preferredWidth: fontSize * 5
                    Layout.fillHeight: true
                    color: "transparent"

                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: fontSize * 0.3

                        // 电池图标
                        Rectangle {
                            width: fontSize * 1.5
                            height: fontSize * 0.8
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 2
                            color: "transparent"
                            border.color: "#FFFFFF"
                            border.width: 1.5

                            // 电池电量填充
                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 2
                                width: (parent.width - 4) * (batteryLevel / 100.0)
                                height: parent.height - 4
                                radius: 1
                                color: {
                                    if (batteryLevel > 50) return accentSuccess
                                    else if (batteryLevel > 20) return accentWarning
                                    else return accentError
                                }
                            }

                            // 电池正极
                            Rectangle {
                                anchors.left: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: 2
                                height: parent.height * 0.5
                                radius: 1
                                color: "#FFFFFF"
                            }
                        }

                        // 电量百分比
                        Text {
                            text: batteryLevel + "%"
                            color: "#FFFFFF"
                            font.pixelSize: fontSize * 1.6
                            font.weight: Font.Medium
                            font.family: mixedFontMedium
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }

        // 全屏主内容区域 - 动态页面容器
        Loader {
            id: pageLoader
            anchors.fill: parent

            // 根据当前页面加载不同的组件
            source: {
                switch(currentPage) {
                    case "home": return "pages/HomePage.qml"
                    case "preview": return "pages/PreviewPage.qml"
                    case "measurement": return "pages/MeasurementPage.qml"
                    case "report": return "pages/ReportPage.qml"
                    case "settings": return "pages/SettingsPage.qml"
                    default: return "pages/HomePage.qml"
                }
            }

            // 页面切换动画
            Behavior on opacity {
                PropertyAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }

            onLoaded: {
                console.log("Page loaded:", currentPage)
            }

            onSourceChanged: {
                console.log("Loading page:", source)
                opacity = 0
                opacity = 1
            }
        }

        // 2024现代化悬浮导航栏
        Item {
            id: navigationContainer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: navigationBarHeight + margins * 2
            z: 100

            // 透明导航容器
            Item {
                id: navigationCard
                anchors.bottom: parent.bottom
                anchors.bottomMargin: margins * 0.3
                anchors.horizontalCenter: parent.horizontalCenter
                width: availableNavWidth
                height: navigationBarHeight

                // 固定单排导航按钮布局
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: margins
                    spacing: margins

                    // 主页按钮
                    NavigationButton {
                        id: homeButton
                        text: "主页"
                        iconSource: "qrc:/icons/home.svg"
                        isActive: true
                        iconOnly: true
                        onClicked: {
                            console.log("Home clicked")
                            setActiveTab(homeButton)
                        }
                    }

                    // 预览按钮
                    NavigationButton {
                        id: detectionButton
                        text: "预览"
                        iconSource: "qrc:/icons/preview.svg"
                        activeColor: "#38BDF8"
                        onClicked: {
                            console.log("Detection clicked")
                            setActiveTab(detectionButton)
                        }
                    }


                    // 报告按钮
                    Rectangle {
                        id: reportButton
                        Layout.preferredWidth: dynamicButtonWidth
                        Layout.fillHeight: true
                        color: reportButton.isActive ?
                            Qt.rgba(0.2, 0.2, 0.2, 0.8) : Qt.rgba(0.12, 0.12, 0.12, 0.6)
                        radius: cornerRadius
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.1)

                        property bool isActive: false
                        property bool hovered: false

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                console.log("Report clicked")
                                setActiveTab(reportButton)
                            }
                            onPressed: reportButton.scale = 0.92
                            onReleased: reportButton.scale = 1.0
                            onEntered: reportButton.hovered = true
                            onExited: reportButton.hovered = false
                        }

                        Behavior on scale { SpringAnimation { spring: 4; damping: 0.6 } }
                        Behavior on color { ColorAnimation { duration: 300 } }

                        Rectangle {
                            anchors.fill: parent
                            color: Qt.rgba(0.31, 0.31, 0.31, 0.7)
                            radius: parent.radius
                            opacity: reportButton.hovered ? 1 : 0
                            Behavior on opacity { PropertyAnimation { duration: 200 } }
                        }

                        Row {
                            anchors.centerIn: parent
                            spacing: spacing * 0.6

                            Item {
                                width: iconSize
                                height: iconSize
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle {
                                    anchors.fill: parent
                                    color: reportButton.isActive ? accentPrimary : "transparent"
                                    radius: width / 2
                                    opacity: reportButton.isActive ? 0.15 : 0
                                    Behavior on opacity {
                                        PropertyAnimation { duration: 300 }
                                    }
                                }

                                Image {
                                    id: reportIcon
                                    source: "qrc:/icons/report.svg"
                                    anchors.fill: parent
                                    anchors.margins: parent.width * 0.15
                                    fillMode: Image.PreserveAspectFit
                                    visible: false
                                }

                                ColorOverlay {
                                    anchors.fill: reportIcon
                                    source: reportIcon
                                    color: "#FFFFFF"
                                    Behavior on color { ColorAnimation { duration: 300 } }
                                }
                            }

                            Text {
                                text: "报告"
                                color: "#FFFFFF"
                                font.pixelSize: 38
                                font.weight: Font.Medium
                                font.family: mixedFontRegular
                                anchors.verticalCenter: parent.verticalCenter
                                Behavior on color { ColorAnimation { duration: 300 } }
                            }
                        }
                    }

                    // 设置按钮
                    Rectangle {
                        id: settingsButton
                        Layout.preferredWidth: dynamicButtonWidth
                        Layout.fillHeight: true
                        color: settingsButton.isActive ?
                            Qt.rgba(0.2, 0.2, 0.2, 0.8) : Qt.rgba(0.12, 0.12, 0.12, 0.6)
                        radius: cornerRadius
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.1)

                        property bool isActive: false
                        property bool hovered: false

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                console.log("Settings clicked")
                                setActiveTab(settingsButton)
                            }
                            onPressed: settingsButton.scale = 0.92
                            onReleased: settingsButton.scale = 1.0
                            onEntered: settingsButton.hovered = true
                            onExited: settingsButton.hovered = false
                        }

                        Behavior on scale { SpringAnimation { spring: 4; damping: 0.6 } }
                        Behavior on color { ColorAnimation { duration: 300 } }

                        Rectangle {
                            anchors.fill: parent
                            color: Qt.rgba(0.31, 0.31, 0.31, 0.7)
                            radius: parent.radius
                            opacity: settingsButton.hovered ? 1 : 0
                            Behavior on opacity { PropertyAnimation { duration: 200 } }
                        }

                        Row {
                            anchors.centerIn: parent
                            spacing: spacing * 0.6

                            Item {
                                width: iconSize
                                height: iconSize
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle {
                                    anchors.fill: parent
                                    color: settingsButton.isActive ? accentPrimary : "transparent"
                                    radius: width / 2
                                    opacity: settingsButton.isActive ? 0.15 : 0
                                    Behavior on opacity {
                                        PropertyAnimation { duration: 300 }
                                    }
                                }

                                Image {
                                    id: settingIcon
                                    source: "qrc:/icons/setting.svg"
                                    anchors.fill: parent
                                    anchors.margins: parent.width * 0.15
                                    fillMode: Image.PreserveAspectFit
                                    visible: false
                                }

                                ColorOverlay {
                                    anchors.fill: settingIcon
                                    source: settingIcon
                                    color: "#FFFFFF"
                                    Behavior on color { ColorAnimation { duration: 300 } }
                                }
                            }

                            Text {
                                text: "设置"
                                color: "#FFFFFF"
                                font.pixelSize: 38
                                font.weight: Font.Medium
                                font.family: mixedFontRegular
                                anchors.verticalCenter: parent.verticalCenter
                                Behavior on color { ColorAnimation { duration: 300 } }
                            }
                        }
                    }

                    // 3D测量按钮
                    Rectangle {
                        id: measurementButton
                        Layout.preferredWidth: dynamicButtonWidth
                        Layout.fillHeight: true
                        color: measurementButton.isActive ?
                            Qt.rgba(0.2, 0.2, 0.2, 0.8) : Qt.rgba(0.12, 0.12, 0.12, 0.6)
                        radius: cornerRadius
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.1)

                        property bool isActive: false
                        property bool hovered: false

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                console.log("Measurement clicked")
                                setActiveTab(measurementButton)
                            }
                            onPressed: measurementButton.scale = 0.92
                            onReleased: measurementButton.scale = 1.0
                            onEntered: measurementButton.hovered = true
                            onExited: measurementButton.hovered = false
                        }

                        Behavior on scale { SpringAnimation { spring: 4; damping: 0.6 } }
                        Behavior on color { ColorAnimation { duration: 300 } }

                        Rectangle {
                            anchors.fill: parent
                            color: Qt.rgba(0.31, 0.31, 0.31, 0.7)
                            radius: parent.radius
                            opacity: measurementButton.hovered ? 1 : 0
                            Behavior on opacity { PropertyAnimation { duration: 200 } }
                        }

                        Row {
                            anchors.centerIn: parent
                            spacing: spacing * 0.6

                            Item {
                                width: iconSize
                                height: iconSize
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle {
                                    anchors.fill: parent
                                    color: measurementButton.isActive ? accentPrimary : "transparent"
                                    radius: width / 2
                                    opacity: measurementButton.isActive ? 0.15 : 0
                                    Behavior on opacity {
                                        PropertyAnimation { duration: 300 }
                                    }
                                }

                                Image {
                                    id: measurementIcon
                                    source: "qrc:/icons/3D.svg"
                                    anchors.fill: parent
                                    anchors.margins: parent.width * 0.15
                                    fillMode: Image.PreserveAspectFit
                                    visible: false
                                }

                                ColorOverlay {
                                    anchors.fill: measurementIcon
                                    source: measurementIcon
                                    color: "#FFFFFF"
                                    Behavior on color { ColorAnimation { duration: 300 } }
                                }
                            }

                            Text {
                                text: "3D测量"
                                color: "#FFFFFF"
                                font.pixelSize: 38
                                font.weight: Font.Medium
                                font.family: mixedFontRegular
                                anchors.verticalCenter: parent.verticalCenter
                                Behavior on color { ColorAnimation { duration: 300 } }
                            }
                        }
                    }

                    // 退出按钮
                    NavigationButton {
                        id: exitButton
                        text: "退出"
                        iconSource: "qrc:/icons/close.svg"
                        isExitButton: true
                        iconOnly: true
                        onClicked: {
                            console.log("Exit clicked")
                            exitDialog.visible = true
                        }
                    }
                }
            }
        }
    }

    // 退出确认对话框
    Rectangle {
        id: exitDialog
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.7)
        visible: false
        z: 200

        MouseArea {
            anchors.fill: parent
            onClicked: exitDialog.visible = false
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.8, 600)
            height: 320
            radius: cornerRadius * 2
            color: surfaceElevated
            border.width: 1
            border.color: borderPrimary

            Column {
                anchors.centerIn: parent
                spacing: spacing * 2

                Text {
                    text: "确认退出"
                    color: textPrimary
                    font.pixelSize: fontSize * 2.2
                    font.weight: Font.Medium
                    font.family: mixedFontMedium
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "确定要退出SmartScope应用吗？"
                    color: textSecondary
                    font.pixelSize: fontSize * 1.6
                    font.family: mixedFontRegular
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: spacing * 2

                    Rectangle {
                        width: 140
                        height: buttonHeight * 0.8
                        radius: cornerRadius
                        color: accentError
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.2)

                        Text {
                            text: "退出"
                            color: "#FFFFFF"
                            font.pixelSize: fontSize * 1.6
                            font.weight: Font.Medium
                            font.family: mixedFontMedium
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                console.log("Application exit confirmed")
                                Qt.quit()
                            }
                        }
                    }

                    Rectangle {
                        width: 140
                        height: buttonHeight * 0.8
                        radius: cornerRadius
                        color: "transparent"
                        border.width: 2
                        border.color: borderPrimary

                        Text {
                            text: "取消"
                            color: textPrimary
                            font.pixelSize: fontSize * 1.6
                            font.weight: Font.Medium
                            font.family: mixedFontMedium
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: exitDialog.visible = false
                        }
                    }
                }
            }
        }
    }

    // 当前页面状态
    property string currentPage: "home"

    // 页面路由函数
    function navigateTo(pageName) {
        console.log("Navigating to:", pageName)
        currentPage = pageName

        // 更新按钮状态
        homeButton.isActive = (pageName === "home")
        detectionButton.isActive = (pageName === "preview")
        measurementButton.isActive = (pageName === "measurement")
        reportButton.isActive = (pageName === "report")
        settingsButton.isActive = (pageName === "settings")

        // 切换页面内容（稍后实现）
        // TODO: 实现实际的页面切换逻辑
    }

    // 标签页切换函数
    function setActiveTab(activeButton) {
        if (activeButton === homeButton) navigateTo("home")
        else if (activeButton === detectionButton) navigateTo("preview")
        else if (activeButton === measurementButton) navigateTo("measurement")
        else if (activeButton === reportButton) navigateTo("report")
        else if (activeButton === settingsButton) navigateTo("settings")
    }
    Component.onCompleted: {
        console.log("=== SmartScope Industrial UI initialized ===")
        console.log("Screen Resolution (logical): " + screenWidth + "x" + screenHeight)
        console.log("Window Size: " + width + "x" + height)
        console.log("Target Resolution: " + targetWidth + "x" + targetHeight)
        console.log("Device Pixel Ratio: " + devicePixelRatio.toFixed(2))
        console.log("Logical DPI: " + logicalDpi.toFixed(1))
        console.log("Visibility: " + visibility)
        console.log("---")
        console.log("Width Ratio: " + widthRatio.toFixed(2))
        console.log("Height Ratio: " + heightRatio.toFixed(2))
        console.log("Screen Ratio: " + screenRatio.toFixed(2))
        console.log("HiDPI Adjustment: " + hiDPIAdjustment.toFixed(2))
        console.log("Final Size Adjustment: " + sizeAdjustment.toFixed(2))
        console.log("---")
        console.log("Status bar height: " + statusBarHeight + "px")
        console.log("Navigation bar height: " + navigationBarHeight + "px")
        console.log("Font size: " + fontSize + "px")
        console.log("Icon size: " + iconSize + "px")
        console.log("Page loader area: " + pageLoader.width + "x" + pageLoader.height)
        console.log("---")
        console.log("Navigation Layout:")
        console.log("Available nav width: " + availableNavWidth + "px")
        console.log("Button max width: " + buttonMaxWidth + "px")
        console.log("Button min width: " + buttonMinWidth + "px")
        console.log("Dynamic button width: " + dynamicButtonWidth + "px")
    }
}