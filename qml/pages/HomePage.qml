import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"

Page {
    id: homePage
    objectName: "home"

    // 信号定义
    signal navigateRequested(string pageType)
    signal captureRequested()
    signal aiDetectionToggled(bool enabled)

    // 属性
    property bool aiDetectionEnabled: false
    property bool cameraConnected: false

    background: Rectangle {
        color: "#1E1E1E"
    }

    // 主内容布局
    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // 左侧：相机预览区
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#2B2B2B"
            border.color: "#3D3D3D"
            border.width: 2
            radius: 10

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                // 相机预览标题
                Text {
                    text: "📹 相机预览"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#ECEFF4"
                }

                // 双相机预览区域
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    // 左相机
                    CameraPreview {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        cameraName: "左相机"
                        connected: homePage.cameraConnected
                    }

                    // 右相机
                    CameraPreview {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        cameraName: "右相机"
                        connected: homePage.cameraConnected
                    }
                }

                // 相机控制按钮
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10

                    Button {
                        text: "连接相机"
                        enabled: !homePage.cameraConnected
                        onClicked: {
                            // TODO: 连接相机逻辑
                            homePage.cameraConnected = true
                        }
                    }

                    Button {
                        text: "断开相机"
                        enabled: homePage.cameraConnected
                        onClicked: {
                            // TODO: 断开相机逻辑
                            homePage.cameraConnected = false
                        }
                    }

                    Button {
                        text: "📷 拍照"
                        enabled: homePage.cameraConnected
                        onClicked: {
                            homePage.captureRequested()
                        }
                    }
                }
            }
        }

        // 右侧：功能控制面板
        Rectangle {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: "#2B2B2B"
            border.color: "#3D3D3D"
            border.width: 2
            radius: 10

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                // 功能面板标题
                Text {
                    text: "🎛️ 功能控制"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#ECEFF4"
                }

                // AI检测控制
                GroupBox {
                    Layout.fillWidth: true
                    title: "AI智能检测"
                    font.pixelSize: 14

                    background: Rectangle {
                        color: "#3B4252"
                        border.color: "#4C566A"
                        radius: 5
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        Switch {
                            text: "启用AI检测"
                            checked: homePage.aiDetectionEnabled
                            onToggled: {
                                homePage.aiDetectionEnabled = checked
                                homePage.aiDetectionToggled(checked)
                            }
                        }

                        Text {
                            text: homePage.aiDetectionEnabled ? "✅ AI检测已启用" : "❌ AI检测已禁用"
                            color: homePage.aiDetectionEnabled ? "#A3BE8C" : "#BF616A"
                            font.pixelSize: 12
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: ["YOLOv8", "自定义模型", "深度检测"]
                            enabled: homePage.aiDetectionEnabled
                        }
                    }
                }

                // 测量工具
                GroupBox {
                    Layout.fillWidth: true
                    title: "测量工具"
                    font.pixelSize: 14

                    background: Rectangle {
                        color: "#3B4252"
                        border.color: "#4C566A"
                        radius: 5
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        Button {
                            Layout.fillWidth: true
                            text: "📏 3D测量"
                            onClicked: {
                                homePage.navigateRequested("measurement")
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "📐 角度测量"
                            enabled: false
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "📊 体积计算"
                            enabled: false
                        }
                    }
                }

                // 相机设置
                GroupBox {
                    Layout.fillWidth: true
                    title: "相机设置"
                    font.pixelSize: 14

                    background: Rectangle {
                        color: "#3B4252"
                        border.color: "#4C566A"
                        radius: 5
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        RowLayout {
                            Text {
                                text: "曝光:"
                                color: "#D8DEE9"
                                font.pixelSize: 12
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: 50
                                enabled: homePage.cameraConnected
                            }
                        }

                        RowLayout {
                            Text {
                                text: "亮度:"
                                color: "#D8DEE9"
                                font.pixelSize: 12
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: 50
                                enabled: homePage.cameraConnected
                            }
                        }

                        RowLayout {
                            Text {
                                text: "对比度:"
                                color: "#D8DEE9"
                                font.pixelSize: 12
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: 50
                                enabled: homePage.cameraConnected
                            }
                        }
                    }
                }

                // 弹性空间
                Item {
                    Layout.fillHeight: true
                }

                // 快速操作
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        Layout.fillWidth: true
                        text: "⚙️ 设置"
                        onClicked: {
                            homePage.navigateRequested("settings")
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "🔍 预览"
                        onClicked: {
                            homePage.navigateRequested("preview")
                        }
                    }
                }
            }
        }
    }


    // 公共方法
    function toggleAiDetection() {
        aiDetectionEnabled = !aiDetectionEnabled
        aiDetectionToggled(aiDetectionEnabled)
    }

    function capturePhoto() {
        if (cameraConnected) {
            captureRequested()
        }
    }

    Component.onCompleted: {
        console.log("HomePage initialized")
    }
}