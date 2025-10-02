import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Rectangle {
    id: navButton

    // 公共属性
    property string text: ""
    property string iconSource: ""
    property bool isActive: false
    property bool isExitButton: false
    property bool iconOnly: false
    property bool isSquareButton: false  // 新增：方形按钮模式
    property color activeColor: "#0EA5E9"
    property color hoverColor: Qt.rgba(0.31, 0.31, 0.31, 0.7)

    // 统一的按钮样式
    property color buttonColor: isExitButton ?
        (hovered ? Qt.rgba(0.8, 0.2, 0.2, 0.8) : Qt.rgba(0.6, 0.15, 0.15, 0.6)) :
        Qt.rgba(0.12, 0.12, 0.12, 0.6)

    property color borderColor: isExitButton ?
        Qt.rgba(1, 0.4, 0.4, 0.3) : Qt.rgba(1, 1, 1, 0.1)

    // 信号
    signal clicked()

    // 私有属性
    property bool hovered: false

    // 样式设置 - 固定尺寸，不使用计算值
    property int fixedButtonHeight: 80  // 与工具栏按钮相同高度
    property int fixedButtonWidth: 160  // 长方形按钮固定宽度
    width: isSquareButton ? fixedButtonHeight : fixedButtonWidth
    height: fixedButtonHeight
    color: buttonColor
    radius: cornerRadius
    border.width: 3
    border.color: borderColor

    // 鼠标交互
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: navButton.clicked()
        onPressed: navButton.scale = 0.92
        onReleased: navButton.scale = 1.0
        onEntered: navButton.hovered = true
        onExited: navButton.hovered = false
    }

    // 动画效果
    Behavior on scale {
        SpringAnimation { spring: 4; damping: 0.6; duration: 200 }
    }
    Behavior on color {
        ColorAnimation { duration: 300; easing.type: Easing.OutCubic }
    }
    Behavior on border.color {
        ColorAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    // 悬停背景效果
    Rectangle {
        anchors.fill: parent
        color: hoverColor
        radius: parent.radius
        opacity: navButton.hovered ? 1 : 0
        Behavior on opacity { PropertyAnimation { duration: 200 } }
    }

    // 按钮内容
    Row {
        anchors.centerIn: parent
        spacing: iconOnly ? 0 : 8  // 固定8px间距，不使用计算值

        // 图标容器
        Item {
            width: iconSize
            height: iconSize
            anchors.verticalCenter: parent.verticalCenter

            // 图标加载器
            Loader {
                anchors.fill: parent
                sourceComponent: iconSource ? iconComponent : textIconComponent
            }

            // 图标组件
            Component {
                id: iconComponent
                Item {
                    Image {
                        id: icon
                        source: iconSource
                        anchors.fill: parent
                        anchors.margins: parent.width * 0.15
                        fillMode: Image.PreserveAspectFit
                        visible: false
                    }

                    ColorOverlay {
                        anchors.fill: icon
                        source: icon
                        color: "#FFFFFF"
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                }
            }

            // 文字图标组件（用于退出按钮等）
            Component {
                id: textIconComponent
                Text {
                    text: "✕"
                    color: "#FFFFFF"
                    font.pixelSize: iconSize * 0.6
                    font.weight: Font.Bold
                    anchors.centerIn: parent
                }
            }
        }

        // 按钮文字 (只在非图标模式显示)
        Text {
            text: navButton.text
            color: "#FFFFFF"
            font.pixelSize: fontSize * 1.4
            font.weight: Font.Medium
            font.family: mixedFontRegular
            anchors.verticalCenter: parent.verticalCenter
            visible: !iconOnly
            Behavior on color { ColorAnimation { duration: 300 } }
        }
    }
}