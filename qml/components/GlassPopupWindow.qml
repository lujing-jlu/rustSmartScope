import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

// 通用弹窗组件 - 全屏透明窗口，中间显示毛玻璃卡片
Window {
    id: popupWindow

    // 公共属性
    property string windowTitle: "弹窗"
    property int windowWidth: 1100
    property int windowHeight: 850
    property bool showCloseButton: true

    // 内容组件 - 由外部提供
    property alias content: contentLoader.sourceComponent

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

    // 信号
    signal closeRequested()
    signal opened()
    signal closed()

    // 全屏无边框透明窗口
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    // 设置窗口为屏幕尺寸并居中
    Component.onCompleted: {
        width = Screen.width
        height = Screen.height
        x = 0
        y = 0
    }

    // 窗口显示时发出信号
    onVisibleChanged: {
        if (visible) {
            opened()
        } else {
            closed()
        }
    }

    // 全屏背景区域 - 点击可关闭窗口
    MouseArea {
        anchors.fill: parent
        onClicked: closeRequested()
    }

    // 主容器 - 毛玻璃效果卡片（居中显示）
    Rectangle {
        id: mainContainer
        anchors.centerIn: parent
        width: windowWidth
        height: windowHeight
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
                visible: windowTitle !== ""

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: windowTitle
                    font.family: fontFamily
                    font.pixelSize: 32
                    font.weight: Font.Medium
                    color: textPrimary
                    z: 1
                }

                // 关闭按钮
                Rectangle {
                    id: closeButton
                    visible: showCloseButton
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
                        onClicked: closeRequested()
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
                visible: windowTitle !== ""
            }

            // 动态内容区域
            Item {
                width: parent.width
                height: parent.height - (windowTitle !== "" ? 81 : 0) - (windowTitle !== "" ? 1 : 0)

                Loader {
                    id: contentLoader
                    anchors.fill: parent
                }
            }
        }
    }

    // 公共函数
    function show() {
        visible = true
        raise()
        requestActivate()
    }

    function hide() {
        visible = false
    }

    function close() {
        visible = false
    }

    // 默认处理关闭请求
    onCloseRequested: {
        close()
    }
}