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
    width: 80
    height: 80
    color: backgroundColor
    radius: 12
    border.width: 2
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
        width: 56  // 增大图标容器大小
        height: 56

        // 活跃状态背景 - 移除圆形背景
        // Rectangle {
        //     anchors.fill: parent
        //     color: toolButton.isActive ? activeColor : "transparent"
        //     radius: width / 2
        //     opacity: toolButton.isActive ? 0.15 : 0
        //     Behavior on opacity { PropertyAnimation { duration: 300 } }
        // }

        // 图标
        Image {
            id: icon
            source: iconSource
            anchors.centerIn: parent
            width: 28   // 缩小图标大小，确保一致性
            height: 28
            fillMode: Image.PreserveAspectFit
            visible: false
        }

        ColorOverlay {
            anchors.centerIn: parent
            width: 28
            height: 28
            source: icon
            color: toolButton.isActive ? activeColor : "#FFFFFF"
            Behavior on color { ColorAnimation { duration: 300 } }
        }

        DropShadow {
            anchors.centerIn: parent
            width: 28
            height: 28
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
            to: 3
            duration: 100
        }
        PropertyAnimation {
            target: toolButton
            property: "border.width"
            to: 2
            duration: 200
        }
    }

    // 点击触发触觉反馈
    onClicked: hapticFeedback.start()
}