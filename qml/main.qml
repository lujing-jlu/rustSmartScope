import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.15
import RustSmartScope.Logger 1.0
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
            (availableNavWidth - (totalButtons - 1) * 10) / totalButtons
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
    property int statusBarHeight: Math.max(75, height * statusBarHeightRatio * 1.5)  // 增加50%
    property int navigationBarHeight: Math.max(150, height * navigationBarHeightRatio * 1.5)  // 增加50%
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
    property string currentTime: ""
    property string currentDate: ""
    property int batteryLevel: 85  // 85% 电量
    property bool isCharging: false  // 充电状态

    // 格式化时间的函数
    function formatCurrentTime() {
        var now = new Date()
        var hours = now.getHours()
        var minutes = now.getMinutes()
        // 手动添加前导零
        var hourStr = hours < 10 ? "0" + hours : hours.toString()
        var minuteStr = minutes < 10 ? "0" + minutes : minutes.toString()
        return hourStr + ":" + minuteStr
    }

    function formatCurrentDate() {
        var now = new Date()
        var month = now.getMonth() + 1
        var day = now.getDate()
        // 手动添加前导零
        var monthStr = month < 10 ? "0" + month : month.toString()
        var dayStr = day < 10 ? "0" + day : day.toString()
        return monthStr + "/" + dayStr
    }

    // 实时更新时间的Timer
    Timer {
        id: timeUpdateTimer
        interval: 1000  // 每秒更新
        running: true
        repeat: true

        onTriggered: {
            currentTime = formatCurrentTime()
            currentDate = formatCurrentDate()
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

        // 状态栏组件
        StatusBar {
            id: statusBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: statusBarHeight
            z: 100

            // 传递属性 - 使用明确的绑定
            currentTime: mainWindow.currentTime
            currentDate: mainWindow.currentDate
            batteryLevel: mainWindow.batteryLevel
            isCharging: mainWindow.isCharging
            fontSize: mainWindow.fontSize * 1.5  // 增加50%
            mixedFontMedium: mixedFontMedium
            accentSuccess: accentSuccess
            accentWarning: accentWarning
            accentError: accentError
        }

        // 全屏主内容区域 - 动态页面容器
        Loader {
            id: pageLoader
            anchors.fill: parent

            // 根据当前页面加载不同的组件
            source: {
                switch(currentPage) {
                    case "home": return "pages/HomePage.qml"
                    case "media": return "pages/HomePage.qml" // 媒体库独立窗口，中心保持主页以减少干扰
                    case "measurement": return "pages/MeasurementPage.qml"
                    case "report": return "pages/HomePage.qml"
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

            onSourceChanged: {
                opacity = 0
                opacity = 1
            }
        }

        // 右侧工具栏（主页专用）
        Item {
            id: toolBarContainer
            visible: currentPage === "home"
            anchors.right: parent.right
            anchors.top: statusBar.bottom
            anchors.bottom: navigationContainer.top
            anchors.margins: margins
            width: 100
            z: 90

            ColumnLayout {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.rightMargin: 0
                anchors.topMargin: margins * 2
                spacing: margins

                // 相机调整按钮
                UniversalButton {
                    id: adjustButton
                    text: "画面调整"
                    iconSource: "qrc:/icons/config.svg"
                    buttonStyle: "toolbar"
                    onClicked: {
                        if (videoOperationsWindow.visibility === Window.Hidden || videoOperationsWindow.visibility === Window.Minimized) {
                            videoOperationsWindow.show()
                            videoOperationsWindow.raise()
                            videoOperationsWindow.requestActivate()
                        } else {
                            videoOperationsWindow.raise()
                            videoOperationsWindow.requestActivate()
                        }
                    }
                }

                // 拍照按钮（保存原图+处理后图 → Pictures）
                UniversalButton {
                    id: captureButton
                    text: "拍照"
                    iconSource: "qrc:/icons/camera.svg"
                    buttonStyle: "toolbar"
                    onClicked: {
                        if (!StorageManager || !CameraManager) return
                        var modeStr = (CameraManager.cameraMode === 2) ? "stereo" : "single"
                        var sessionDir = StorageManager.resolveCaptureSessionPath(modeStr)
                        if (sessionDir && sessionDir.length > 0) {
                            var ok = CameraManager.saveScreenshotSession(sessionDir)
                            if (ok) {
                                Logger.info("拍照已保存: " + sessionDir)
                            } else {
                                Logger.error("保存拍照失败")
                            }
                        } else {
                            Logger.error("无法解析拍照目录")
                        }
                    }
                }

                // 屏幕截图按钮（保存UI截图 → Screenshots）
                UniversalButton {
                    id: screenshotButton
                    text: "屏幕截图"
                    iconSource: "qrc:/icons/screenshot.svg"
                    buttonStyle: "toolbar"
                    onClicked: {
                        if (!StorageManager) return
                        var modeStr = (CameraManager && CameraManager.cameraMode === 2) ? "stereo" : "single"
                        var sessionDir = StorageManager.resolveScreenshotSessionPath(modeStr)
                        if (!sessionDir || sessionDir.length === 0) { Logger.error("无法解析截图目录"); return }
                        var filePath = sessionDir + "/screenshot.png"
                        // 抓取整个主内容容器
                        rootContainer.grabToImage(function(result) {
                            var saved = result.saveToFile(filePath)
                            if (saved) {
                                Logger.info("截图已保存: " + filePath)
                            } else {
                                Logger.error("保存截图失败: " + filePath)
                            }
                        })
                    }
                }

                // LED控制按钮
                UniversalButton {
                    id: ledControlButton
                    text: "LED控制"
                    iconSource: "qrc:/icons/brightness.svg"
                    buttonStyle: "toolbar"
                }

                // AI检测按钮
                UniversalButton {
                    id: aiDetectionButton
                    text: "AI检测"
                    iconSource: "qrc:/icons/AI.svg"
                    buttonStyle: "toolbar"
                    onClicked: {
                        // 切换AI检测状态
                        aiDetectionButton.isActive = !aiDetectionButton.isActive
                        if (typeof AiDetectionManager !== 'undefined' && AiDetectionManager) {
                            AiDetectionManager.enabled = aiDetectionButton.isActive
                        }
                    }
                }

                // 弹性填充空间
                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // 导航栏组件
        Item {
            id: navigationContainer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: navigationBarHeight + margins * 2
            z: 100

            NavigationBar {
                id: navigationBar
                anchors.bottom: parent.bottom
                anchors.bottomMargin: margins * 2
                anchors.horizontalCenter: parent.horizontalCenter
                width: availableNavWidth
                height: navigationBarHeight

                // 传递属性
                navigationBarHeight: navigationBarHeight
                margins: margins
                cornerRadius: cornerRadius
                mixedFontRegular: mixedFontRegular

                // 连接信号
                onHomeClicked: {
                    navigateTo("home")
                }
                onPreviewClicked: {
                    // 切换到“媒体库”状态以关闭相机/AI，但中心内容保持主页
                    navigateTo("media")
                    if (mediaLibraryWindow.visibility === Window.Hidden || mediaLibraryWindow.visibility === Window.Minimized) {
                        mediaLibraryWindow.show()
                        mediaLibraryWindow.raise()
                        mediaLibraryWindow.requestActivate()
                    } else {
                        mediaLibraryWindow.raise()
                        mediaLibraryWindow.requestActivate()
                    }
                }
                onReportClicked: {
                    navigateTo("report")
                }
                onSettingsClicked: {
                    settingsWindow.show()
                    settingsWindow.raise()
                    settingsWindow.requestActivate()
                }
                onMeasurementClicked: {
                    if (measurement3DWindow.visibility === Window.Hidden || measurement3DWindow.visibility === Window.Minimized) {
                        measurement3DWindow.show()
                        measurement3DWindow.raise()
                        measurement3DWindow.requestActivate()
                    } else {
                        measurement3DWindow.raise()
                        measurement3DWindow.requestActivate()
                    }
                }
                onExitClicked: {
                    exitDialog.visible = true
                }
            }
        }
    }

    // 退出确认对话框
    Item {
        id: exitDialog
        anchors.fill: parent
        visible: false
        z: 200

        // 半透明背景
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.75

            MouseArea {
                anchors.fill: parent
                onClicked: exitDialog.visible = false
            }
        }

        // 对话框卡片 - 毛玻璃效果
        Rectangle {
            id: dialogCard
            width: 700
            height: 320
            anchors.centerIn: parent
            color: Qt.rgba(0.16, 0.16, 0.18, 0.85)
            radius: cornerRadius * 2
            border.width: 2
            border.color: Qt.rgba(14, 165, 233, 0.3)

            // 内层毛玻璃效果
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: parent.radius - 1
                color: Qt.rgba(0.2, 0.2, 0.24, 0.6)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.05)
            }

            // 提示文本
            Text {
                id: dialogMessage
                text: "确定要退出应用吗？"
                font.family: mixedFontRegular
                font.pixelSize: Math.max(28, fontSize * 1.8)
                color: textPrimary
                anchors.top: parent.top
                anchors.topMargin: 60
                anchors.horizontalCenter: parent.horizontalCenter
                z: 1
            }

            // 按钮行
            Row {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 50
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 30
                z: 1

                // 取消按钮
                Rectangle {
                    id: cancelButton
                    width: 200
                    height: 90
                    radius: cornerRadius
                    color: Qt.rgba(0.12, 0.12, 0.12, 0.7)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.15)

                    property bool hovered: false

                    // 内层高光
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: parent.radius - 1
                        color: Qt.rgba(1, 1, 1, 0.03)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.05)
                    }

                    Text {
                        text: "取消"
                        font.family: mixedFontRegular
                        font.pixelSize: Math.max(28, fontSize * 1.8)
                        font.weight: Font.Medium
                        color: "#FFFFFF"
                        anchors.centerIn: parent
                        z: 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: exitDialog.visible = false
                        onEntered: parent.hovered = true
                        onExited: parent.hovered = false
                    }

                    states: State {
                        when: cancelButton.hovered
                        PropertyChanges {
                            target: cancelButton
                            color: Qt.rgba(0.2, 0.2, 0.2, 0.85)
                            border.color: Qt.rgba(14, 165, 233, 0.5)
                        }
                    }

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                    Behavior on border.color {
                        ColorAnimation { duration: 150 }
                    }
                }

                // 确认退出按钮
                Rectangle {
                    id: confirmButton
                    width: 200
                    height: 90
                    radius: cornerRadius
                    color: Qt.rgba(0.7, 0.2, 0.2, 0.85)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.15)

                    property bool hovered: false

                    // 内层高光
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: parent.radius - 1
                        color: Qt.rgba(1, 1, 1, 0.05)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                    }

                    Text {
                        text: "退出"
                        font.family: mixedFontRegular
                        font.pixelSize: Math.max(28, fontSize * 1.8)
                        font.weight: Font.Medium
                        color: "#FFFFFF"
                        anchors.centerIn: parent
                        z: 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.quit()
                        onEntered: parent.hovered = true
                        onExited: parent.hovered = false
                    }

                    states: State {
                        when: confirmButton.hovered
                        PropertyChanges {
                            target: confirmButton
                            color: Qt.rgba(0.85, 0.25, 0.25, 0.95)
                            border.color: Qt.rgba(1, 1, 1, 0.3)
                        }
                    }

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                    Behavior on border.color {
                        ColorAnimation { duration: 150 }
                    }
                }
            }
        }
    }

    // 当前页面状态
    property string currentPage: "home"

    // 页面路由函数
    // 仅在主页显示与处理视频（含AI检测/后处理），其他页面停止相机并关闭AI以节省资源
    property bool _prevAiEnabled: false
    function navigateTo(pageName) {
        // 管理视频与AI检测开关
        if (pageName === "home") {
            // 恢复相机与AI（尊重用户之前的AI启用状态）
            if (CameraManager && !CameraManager.cameraRunning) {
                CameraManager.startCameraAsync()
            }
            if (typeof AiDetectionManager !== 'undefined' && AiDetectionManager) {
                // 仅在用户先前启用了AI时恢复
                AiDetectionManager.enabled = _prevAiEnabled
            }
        } else {
            // 离开主页：记录AI状态并关闭，停止相机
            if (typeof AiDetectionManager !== 'undefined' && AiDetectionManager) {
                _prevAiEnabled = AiDetectionManager.enabled
                AiDetectionManager.enabled = false
            }
            if (CameraManager && CameraManager.cameraRunning) {
                CameraManager.stopCameraAsync()
            }
        }

        currentPage = pageName

        // 更新导航栏按钮状态
        navigationBar.setActiveTab(pageName)
    }

    // 快速返回首页
    function goHome() {
        navigateTo("home")
    }
    // 3D测量窗口实例
    Measurement3DWindow {
        id: measurement3DWindow
        onClosing: mainWindow.goHome()
        onVisibleChanged: if (!visible) mainWindow.goHome()
    }

    // 系统设置窗口实例（独立窗口，复用主窗口状态栏样式）
    SettingsWindow {
        id: settingsWindow
        // 同步主窗口状态栏信息与字体参数
        currentTime: mainWindow.currentTime
        currentDate: mainWindow.currentDate
        batteryLevel: mainWindow.batteryLevel
        isCharging: mainWindow.isCharging
        fontSize: mainWindow.fontSize
        mixedFontMedium: mixedFontMedium
        statusBarHeight: mainWindow.statusBarHeight
        margins: margins
        onClosing: mainWindow.goHome()
        onVisibleChanged: if (!visible) mainWindow.goHome()
    }

    // 视频操作窗口实例
    VideoOperationsWindow {
        id: videoOperationsWindow
        onOpenCameraParameters: {
            // 打开相机参数设置窗口
            cameraParameterWindow.show()
        }
        onClosing: mainWindow.goHome()
        onVisibleChanged: if (!visible) mainWindow.goHome()
    }

    // 相机参数设置窗口实例
    CameraParameterWindow {
        id: cameraParameterWindow
        onClosing: mainWindow.goHome()
        onVisibleChanged: if (!visible) mainWindow.goHome()
    }

    
    // 全局录制工具栏（独立置顶窗口）
    RecordingToolWindow {
        id: recordingTool
        // 可在此连接录制开始/停止与后端桥接
        onRecordingToggled: function(on) {
            Logger.info(on ? "开始录制" : "停止录制")
            if (on) {
                var dir = StorageManager.resolveVideoSessionPath("screen")
                var ts = Qt.formatDateTime(new Date(), "yyyyMMdd_HHmmss")
                var file = dir + "/screen_" + ts + ".mp4"
                // 固定为屏幕刷新率（不自适应）
                var fps = Math.max(1, Math.round(Screen.refreshRate || 60))
                if (!ScreenRecorderManager.startScreenRecording(file, fps, 4000000)) {
                    Logger.error("启动屏幕录制失败: " + file)
                    recordingTool.recording = false
                }
            } else {
                ScreenRecorderManager.stopScreenRecording()
            }
        }
    }

    // 媒体库窗口实例（独立窗口）
    MediaLibraryWindow {
        id: mediaLibraryWindow
        currentTime: mainWindow.currentTime
        currentDate: mainWindow.currentDate
        batteryLevel: mainWindow.batteryLevel
        isCharging: mainWindow.isCharging
        fontSize: mainWindow.fontSize
        mixedFontMedium: mixedFontMedium
        statusBarHeight: mainWindow.statusBarHeight
        margins: margins
        backgroundColor: mainWindow.backgroundColor
        backgroundSecondary: mainWindow.backgroundSecondary
        textPrimary: mainWindow.textPrimary
        borderPrimary: mainWindow.borderPrimary
        onClosing: mainWindow.goHome()
        onVisibleChanged: if (!visible) mainWindow.goHome()
    }
Component.onCompleted: {
        // 立即更新时间
        currentTime = formatCurrentTime()
        currentDate = formatCurrentDate()

        Logger.info("SmartScope UI initialized")
    }
}
