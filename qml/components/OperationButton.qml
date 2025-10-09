import QtQuick 2.15

// 操作按钮组件 - 用于视频操作窗口（毛玻璃效果风格，方形）
Rectangle {
    id: opBtn
    width: 170
    height: 75
    radius: 8

    property string text: ""
    property bool isActive: false
    property bool hovered: false
    property color buttonColor: Qt.rgba(0.12, 0.12, 0.12, 0.7)
    property color hoverColor: Qt.rgba(0.2, 0.2, 0.2, 0.85)
    property color activeColor: Qt.rgba(14, 165, 233, 0.3)

    signal clicked()

    color: {
        if (isActive) return activeColor
        if (hovered) return hoverColor
        return buttonColor
    }

    border.width: isActive ? 2 : 1
    border.color: isActive ? Qt.rgba(14, 165, 233, 0.8) : Qt.rgba(1, 1, 1, 0.15)

    // 内层高光效果
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: parent.radius - 1
        color: Qt.rgba(1, 1, 1, 0.03)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.05)
        visible: !isActive
    }

    Text {
        anchors.centerIn: parent
        text: opBtn.text
        font.family: "Inter"
        font.pixelSize: 22
        font.weight: Font.Medium
        color: opBtn.isActive ? "#38BDF8" : "#FFFFFF"
        z: 1
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: opBtn.clicked()
        onEntered: opBtn.hovered = true
        onExited: opBtn.hovered = false
    }

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }
}
