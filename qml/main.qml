import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.15

ApplicationWindow {
    id: mainWindow
    visible: true
    title: "SmartScope Industrial"

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

    // Apple风格毛玻璃深色主题
    property color backgroundColor: "#0F0F0F"
    property color glassBackground: "#1A1A1A"
    property color surfaceColor: "#1E1E1E"
    property color cardColor: "#2D2D2D"
    property color accentColor: "#007AFF"
    property color accentSecondary: "#34C759"
    property color warningColor: "#FF9F0A"
    property color errorColor: "#FF453A"
    property color textPrimary: "#FFFFFF"
    property color textSecondary: "#EBEBF5"
    property color textTertiary: "#EBEBF599"
    property color separatorColor: "#545458"

    // HiDPI自适应百分比布局
    property real baseStatusBarRatio: 0.05          // 基础状态栏比例
    property real baseNavigationBarRatio: 0.08      // 基础导航栏比例
    property real baseButtonRatio: 0.04             // 基础按钮比例
    property real baseIconRatio: 0.025              // 基础图标比例
    property real baseFontRatio: 0.016              // 基础字体比例
    property real baseTitleRatio: 0.02              // 基础标题比例
    property real baseSpacingRatio: 0.01            // 基础间距比例
    property real baseMarginsRatio: 0.015           // 基础边距比例
    property real baseCornerRatio: 0.012            // 基础圆角比例

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

    // 计算实际尺寸 - 适配HiDPI
    property int statusBarHeight: Math.max(40, height * statusBarHeightRatio)
    property int navigationBarHeight: Math.max(60, height * navigationBarHeightRatio)
    property int buttonHeight: Math.max(32, height * buttonHeightRatio)
    property int iconSize: Math.max(16, Math.min(width, height) * iconSizeRatio)
    property int fontSize: Math.max(12, Math.min(width, height) * fontSizeRatio)
    property int titleSize: Math.max(16, Math.min(width, height) * titleSizeRatio)
    property int spacing: Math.max(6, Math.min(width, height) * spacingRatio)
    property int margins: Math.max(8, Math.min(width, height) * marginsRatio)
    property int cornerRadius: Math.max(4, Math.min(width, height) * cornerRadiusRatio)

    // 毛玻璃效果属性 - 根据性能调整
    property real glassOpacity: devicePixelRatio > 2.0 ? 0.7 : 0.8
    property real blurRadius: devicePixelRatio > 2.0 ? 30 : 40
    property real glassBlur: devicePixelRatio > 2.0 ? 45 : 60

    // 动态背景渐变
    Rectangle {
        id: backgroundGradient
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: backgroundColor }
            GradientStop { position: 0.3; color: "#151515" }
            GradientStop { position: 0.7; color: "#1A1A1A" }
            GradientStop { position: 1.0; color: backgroundColor }
        }
    }

    Rectangle {
        id: rootContainer
        anchors.fill: parent
        color: "transparent"

        // 毛玻璃状态栏
        Rectangle {
            id: statusBarBackground
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: statusBarHeight
            color: glassBackground
            opacity: glassOpacity
            radius: 0
        }

        FastBlur {
            anchors.fill: statusBarBackground
            source: backgroundGradient
            radius: blurRadius
            cached: true

            Rectangle {
                anchors.fill: parent
                color: surfaceColor
                opacity: 0.3
            }
        }

        // 状态栏内容
        Rectangle {
            id: statusBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: statusBarHeight
            color: "transparent"

            // Apple风格分割线
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 0.5
                color: separatorColor
                opacity: 0.6
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: margins
                anchors.rightMargin: margins
                spacing: spacing

                // 系统标题 - Apple风格
                Text {
                    text: "SmartScope"
                    color: textPrimary
                    font.pixelSize: titleSize
                    font.weight: Font.DemiBold
                    font.family: "-apple-system, SF Pro Display"
                }

                Item { Layout.fillWidth: true }

                // 状态指示器组 - Apple风格
                Row {
                    spacing: spacing * 2

                    // 连接状态指示器
                    Rectangle {
                        width: iconSize * 0.4
                        height: iconSize * 0.4
                        radius: width / 2
                        color: accentSecondary

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 0.6
                            height: parent.height * 0.6
                            radius: width / 2
                            color: textPrimary
                            opacity: 0.9
                        }

                        // Apple风格呼吸动画
                        PropertyAnimation on scale {
                            id: breathingAnimation
                            running: true
                            loops: Animation.Infinite
                            from: 1.0
                            to: 1.2
                            duration: 4000
                            easing.type: Easing.InOutSine

                            onRunningChanged: {
                                if (!running && loops == Animation.Infinite) {
                                    running = true
                                }
                            }
                        }
                    }

                    // 时间显示 - Apple风格
                    Text {
                        text: Qt.formatTime(new Date(), "HH:mm")
                        color: textSecondary
                        font.pixelSize: fontSize
                        font.weight: Font.Medium
                        font.family: "-apple-system, SF Pro Display"
                    }

                    // 电池状态（可选）
                    Rectangle {
                        width: fontSize * 1.8
                        height: fontSize * 0.9
                        radius: 2
                        color: "transparent"
                        border.color: textSecondary
                        border.width: 1

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 1
                            width: parent.width * 0.85 // 85% 电量
                            height: parent.height - 2
                            radius: 1
                            color: accentSecondary
                        }

                        Rectangle {
                            anchors.left: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 2
                            height: parent.height * 0.6
                            radius: 1
                            color: textSecondary
                        }
                    }
                }
            }
        }

        // 主内容区域
        Rectangle {
            id: contentArea
            anchors.top: statusBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: navigationBar.top
            anchors.margins: margins
            color: "transparent"

            // 毛玻璃主卡片
            Rectangle {
                id: mainCardBackground
                anchors.fill: parent
                color: glassBackground
                opacity: 0.7
                radius: cornerRadius
            }

            FastBlur {
                anchors.fill: mainCardBackground
                source: backgroundGradient
                radius: glassBlur
                cached: true

                Rectangle {
                    anchors.fill: parent
                    color: cardColor
                    opacity: 0.2
                    radius: cornerRadius
                }
            }

            // 主内容卡片
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                radius: cornerRadius
                border.color: separatorColor
                border.width: 0.5

                // 欢迎内容 - Apple风格
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: mainWindow.spacing * 3

                    // 主图标 - Apple风格渐变
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: iconSize * 4
                        height: iconSize * 4
                        radius: width / 2
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: accentColor }
                            GradientStop { position: 1.0; color: "#0066CC" }
                        }
                        opacity: 0.15

                        Text {
                            anchors.centerIn: parent
                            text: "🔬"
                            color: accentColor
                            font.pixelSize: iconSize * 2
                        }

                        // Apple风格光环效果
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 1.3
                            height: parent.height * 1.3
                            radius: width / 2
                            color: "transparent"
                            border.color: accentColor
                            border.width: 1
                            opacity: 0.3

                            SequentialAnimation on opacity {
                                id: haloAnimation
                                running: true
                                loops: Animation.Infinite

                                PropertyAnimation {
                                    from: 0.3
                                    to: 0.0
                                    duration: 3000
                                    easing.type: Easing.OutQuart
                                }
                                PropertyAnimation {
                                    from: 0.0
                                    to: 0.3
                                    duration: 1000
                                    easing.type: Easing.InQuart
                                }
                            }
                        }
                    }

                    // 主标题 - Apple风格
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "SmartScope Industrial"
                        color: textPrimary
                        font.pixelSize: titleSize * 1.5
                        font.weight: Font.Bold
                        font.family: "-apple-system, SF Pro Display"
                    }

                    // 子标题
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "专业级深度检测与测量解决方案"
                        color: textSecondary
                        font.pixelSize: fontSize * 1.1
                        font.weight: Font.Medium
                        font.family: "-apple-system, SF Pro Text"
                    }

                    // 状态胶囊 - Apple风格
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: spacing * 2
                        width: fontSize * 12
                        height: buttonHeight * 0.7
                        radius: height / 2
                        color: accentSecondary
                        opacity: 0.2

                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            radius: parent.radius
                            border.color: accentSecondary
                            border.width: 1
                        }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: spacing

                            Rectangle {
                                width: fontSize * 0.7
                                height: fontSize * 0.7
                                radius: width / 2
                                color: accentSecondary
                            }

                            Text {
                                text: "系统就绪"
                                color: accentSecondary
                                font.pixelSize: fontSize
                                font.weight: Font.Medium
                                font.family: "-apple-system, SF Pro Text"
                            }
                        }
                    }
                }
            }
        }

        // 毛玻璃导航栏背景
        Rectangle {
            id: navBarBackground
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: navigationBarHeight
            color: glassBackground
            opacity: glassOpacity
        }

        FastBlur {
            anchors.fill: navBarBackground
            source: backgroundGradient
            radius: blurRadius
            cached: true

            Rectangle {
                anchors.fill: parent
                color: surfaceColor
                opacity: 0.4
            }
        }

        // 导航栏内容
        Rectangle {
            id: navigationBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: navigationBarHeight
            color: "transparent"

            // Apple风格顶部分割线
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 0.5
                color: separatorColor
                opacity: 0.8
            }

            // 导航按钮布局
            RowLayout {
                anchors.fill: parent
                anchors.margins: margins
                anchors.topMargin: margins * 0.5
                anchors.bottomMargin: margins * 1.5
                spacing: 0

                // 主页按钮 - Apple风格
                Rectangle {
                    id: homeButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"
                    radius: cornerRadius

                    property bool isActive: true

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            console.log("Home clicked")
                            setActiveTab(homeButton)
                        }
                        onPressed: homeButton.scale = 0.95
                        onReleased: homeButton.scale = 1.0
                    }

                    Behavior on scale {
                        PropertyAnimation {
                            duration: 150
                            easing.type: Easing.OutQuart
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: spacing * 0.5

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: iconSize * 1.4
                            height: iconSize * 1.4
                            radius: cornerRadius
                            color: homeButton.isActive ? accentColor : "transparent"
                            opacity: homeButton.isActive ? 0.2 : 0

                            Behavior on opacity {
                                PropertyAnimation { duration: 200 }
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "🏠"
                                color: homeButton.isActive ? accentColor : textTertiary
                                font.pixelSize: iconSize * 0.9
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "主页"
                            color: homeButton.isActive ? accentColor : textTertiary
                            font.pixelSize: fontSize * 0.8
                            font.weight: homeButton.isActive ? Font.Medium : Font.Normal
                            font.family: "-apple-system, SF Pro Text"
                        }
                    }
                }

                // 检测按钮
                Rectangle {
                    id: detectionButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"
                    radius: cornerRadius

                    property bool isActive: false

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            console.log("Detection clicked")
                            setActiveTab(detectionButton)
                        }
                        onPressed: detectionButton.scale = 0.95
                        onReleased: detectionButton.scale = 1.0
                    }

                    Behavior on scale {
                        PropertyAnimation {
                            duration: 150
                            easing.type: Easing.OutQuart
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: spacing * 0.5

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: iconSize * 1.4
                            height: iconSize * 1.4
                            radius: cornerRadius
                            color: detectionButton.isActive ? accentColor : "transparent"
                            opacity: detectionButton.isActive ? 0.2 : 0

                            Text {
                                anchors.centerIn: parent
                                text: "🔍"
                                color: detectionButton.isActive ? accentColor : textTertiary
                                font.pixelSize: iconSize * 0.9
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "检测"
                            color: detectionButton.isActive ? accentColor : textTertiary
                            font.pixelSize: fontSize * 0.8
                            font.weight: detectionButton.isActive ? Font.Medium : Font.Normal
                            font.family: "-apple-system, SF Pro Text"
                        }
                    }
                }

                // 测量按钮
                Rectangle {
                    id: measurementButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"
                    radius: cornerRadius

                    property bool isActive: false

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            console.log("Measurement clicked")
                            setActiveTab(measurementButton)
                        }
                        onPressed: measurementButton.scale = 0.95
                        onReleased: measurementButton.scale = 1.0
                    }

                    Behavior on scale {
                        PropertyAnimation {
                            duration: 150
                            easing.type: Easing.OutQuart
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: spacing * 0.5

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: iconSize * 1.4
                            height: iconSize * 1.4
                            radius: cornerRadius
                            color: measurementButton.isActive ? accentColor : "transparent"
                            opacity: measurementButton.isActive ? 0.2 : 0

                            Text {
                                anchors.centerIn: parent
                                text: "📏"
                                color: measurementButton.isActive ? accentColor : textTertiary
                                font.pixelSize: iconSize * 0.9
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "测量"
                            color: measurementButton.isActive ? accentColor : textTertiary
                            font.pixelSize: fontSize * 0.8
                            font.weight: measurementButton.isActive ? Font.Medium : Font.Normal
                            font.family: "-apple-system, SF Pro Text"
                        }
                    }
                }

                // 设置按钮
                Rectangle {
                    id: settingsButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"
                    radius: cornerRadius

                    property bool isActive: false

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            console.log("Settings clicked")
                            setActiveTab(settingsButton)
                        }
                        onPressed: settingsButton.scale = 0.95
                        onReleased: settingsButton.scale = 1.0
                    }

                    Behavior on scale {
                        PropertyAnimation {
                            duration: 150
                            easing.type: Easing.OutQuart
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: spacing * 0.5

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: iconSize * 1.4
                            height: iconSize * 1.4
                            radius: cornerRadius
                            color: settingsButton.isActive ? accentColor : "transparent"
                            opacity: settingsButton.isActive ? 0.2 : 0

                            Text {
                                anchors.centerIn: parent
                                text: "⚙️"
                                color: settingsButton.isActive ? accentColor : textTertiary
                                font.pixelSize: iconSize * 0.9
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "设置"
                            color: settingsButton.isActive ? accentColor : textTertiary
                            font.pixelSize: fontSize * 0.8
                            font.weight: settingsButton.isActive ? Font.Medium : Font.Normal
                            font.family: "-apple-system, SF Pro Text"
                        }
                    }
                }
            }
        }
    }

    // 标签页切换函数
    function setActiveTab(activeButton) {
        homeButton.isActive = (activeButton === homeButton)
        detectionButton.isActive = (activeButton === detectionButton)
        measurementButton.isActive = (activeButton === measurementButton)
        settingsButton.isActive = (activeButton === settingsButton)
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
        console.log("Content area: " + contentArea.width + "x" + contentArea.height)
    }
}