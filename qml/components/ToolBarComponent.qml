import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: toolBar
    width: 80
    height: 400
    color: "#2B2B2B"
    border.color: "#3D3D3D"
    border.width: 1
    radius: 40

    // 工具按钮数据
    property var toolItems: [
        {
            id: "capture",
            icon: "qrc:/icons/camera.svg",
            label: "拍照",
            shortcut: "F9",
            enabled: true
        },
        {
            id: "record",
            icon: "qrc:/icons/record_start.svg",
            label: "录制",
            shortcut: "F10",
            enabled: true
        },
        {
            id: "ai_detection",
            icon: "qrc:/icons/AI.svg",
            label: "AI检测",
            shortcut: "F4",
            enabled: true,
            toggle: true
        },
        {
            id: "led_control",
            icon: "qrc:/icons/brightness.svg",
            label: "灯光",
            shortcut: "F12",
            enabled: true
        },
        {
            id: "zoom_in",
            icon: "qrc:/icons/zoom_in.svg",
            label: "放大",
            shortcut: "+",
            enabled: true
        },
        {
            id: "zoom_out",
            icon: "qrc:/icons/zoom_out.svg",
            label: "缩小",
            shortcut: "-",
            enabled: true
        }
    ]

    // 信号定义
    signal toolRequested(string toolId)
    signal captureRequested()
    signal recordToggled(bool recording)
    signal aiDetectionToggled(bool enabled)
    signal ledControlRequested()

    // 工具按钮布局
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        Repeater {
            model: toolBar.toolItems

            delegate: ToolButton {
                id: toolButton

                Component.onCompleted: {
                    iconText = modelData.icon
                    label = modelData.label
                    shortcut = modelData.shortcut
                    enabled = modelData.enabled
                    isToggle = modelData.toggle || false
                    toolId = modelData.id
                }

                onClicked: {
                    toolBar.toolRequested(modelData.id)

                    // 特定工具的信号处理
                    switch(modelData.id) {
                        case "capture":
                            toolBar.captureRequested()
                            break
                        case "record":
                            isToggled = !isToggled
                            toolBar.recordToggled(isToggled)
                            break
                        case "ai_detection":
                            isToggled = !isToggled
                            toolBar.aiDetectionToggled(isToggled)
                            break
                        case "led_control":
                            toolBar.ledControlRequested()
                            break
                    }
                }
            }
        }

        // 分隔线
        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 1
            color: "#4C566A"
            Layout.alignment: Qt.AlignHCenter
        }

        // 额外控制按钮
        ToolButton {
            Component.onCompleted: {
                iconText = "qrc:/icons/back.svg"
                label = "返回"
                shortcut = "F7"
                toolId = "back"
            }
            onClicked: {
                toolBar.toolRequested("back")
            }
        }
    }

    // 公共方法
    function setToolEnabled(toolId, enabled) {
        for (var i = 0; i < toolItems.length; i++) {
            if (toolItems[i].id === toolId) {
                toolItems[i].enabled = enabled
                break
            }
        }
    }

    function setToolToggled(toolId, toggled) {
        for (var i = 0; i < toolItems.length; i++) {
            if (toolItems[i].id === toolId && toolItems[i].toggle) {
                // 更新状态逻辑
                break
            }
        }
    }

    function highlightTool(toolId) {
        // 临时高亮工具按钮
        for (var i = 0; i < toolItems.length; i++) {
            if (toolItems[i].id === toolId) {
                // 高亮逻辑
                break
            }
        }
    }

    // 鼠标悬停效果
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        z: -1

        onEntered: {
            toolBar.color = "#333333"
        }

        onExited: {
            toolBar.color = "#2B2B2B"
        }
    }
}