import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"
import RustSmartScope.Logger 1.0

Rectangle {
    id: settingsPage
    color: "transparent"

    signal backRequested()
    signal navigateRequested(string pageType)

    // 主布局
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 30

        // 页面标题
        Text {
            text: "⚙️ 系统设置"
            font.pixelSize: 48
            font.bold: true
            color: "#F59E0B"  // 橙色
            Layout.alignment: Qt.AlignHCenter
        }

        // 设置内容区域（滚动视图）
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 30

                // 相机设置区域
                GroupBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: cameraSettingsContent.height + 100

                    background: Rectangle {
                        color: "rgba(20, 20, 20, 200)"
                        border.color: "#3B82F6"
                        border.width: 2
                        radius: 10
                    }

                    label: Text {
                        text: "📷 相机参数设置"
                        font.pixelSize: 32
                        font.bold: true
                        color: "#3B82F6"
                        padding: 10
                    }

                    ColumnLayout {
                        id: cameraSettingsContent
                        width: parent.width
                        spacing: 20

                        // 相机状态显示
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 20

                            Text {
                                text: "相机模式:"
                                font.pixelSize: 24
                                color: "white"
                            }

                            Text {
                                text: {
                                    var mode = CameraManager.cameraMode
                                    if (mode === 0) return "无相机"
                                    else if (mode === 1) return "单相机"
                                    else if (mode === 2) return "双目相机"
                                    else return "未知"
                                }
                                font.pixelSize: 24
                                font.bold: true
                                color: {
                                    var mode = CameraManager.cameraMode
                                    if (mode === 0) return "#EF4444"  // 红色
                                    else if (mode === 1) return "#F59E0B"  // 橙色
                                    else if (mode === 2) return "#10B981"  // 绿色
                                    else return "#6B7280"  // 灰色
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: CameraManager.cameraRunning ? "运行中 🟢" : "已停止 🔴"
                                font.pixelSize: 24
                                color: CameraManager.cameraRunning ? "#10B981" : "#EF4444"
                            }
                        }

                        // 相机参数面板
                        CameraParameterPanel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 800

                            cameraMode: CameraManager.cameraMode

                            onApplySettings: {
                                Logger.info("应用相机参数设置")
                            }

                            onResetToDefaults: {
                                Logger.info("重置相机参数到默认值")
                                // 调用FFI重置
                                if (cameraMode === 1) {
                                    CameraParameterManager.resetSingleCameraParameters()
                                } else if (cameraMode === 2) {
                                    CameraParameterManager.resetLeftCameraParameters()
                                    CameraParameterManager.resetRightCameraParameters()
                                }
                            }
                        }
                    }
                }

                // 其他设置区域（预留）
                GroupBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200

                    background: Rectangle {
                        color: "rgba(20, 20, 20, 200)"
                        border.color: "#8B5CF6"
                        border.width: 2
                        radius: 10
                    }

                    label: Text {
                        text: "🔧 系统设置"
                        font.pixelSize: 32
                        font.bold: true
                        color: "#8B5CF6"
                        padding: 10
                    }

                    ColumnLayout {
                        width: parent.width
                        spacing: 15

                        Text {
                            text: "其他系统设置（待实现）"
                            font.pixelSize: 20
                            color: "#9CA3AF"
                        }
                    }
                }

                // 底部留白
                Item {
                    Layout.fillHeight: true
                    Layout.minimumHeight: 50
                }
            }
        }
    }

    Component.onCompleted: {
        Logger.info("SettingsPage initialized")
    }
}