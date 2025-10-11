import QtQuick 2.15
import QtQuick.Controls 2.15

// 自定义滑块组件 - 确保数值更新能够正确反映
Rectangle {
    id: root

    // 公共属性
    property real from: 0
    property real to: 100
    property real value: 0
    property real stepSize: 1
    property bool enabled: true
    property alias pressed: mouseArea.pressed

    // 信号
    signal sliderValueChanged(real newValue)

    // 外观
    implicitWidth: 200
    implicitHeight: 40

    color: "transparent"

    // 背景轨道
    Rectangle {
        id: track
        anchors.centerIn: parent
        width: parent.width - 20
        height: 8
        radius: 4
        color: "#555555"

        // 进度条
        Rectangle {
            id: progress
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: (root.value - root.from) / (root.to - root.from) * parent.width
            radius: 4
            color: enabled ? "#FFC107" : "#666666"
        }
    }

    // 滑块手柄
    Rectangle {
        id: handle
        width: 24
        height: 24
        radius: 12
        color: enabled ? (pressed ? "#666666" : "#888888") : "#444444"
        border.color: "#000000"
        border.width: 2

        x: 10 + (root.value - root.from) / (root.to - root.from) * (track.width - width)
        y: parent.height / 2 - height / 2

        Behavior on x {
            enabled: !pressed
            NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
        }
    }

    // 鼠标交互区域
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled

        drag.target: handle
        drag.axis: Drag.XAxis
        drag.minimumX: 10 - handle.width/2
        drag.maximumX: 10 + track.width - handle.width/2

        onPositionChanged: {
            if (pressed) {
                updateValue(mouseX)
            }
        }

        onPressed: {
            updateValue(mouseX)
        }

        function updateValue(mouseX) {
            var relativeX = Math.max(0, Math.min(track.width, mouseX - 10))
            var newValue = root.from + (relativeX / track.width) * (root.to - root.from)

            // 应用步长
            if (stepSize > 0) {
                newValue = Math.round(newValue / stepSize) * stepSize
            }

            // 限制在范围内
            newValue = Math.max(root.from, Math.min(root.to, newValue))

            if (Math.abs(root.value - newValue) >= stepSize * 0.5) {
                root.value = newValue
                root.sliderValueChanged(newValue)
            }
        }
    }

    // 键盘控制
    Keys.onLeftPressed: {
        var newValue = Math.max(root.from, root.value - root.stepSize)
        root.value = newValue
        root.sliderValueChanged(newValue)
    }

    Keys.onRightPressed: {
        var newValue = Math.min(root.to, root.value + root.stepSize)
        root.value = newValue
        root.sliderValueChanged(newValue)
    }

    // 确保组件可以接收键盘焦点
    focus: enabled

    // 调试输出
    onValueChanged: {
        console.log("CustomSlider onValueChanged:", value)
    }
}