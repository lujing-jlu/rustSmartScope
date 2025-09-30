import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

Item {
    id: navButton

    property string icon: ""
    property string label: ""
    property string shortcut: ""
    property bool isActive: false

    signal clicked()

    width: 120
    height: 60

    Rectangle {
        id: background
        anchors.fill: parent
        color: {
            if (isActive) return "#5E81AC"
            if (mouseArea.pressed) return "#4C566A"
            if (mouseArea.containsMouse) return "#3B4252"
            return "transparent"
        }
        radius: 30
        border.color: isActive ? "#81A1C1" : "transparent"
        border.width: isActive ? 2 : 0

        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 8

        Image {
            source: navButton.icon
            width: 32
            height: 32
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: parent.verticalCenter

            // SVG颜色处理 - 设置为白色
            ColorOverlay {
                anchors.fill: parent
                source: parent
                color: isActive ? "#FFFFFF" : "#E0E0E0"
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: navButton.label
                font.pixelSize: 12
                font.bold: true
                color: isActive ? "#FFFFFF" : "#E0E0E0"
            }

            Text {
                text: navButton.shortcut
                font.pixelSize: 9
                color: "#A0A0A0"
                visible: shortcut !== ""
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            clickAnimation.start()
            navButton.clicked()
        }
    }

    // 悬停效果
    scale: mouseArea.pressed ? 0.95 : (mouseArea.containsMouse ? 1.05 : 1.0)

    // 提示气泡
    ToolTip {
        visible: mouseArea.containsMouse
        text: label + (shortcut ? " (" + shortcut + ")" : "")
        delay: 500
    }

    // 点击反馈动画
    SequentialAnimation {
        id: clickAnimation
        PropertyAnimation {
            target: navButton
            property: "scale"
            to: 0.9
            duration: 50
        }
        PropertyAnimation {
            target: navButton
            property: "scale"
            to: 1.1
            duration: 100
        }
        PropertyAnimation {
            target: navButton
            property: "scale"
            to: 1.0
            duration: 50
        }
    }

    // 按钮动画
    Behavior on scale {
        NumberAnimation {
            duration: 150
            easing.type: Easing.OutCubic
        }
    }
}