import QtQuick 2.15
import QtGraphicalEffects 1.15

// 操作按钮组件 - 用于视频操作窗口（毛玻璃效果风格，方形，图标在上文字在下）
Rectangle {
    id: opBtn
    width: 200
    height: 170
    radius: 12

    property string text: ""
    property string iconSource: ""
    property bool isActive: false
    property bool hovered: false
    property color buttonColor: Qt.rgba(58/255, 58/255, 58/255, 0.51)
    property color hoverColor: Qt.rgba(74/255, 74/255, 74/255, 0.59)
    property color activeColor: Qt.rgba(14, 165, 233, 0.3)

    signal clicked()

    color: {
        if (isActive) return activeColor
        if (hovered) return hoverColor
        return buttonColor
    }

    border.width: isActive ? 2 : 1
    border.color: isActive ? Qt.rgba(14, 165, 233, 0.8) : Qt.rgba(74/255, 74/255, 74/255, 0.63)

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

    // 内容列布局：图标在上，文字在下
    Column {
        anchors.centerIn: parent
        spacing: 10

        // 图标
        Image {
            id: icon
            source: opBtn.iconSource
            width: 65
            height: 65
            anchors.horizontalCenter: parent.horizontalCenter
            visible: opBtn.iconSource !== ""
            fillMode: Image.PreserveAspectFit

            // 图标颜色滤镜（活动状态时变蓝色）
            layer.enabled: true
            layer.effect: ColorOverlay {
                color: opBtn.isActive ? "#38BDF8" : "#EEEEEE"
            }
        }

        // 文字
        Text {
            text: opBtn.text
            font.family: "Inter"
            font.pixelSize: 26
            font.weight: Font.Bold
            color: opBtn.isActive ? "#38BDF8" : "#EEEEEE"
            anchors.horizontalCenter: parent.horizontalCenter
            z: 1
        }
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
