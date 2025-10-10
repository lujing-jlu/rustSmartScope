import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "components"

// 视频操作窗口 - 全屏透明窗口，中间显示毛玻璃卡片
Window {
    id: videoOperationsWindow
    width: Screen.width
    height: Screen.height
    title: "画面调整"

    // 全屏无边框透明窗口
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    // 窗口显示时填充整个屏幕
    Component.onCompleted: {
        setX(0)
        setY(0)
    }

    // 现代化设计系统颜色
    property color accentPrimary: "#0EA5E9"
    property color accentSecondary: "#38BDF8"
    property color textPrimary: "#FFFFFF"
    property color textSecondary: "#E2E8F0"
    property color textMuted: "#94A3B8"
    property color borderPrimary: "#334155"

    // 字体
    property string fontFamily: "Inter"
    property int cornerRadius: 24

    // 当前变换状态
    property int rotationDegrees: 0
    property bool flipHorizontal: false
    property bool flipVertical: false
    property bool invertColors: false

    // 曝光控制状态
    property bool autoExposureEnabled: true
    property int exposurePresetIndex: 0
    property var exposurePresets: [50, 100, 300, 500, 1000, 1500]

    // 亮度控制状态
    property int brightnessLevel: 0
    property var brightnessLevels: [0, 25, 50, 75, 100]

    // 全屏背景区域 - 点击可关闭窗口
    MouseArea {
        anchors.fill: parent
        onClicked: videoOperationsWindow.close()
    }

    // 主容器 - 毛玻璃效果卡片（居中显示）
    Rectangle {
        id: mainContainer
        anchors.centerIn: parent
        width: 1100
        height: 850
        color: Qt.rgba(0.16, 0.16, 0.18, 0.90)
        radius: cornerRadius
        border.width: 2
        border.color: Qt.rgba(14, 165, 233, 0.3)

        // 阻止点击事件传递到背景MouseArea
        MouseArea {
            anchors.fill: parent
            onClicked: {} // 阻止事件冒泡，保持窗口不关闭
        }

        // 内层毛玻璃效果
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            color: Qt.rgba(0.2, 0.2, 0.24, 0.6)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.05)
        }

        // 内容区域
        Column {
            anchors.fill: parent
            anchors.margins: 40
            spacing: 0

            // 标题栏
            Item {
                width: parent.width
                height: 80

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "画面调整"
                    font.family: fontFamily
                    font.pixelSize: 32
                    font.weight: Font.Medium
                    color: textPrimary
                    z: 1
                }

                // 关闭按钮
                Rectangle {
                    id: closeButton
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 60
                    height: 60
                    radius: 30
                    color: Qt.rgba(0.7, 0.2, 0.2, 0.85)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.15)
                    z: 1

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
                        anchors.centerIn: parent
                        text: "×"
                        font.pixelSize: 36
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                        z: 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: videoOperationsWindow.close()
                        onEntered: parent.hovered = true
                        onExited: parent.hovered = false
                    }

                    states: State {
                        when: closeButton.hovered
                        PropertyChanges {
                            target: closeButton
                            color: Qt.rgba(0.85, 0.25, 0.25, 0.95)
                            border.color: Qt.rgba(1, 1, 1, 0.3)
                        }
                    }

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                }
            }

            // 分隔线
            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(14, 165, 233, 0.3)
                z: 1
            }

            // 按钮网格布局（3x3）
            Item {
                width: parent.width
                height: parent.height - 80 - 1

                Grid {
                    anchors.centerIn: parent
                    columns: 3
                    rows: 3
                    columnSpacing: 20
                    rowSpacing: 18

                    // 第一行
                    // 旋转按钮
                    OperationButton {
                        text: "旋转"
                        iconSource: "qrc:/icons/rotate.svg"
                        onClicked: {
                            VideoTransformManager.applyRotation()
                            rotationDegrees = VideoTransformManager.rotationDegrees
                            applyTransformations()
                        }
                    }

                    // 水平翻转按钮
                    OperationButton {
                        text: "水平翻转"
                        iconSource: "qrc:/icons/horizontal_filp.svg"
                        isActive: flipHorizontal
                        onClicked: {
                            VideoTransformManager.toggleFlipHorizontal()
                            flipHorizontal = VideoTransformManager.flipHorizontal
                            applyTransformations()
                        }
                    }

                    // 垂直翻转按钮
                    OperationButton {
                        text: "垂直翻转"
                        iconSource: "qrc:/icons/vertical_filp.svg"
                        isActive: flipVertical
                        onClicked: {
                            VideoTransformManager.toggleFlipVertical()
                            flipVertical = VideoTransformManager.flipVertical
                            applyTransformations()
                        }
                    }

                    // 第二行
                    // 反色按钮
                    OperationButton {
                        text: "反色"
                        iconSource: "qrc:/icons/invert_color.svg"
                        isActive: invertColors
                        onClicked: {
                            VideoTransformManager.toggleInvert()
                            invertColors = VideoTransformManager.invertColors
                            applyTransformations()
                        }
                    }

                    // 自动曝光按钮
                    OperationButton {
                        id: exposureButton
                        text: autoExposureEnabled ? "自动曝光" : ("曝光 " + exposurePresets[exposurePresetIndex])
                        iconSource: autoExposureEnabled ? "qrc:/icons/auto_exposure.svg" : "qrc:/icons/exposure.svg"
                        onClicked: {
                            cycleExposure()
                        }
                    }

                    // 亮度按钮
                    OperationButton {
                        id: brightnessButton
                        text: "亮度 " + brightnessLevels[brightnessLevel] + "%"
                        iconSource: "qrc:/icons/brightness.svg"
                        onClicked: {
                            cycleBrightness()
                        }
                    }

                    // 第三行
                    // 畸变校正按钮
                    OperationButton {
                        text: "畸变校正"
                        iconSource: "qrc:/icons/distortion.svg"
                        isActive: VideoTransformManager.distortionCorrectionEnabled
                        onClicked: {
                            toggleDistortionCorrection()
                        }
                    }

                    // 还原按钮
                    OperationButton {
                        text: "还原"
                        iconSource: "qrc:/icons/restore.svg"
                        buttonColor: Qt.rgba(0.7, 0.2, 0.2, 0.6)
                        hoverColor: Qt.rgba(0.85, 0.25, 0.25, 0.8)
                        onClicked: {
                            resetAll()
                        }
                    }

                    // 高级模式按钮
                    OperationButton {
                        text: "高级模式"
                        iconSource: "qrc:/icons/advanced_settings.svg"
                        onClicked: {
                            openAdvancedSettings()
                        }
                    }
                }
            }
        }
    }

    // 辅助函数：获取当前状态文本
    function getCurrentStatusText() {
        var status = []

        if (rotationDegrees !== 0) {
            status.push("旋转: " + rotationDegrees + "°")
        }

        if (flipHorizontal) {
            status.push("水平翻转")
        }

        if (flipVertical) {
            status.push("垂直翻转")
        }

        if (invertColors) {
            status.push("反色")
        }

        if (status.length === 0) {
            return "未应用任何变换"
        }

        return "当前变换: " + status.join(" | ")
    }

    // 应用变换
    function applyTransformations() {
        console.log("应用变换:")
        console.log("  旋转:", rotationDegrees, "度")
        console.log("  水平翻转:", flipHorizontal)
        console.log("  垂直翻转:", flipVertical)
        console.log("  反色:", invertColors)
        // 变换会自动在C++端应用，这里仅用于日志
    }

    // 曝光循环：自动曝光 -> 50 -> 100 -> 300 -> 500 -> 1000 -> 1500 -> 自动曝光
    function cycleExposure() {
        if (autoExposureEnabled) {
            // 从自动切换到手动第一档（50）
            autoExposureEnabled = false
            exposurePresetIndex = 0
            console.log("曝光模式: 手动", exposurePresets[exposurePresetIndex])
            // TODO: 调用C++后端设置曝光
        } else {
            // 手动模式下循环档位
            exposurePresetIndex++
            if (exposurePresetIndex >= exposurePresets.length) {
                // 超过最后一档，回到自动曝光
                autoExposureEnabled = true
                exposurePresetIndex = 0
                console.log("曝光模式: 自动")
                // TODO: 调用C++后端设置自动曝光
            } else {
                console.log("曝光值:", exposurePresets[exposurePresetIndex])
                // TODO: 调用C++后端设置曝光值
            }
        }
    }

    // 亮度循环：0% -> 25% -> 50% -> 75% -> 100% -> 0%
    function cycleBrightness() {
        brightnessLevel = (brightnessLevel + 1) % brightnessLevels.length
        console.log("亮度:", brightnessLevels[brightnessLevel] + "%")
        // TODO: 调用C++后端设置LED亮度
    }

    // 畸变校正开关
    function toggleDistortionCorrection() {
        VideoTransformManager.toggleDistortionCorrection()
        console.log("畸变校正: ", VideoTransformManager.distortionCorrectionEnabled ? "启用" : "禁用")
    }

    // 还原所有设置
    function resetAll() {
        VideoTransformManager.resetAll()
        rotationDegrees = VideoTransformManager.rotationDegrees
        flipHorizontal = VideoTransformManager.flipHorizontal
        flipVertical = VideoTransformManager.flipVertical
        invertColors = VideoTransformManager.invertColors
        autoExposureEnabled = true
        exposurePresetIndex = 0
        brightnessLevel = 0
        console.log("已还原所有设置")
        applyTransformations()
    }

    // 打开高级设置
    function openAdvancedSettings() {
        console.log("打开高级设置面板")
        videoOperationsWindow.close()
        // TODO: 打开高级设置窗口/面板
    }
}
