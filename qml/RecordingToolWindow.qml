import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import "components"

// 全局置顶的右下角录制工具栏（独立无边框半透明窗口）
Window {
    id: recordTool
    visible: true
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.NonModal

    // 尺寸与位置
    property int panelWidth: 100
    property int panelHeight: 100
    // 底部预留比例（从下向上预留20%的屏幕高度）
    property real bottomReserveRatio: 0.20
    // 右侧边距（与右上工具栏对齐，可由父级传入）
    property int rightMargin: 24
    width: panelWidth
    height: panelHeight
    // 右侧预留边距，与上方工具栏对齐
    x: (Screen.width || 1920) - width - rightMargin
    // 从底部向上按比例预留空间
    y: (Screen.height || 1200) - height - Math.round((Screen.height || 1200) * bottomReserveRatio)

    // 录制状态
    property bool recording: false
    signal recordingToggled(bool recording)

    // 内容（按钮 + 停止收尾时的指示）
    Item {
        anchors.fill: parent
        anchors.margins: 0

        // 单个按钮，风格与现有工具栏一致
        UniversalButton {
            id: recordButton
            anchors.centerIn: parent
            buttonStyle: "toolbar"
            text: ""
            // 使用自定义绘制外观，取消默认图标
            iconSource: ""
            // 取消蓝色高亮，使用白色作为激活色（影响边框/阴影）
            activeColor: "#FFFFFF"
            isActive: recording
            enabled: true
            // 控制图标区域尺寸
            customIconSize: Math.min(recordTool.panelHeight * 0.6, 64)
            onClicked: {
                recordTool.recording = !recordTool.recording
                recordTool.recordingToggled(recordTool.recording)
            }

            // 自定义录制外观：白色圆环 + 红色圆点（录制中）或白色三角形（未录制）
            Item {
                anchors.centerIn: parent
                width: recordButton.customIconSize
                height: recordButton.customIconSize

                // 白色圆环
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    radius: width / 2
                    color: "transparent"
                    border.color: "#FFFFFF"
                    border.width: Math.max(3, Math.round(width * 0.08))
                }

                // 红色中心点（录制中显示）
                Rectangle {
                    anchors.centerIn: parent
                    visible: recordTool.recording
                    width: Math.round(parent.width * 0.38)
                    height: width
                    radius: width / 2
                    color: "#DC2626"   // 红色
                }

                // 未录制时的三角形（白色，指向右侧）
                Canvas {
                    id: idleTriangle
                    anchors.centerIn: parent
                    width: Math.round(parent.width * 0.46)
                    height: width
                    visible: !recordTool.recording
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset && ctx.reset()
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = "#FFFFFF"
                        ctx.beginPath()
                        ctx.moveTo(width * 0.28, height * 0.2)
                        ctx.lineTo(width * 0.28, height * 0.8)
                        ctx.lineTo(width * 0.80, height * 0.5)
                        ctx.closePath()
                        ctx.fill()
                    }
                    onVisibleChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                }
            }
        }

        // BusyIndicator已移除(C++端没有finalizing属性)
        // 硬件编码菜单已移除：固定软件编码 720p
    }
}
