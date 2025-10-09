import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "components"

// 视频操作窗口 - 无边框圆角毛玻璃效果窗口
Window {
    id: videoOperationsWindow
    width: 1100
    height: 850
    title: "画面调整"

    // 无边框、半透明背景
    flags: Qt.Dialog | Qt.FramelessWindowHint
    color: "transparent"

    // 窗口显示时居中
    Component.onCompleted: {
        setX(Screen.width / 2 - width / 2)
        setY(Screen.height / 2 - height / 2)
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

    // 主容器 - 毛玻璃效果卡片（直接填充整个窗口，不留边距）
    Rectangle {
        id: mainContainer
        anchors.fill: parent
        color: Qt.rgba(0.16, 0.16, 0.18, 0.90)
        radius: cornerRadius
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

            // 按钮网格布局
            Item {
                width: parent.width
                height: parent.height - 80 - 1

                Grid {
                    anchors.centerIn: parent
                    columns: 3
                    rows: 2
                    columnSpacing: 20
                    rowSpacing: 20

                    // 旋转按钮
                    OperationButton {
                        text: "旋转"
                        width: 200
                        height: 180
                        onClicked: {
                            rotationDegrees = (rotationDegrees + 90) % 360
                            applyTransformations()
                        }
                    }

                    // 水平翻转按钮
                    OperationButton {
                        text: "水平翻转"
                        width: 200
                        height: 180
                        isActive: flipHorizontal
                        onClicked: {
                            flipHorizontal = !flipHorizontal
                            applyTransformations()
                        }
                    }

                    // 垂直翻转按钮
                    OperationButton {
                        text: "垂直翻转"
                        width: 200
                        height: 180
                        isActive: flipVertical
                        onClicked: {
                            flipVertical = !flipVertical
                            applyTransformations()
                        }
                    }

                    // 反色按钮
                    OperationButton {
                        text: "反色"
                        width: 200
                        height: 180
                        isActive: invertColors
                        onClicked: {
                            invertColors = !invertColors
                            applyTransformations()
                        }
                    }

                    // 占位按钮（可以后续扩展其他功能）
                    Rectangle {
                        width: 200
                        height: 180
                        color: "transparent"
                    }

                    // 还原按钮
                    OperationButton {
                        text: "还原"
                        width: 200
                        height: 180
                        buttonColor: Qt.rgba(0.7, 0.2, 0.2, 0.6)
                        hoverColor: Qt.rgba(0.85, 0.25, 0.25, 0.8)
                        onClicked: {
                            rotationDegrees = 0
                            flipHorizontal = false
                            flipVertical = false
                            invertColors = false
                            applyTransformations()
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
        // TODO: 调用C++后端应用变换
        console.log("应用变换:")
        console.log("  旋转:", rotationDegrees, "度")
        console.log("  水平翻转:", flipHorizontal)
        console.log("  垂直翻转:", flipVertical)
        console.log("  反色:", invertColors)
    }
}
