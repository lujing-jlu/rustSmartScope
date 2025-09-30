import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

Item {
    id: toolButton

    property string iconText: ""
    property string label: ""
    property string shortcut: ""
    property bool isToggle: false
    property bool isToggled: false
    property string toolId: ""
    property bool enabled: true

    signal clicked()

    width: 60
    height: 60

    Rectangle {
        id: background
        anchors.fill: parent
        color: {
            if (isToggle && isToggled) return "#A3BE8C"
            if (mouseArea.pressed) return "#5E81AC"
            if (mouseArea.containsMouse) return "#4C566A"
            return "transparent"
        }
        radius: 30
        border.color: {
            if (isToggle && isToggled) return "#8FBCBB"
            if (mouseArea.containsMouse) return "#81A1C1"
            return "transparent"
        }
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 200 }
        }

        // 呼吸效果（用于切换状态）
        SequentialAnimation on opacity {
            running: isToggle && isToggled
            loops: Animation.Infinite
            PropertyAnimation { to: 0.6; duration: 1000 }
            PropertyAnimation { to: 1.0; duration: 1000 }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 2

        Image {
            source: toolButton.iconText
            width: 20
            height: 20
            fillMode: Image.PreserveAspectFit
            anchors.horizontalCenter: parent.horizontalCenter

            ColorOverlay {
                anchors.fill: parent
                source: parent
                color: {
                    if (!toolButton.enabled) return "#4C566A"
                    if (isToggle && isToggled) return "#2E3440"
                    return "#ECEFF4"
                }
            }
        }

        Text {
            text: toolButton.label
            font.pixelSize: 8
            color: {
                if (!toolButton.enabled) return "#4C566A"
                if (isToggle && isToggled) return "#2E3440"
                return "#88C0D0"
            }
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            wrapMode: Text.WordWrap
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if (enabled) {
                clickFeedback.start()
                toolButton.clicked()
            }
        }
    }

    // 禁用时的透明度
    opacity: enabled ? 1.0 : 0.5

    // 悬停和按压效果
    scale: {
        if (!enabled) return 1.0
        if (mouseArea.pressed) return 0.9
        if (mouseArea.containsMouse) return 1.1
        return 1.0
    }

    Behavior on scale {
        NumberAnimation {
            duration: 150
            easing.type: Easing.OutCubic
        }
    }

    // 提示气泡
    ToolTip {
        visible: mouseArea.containsMouse && enabled
        text: label + (shortcut ? " (" + shortcut + ")" : "")
        delay: 500
    }

    // 点击反馈
    SequentialAnimation {
        id: clickFeedback
        PropertyAnimation {
            target: toolButton
            property: "scale"
            to: 0.8
            duration: 50
        }
        PropertyAnimation {
            target: toolButton
            property: "scale"
            to: 1.2
            duration: 100
        }
        PropertyAnimation {
            target: toolButton
            property: "scale"
            to: 1.0
            duration: 50
        }
    }
}