import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: statusBar
    color: "#2B2B2B"
    border.color: "#3D3D3D"
    border.width: 1

    // 状态栏内容
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 20

        // 左侧：应用名称和版本
        RowLayout {
            spacing: 10

            Text {
                id: appIcon
                text: "🔬"
                font.pixelSize: 24
                color: "#ECEFF4"

                // 显示应用程序图标
            }

            Text {
                text: "RustSmartScope"
                color: "#ECEFF4"
                font.pixelSize: 16
                font.bold: true
            }

            Text {
                text: "v0.1.0"
                color: "#88C0D0"
                font.pixelSize: 12
            }
        }

        // 中间：弹性空间
        Item {
            Layout.fillWidth: true
        }

        // 中央：系统状态指示器
        RowLayout {
            spacing: 15

            // Rust核心状态
            StatusIndicator {
                id: rustStatus
                label: "Rust核心"
                status: "connected"
                description: "已初始化"
            }

            // 相机状态
            StatusIndicator {
                id: cameraStatus
                label: "相机"
                status: "disconnected"
                description: "未连接"
            }

            // AI推理状态
            StatusIndicator {
                id: aiStatus
                label: "AI推理"
                status: "disabled"
                description: "未启用"
            }
        }

        // 中间：弹性空间
        Item {
            Layout.fillWidth: true
        }

        // 右侧：时间和系统信息
        RowLayout {
            spacing: 15

            // 内存使用
            Text {
                text: "内存: 245MB"
                color: "#D8DEE9"
                font.pixelSize: 12
            }

            // 当前时间
            Text {
                id: timeLabel
                color: "#ECEFF4"
                font.pixelSize: 14
                font.family: "monospace"

                Timer {
                    interval: 1000
                    running: true
                    repeat: true
                    onTriggered: {
                        timeLabel.text = Qt.formatDateTime(new Date(), "hh:mm:ss")
                    }
                }

                Component.onCompleted: {
                    timeLabel.text = Qt.formatDateTime(new Date(), "hh:mm:ss")
                }
            }
        }
    }


    // 鼠标悬停效果
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true

        onEntered: {
            statusBar.color = "#333333"
        }

        onExited: {
            statusBar.color = "#2B2B2B"
        }
    }

    // 公共方法：更新状态
    function updateRustStatus(connected, description) {
        rustStatus.status = connected ? "connected" : "disconnected"
        rustStatus.description = description || ""
    }

    function updateCameraStatus(connected, description) {
        cameraStatus.status = connected ? "connected" : "disconnected"
        cameraStatus.description = description || ""
    }

    function updateAiStatus(enabled, description) {
        aiStatus.status = enabled ? "connected" : "disabled"
        aiStatus.description = description || ""
    }
}