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
    property bool iconOnly: false  // 只显示图标模式
    property color activeColor: "#0EA5E9"
    property color hoverColor: Qt.rgba(0.31, 0.31, 0.31, 0.7)
    property color backgroundColor: isActive ?
        Qt.rgba(0.2, 0.2, 0.2, 0.8) : Qt.rgba(0.12, 0.12, 0.12, 0.6)
    property color exitBackgroundColor: hovered ?
        Qt.rgba(0.8, 0.2, 0.2, 0.8) : Qt.rgba(0.6, 0.15, 0.15, 0.6)

    // 信号
    signal clicked()

    // 私有属性
    property bool hovered: false

    // 样式设置
    Layout.preferredWidth: dynamicButtonWidth
    Layout.fillHeight: true
    color: isExitButton ? exitBackgroundColor : backgroundColor
    radius: cornerRadius
    border.width: 3
    border.color: isExitButton ?
        Qt.rgba(1, 0.4, 0.4, 0.3) : Qt.rgba(1, 1, 1, 0.1)

    // 鼠标交互
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
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
        color: isExitButton ?
            Qt.rgba(0.8, 0.3, 0.3, 0.7) : hoverColor
        radius: parent.radius
        opacity: navButton.hovered ? 1 : 0
        Behavior on opacity { PropertyAnimation { duration: 200 } }
    }

    // 按钮内容
    Row {
        anchors.centerIn: parent
        spacing: iconOnly ? 0 : spacing * 0.6

        // 图标容器
        Item {
            width: iconSize
            height: iconSize
            anchors.verticalCenter: parent.verticalCenter

            // 活跃状态背景
            Rectangle {
                anchors.fill: parent
                color: navButton.isActive ? activeColor : "transparent"
                radius: width / 2
                opacity: navButton.isActive ? 0.15 : 0
                Behavior on opacity { PropertyAnimation { duration: 300 } }
            }

            // 图标或文字
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

                    DropShadow {
                        anchors.fill: icon
                        horizontalOffset: 0
                        verticalOffset: 2
                        radius: 8
                        samples: 17
                        color: activeColor
                        opacity: navButton.isActive ? 0.4 : 0
                        source: icon
                        Behavior on opacity { PropertyAnimation { duration: 300 } }
                    }
                }
            }

            // 文字图标组件（用于退出按钮）
            Component {
                id: textIconComponent
                Text {
                    text: "✕"
                    color: "#FFFFFF"
                    font.pixelSize: iconSize * 1.2
                    font.weight: Font.Bold
                    anchors.centerIn: parent
                }
            }
        }

        // 按钮文字 (只在非图标模式显示)
        Text {
            text: navButton.text
            color: "#FFFFFF"
            font.pixelSize: 38
            font.weight: Font.Medium
            font.family: mixedFontRegular
            anchors.verticalCenter: parent.verticalCenter
            visible: !iconOnly
            Behavior on color { ColorAnimation { duration: 300 } }
        }
    }

    // 触觉反馈动画
    SequentialAnimation {
        id: hapticFeedback
        PropertyAnimation {
            target: navButton
            property: "border.width"
            to: 3
            duration: 100
        }
        PropertyAnimation {
            target: navButton
            property: "border.width"
            to: navButton.isActive ? 4 : 3
            duration: 200
        }
    }

    // 点击触发触觉反馈
    onClicked: hapticFeedback.start()
}