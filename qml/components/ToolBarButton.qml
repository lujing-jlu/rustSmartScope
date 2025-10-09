import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Rectangle {
    id: toolButton

    // 公共属性
    property string text: ""
    property string iconSource: ""
    property bool isActive: false
    property color activeColor: "#0EA5E9"
    property color hoverColor: Qt.rgba(0.31, 0.31, 0.31, 0.7)
    property color backgroundColor: isActive ?
        Qt.rgba(0.2, 0.2, 0.2, 0.8) : Qt.rgba(0.12, 0.12, 0.12, 0.6)

    // 信号
    signal clicked()

    // 私有属性
    property bool hovered: false

    // 样式设置
    width: 82  // 放大70% (48 * 1.7)
    height: 82
    color: backgroundColor
    radius: 8
    border.width: 1.5
    border.color: isActive ?
        Qt.rgba(14/255, 165/255, 233/255, 0.5) : Qt.rgba(1, 1, 1, 0.1)

    // 鼠标交互
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: toolButton.clicked()
        onPressed: toolButton.scale = 0.92
        onReleased: toolButton.scale = 1.0
        onEntered: toolButton.hovered = true
        onExited: toolButton.hovered = false
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
        opacity: toolButton.hovered ? 1 : 0
        Behavior on opacity { PropertyAnimation { duration: 200 } }
    }

    // 图标内容
    Item {
        anchors.centerIn: parent
        width: 70  // 接近填满按钮 (82 * 0.85)
        height: 70

        // 图标
        Image {
            id: icon
            source: iconSource
            anchors.centerIn: parent
            width: 56   // 填满大部分空间 (70 * 0.8)
            height: 56
            fillMode: Image.PreserveAspectFit
            visible: false
        }

        ColorOverlay {
            anchors.centerIn: parent
            width: 56  // 填满大部分空间 (70 * 0.8)
            height: 56
            source: icon
            color: toolButton.isActive ? activeColor : "#FFFFFF"
            Behavior on color { ColorAnimation { duration: 300 } }
        }

        DropShadow {
            anchors.centerIn: parent
            width: 56  // 填满大部分空间 (70 * 0.8)
            height: 56
            horizontalOffset: 0
            verticalOffset: 2
            radius: 8
            samples: 17
            color: activeColor
            opacity: toolButton.isActive ? 0.4 : 0
            source: icon
            Behavior on opacity { PropertyAnimation { duration: 300 } }
        }
    }

    // 触觉反馈动画
    SequentialAnimation {
        id: hapticFeedback
        PropertyAnimation {
            target: toolButton
            property: "border.width"
            to: 2.5
            duration: 100
        }
        PropertyAnimation {
            target: toolButton
            property: "border.width"
            to: 1.5
            duration: 200
        }
    }

    // 点击触发触觉反馈
    onClicked: hapticFeedback.start()
}