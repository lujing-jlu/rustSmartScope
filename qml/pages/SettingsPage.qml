import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: settingsPage
    objectName: "settings"

    signal backRequested()

    background: Rectangle {
        color: "#1E1E1E"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // 页面标题和返回按钮
        RowLayout {
            Layout.fillWidth: true

            Button {
                text: "← 返回"
                onClicked: settingsPage.backRequested()
            }

            Text {
                text: "⚙️ 系统设置"
                font.pixelSize: 24
                font.bold: true
                color: "#ECEFF4"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 80 } // 占位，保持标题居中
        }

        // 设置内容
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                width: settingsPage.width - 40
                spacing: 20

                // 相机设置
                GroupBox {
                    Layout.fillWidth: true
                    title: "📹 相机设置"
                    font.pixelSize: 16

                    background: Rectangle {
                        color: "#2B2B2B"
                        border.color: "#3D3D3D"
                        border.width: 2
                        radius: 10
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 15

                        // 左相机设置
                        GroupBox {
                            Layout.fillWidth: true
                            title: "左相机"

                            background: Rectangle {
                                color: "#3B4252"
                                border.color: "#4C566A"
                                radius: 5
                            }

                            GridLayout {
                                anchors.fill: parent
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 8

                                Text { text: "设备ID:"; color: "#D8DEE9" }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["/dev/video0", "/dev/video1", "/dev/video2"]
                                }

                                Text { text: "分辨率:"; color: "#D8DEE9" }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["1920x1080", "1280x720", "640x480"]
                                }

                                Text { text: "帧率:"; color: "#D8DEE9" }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["30 FPS", "25 FPS", "15 FPS"]
                                }
                            }
                        }

                        // 右相机设置
                        GroupBox {
                            Layout.fillWidth: true
                            title: "右相机"

                            background: Rectangle {
                                color: "#3B4252"
                                border.color: "#4C566A"
                                radius: 5
                            }

                            GridLayout {
                                anchors.fill: parent
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 8

                                Text { text: "设备ID:"; color: "#D8DEE9" }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["/dev/video1", "/dev/video0", "/dev/video2"]
                                }

                                Text { text: "分辨率:"; color: "#D8DEE9" }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["1920x1080", "1280x720", "640x480"]
                                }

                                Text { text: "帧率:"; color: "#D8DEE9" }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["30 FPS", "25 FPS", "15 FPS"]
                                }
                            }
                        }

                        CheckBox {
                            text: "启用自动对焦"
                            checked: true
                        }

                        CheckBox {
                            text: "启用自动曝光"
                            checked: true
                        }
                    }
                }

                // 立体视觉设置
                GroupBox {
                    Layout.fillWidth: true
                    title: "🎯 立体视觉设置"
                    font.pixelSize: 16

                    background: Rectangle {
                        color: "#2B2B2B"
                        border.color: "#3D3D3D"
                        border.width: 2
                        radius: 10
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 15

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            Text { text: "立体匹配算法:"; color: "#D8DEE9" }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["SGBM", "BM", "神经网络"]
                            }

                            Text { text: "视差范围:"; color: "#D8DEE9" }
                            SpinBox {
                                Layout.fillWidth: true
                                from: 16
                                to: 256
                                value: 64
                                stepSize: 16
                            }

                            Text { text: "块大小:"; color: "#D8DEE9" }
                            SpinBox {
                                Layout.fillWidth: true
                                from: 3
                                to: 21
                                value: 9
                                stepSize: 2
                            }

                            Text { text: "最小视差:"; color: "#D8DEE9" }
                            SpinBox {
                                Layout.fillWidth: true
                                from: 0
                                to: 64
                                value: 0
                            }
                        }

                        CheckBox {
                            text: "启用视差后处理"
                            checked: true
                        }

                        CheckBox {
                            text: "启用WLS滤波"
                            checked: false
                        }
                    }
                }

                // AI设置
                GroupBox {
                    Layout.fillWidth: true
                    title: "🤖 AI设置"
                    font.pixelSize: 16

                    background: Rectangle {
                        color: "#2B2B2B"
                        border.color: "#3D3D3D"
                        border.width: 2
                        radius: 10
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 15

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            Text { text: "检测模型:"; color: "#D8DEE9" }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["YOLOv8", "自定义模型"]
                            }

                            Text { text: "置信度阈值:"; color: "#D8DEE9" }
                            Slider {
                                Layout.fillWidth: true
                                from: 0.1
                                to: 1.0
                                value: 0.5
                                stepSize: 0.1
                            }

                            Text { text: "NMS阈值:"; color: "#D8DEE9" }
                            Slider {
                                Layout.fillWidth: true
                                from: 0.1
                                to: 1.0
                                value: 0.4
                                stepSize: 0.1
                            }
                        }

                        CheckBox {
                            text: "启用GPU加速"
                            checked: true
                        }

                        CheckBox {
                            text: "显示检测框"
                            checked: true
                        }
                    }
                }

                // 界面设置
                GroupBox {
                    Layout.fillWidth: true
                    title: "🎨 界面设置"
                    font.pixelSize: 16

                    background: Rectangle {
                        color: "#2B2B2B"
                        border.color: "#3D3D3D"
                        border.width: 2
                        radius: 10
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 15

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            Text { text: "主题:"; color: "#D8DEE9" }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["深色主题", "浅色主题", "自动"]
                            }

                            Text { text: "语言:"; color: "#D8DEE9" }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["中文", "English", "日本語"]
                            }

                            Text { text: "字体大小:"; color: "#D8DEE9" }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["小", "中", "大"]
                            }
                        }

                        CheckBox {
                            text: "启用动画效果"
                            checked: true
                        }

                        CheckBox {
                            text: "全屏模式"
                            checked: true
                        }
                    }
                }

                // 存储设置
                GroupBox {
                    Layout.fillWidth: true
                    title: "💾 存储设置"
                    font.pixelSize: 16

                    background: Rectangle {
                        color: "#2B2B2B"
                        border.color: "#3D3D3D"
                        border.width: 2
                        radius: 10
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 15

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: 10
                            rowSpacing: 8

                            Text { text: "数据目录:"; color: "#D8DEE9" }
                            TextField {
                                Layout.fillWidth: true
                                text: "/home/data"
                                selectByMouse: true
                            }
                            Button {
                                text: "浏览"
                            }

                            Text { text: "图片格式:"; color: "#D8DEE9" }
                            ComboBox {
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                                model: ["PNG", "JPEG", "TIFF"]
                            }
                        }

                        CheckBox {
                            text: "自动保存截图"
                            checked: false
                        }

                        CheckBox {
                            text: "保存测量数据"
                            checked: true
                        }
                    }
                }

                // 操作按钮
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 20
                    spacing: 15

                    Button {
                        Layout.fillWidth: true
                        text: "💾 保存配置"
                        highlighted: true
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "🔄 重置为默认"
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "📂 导入配置"
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "📤 导出配置"
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        console.log("SettingsPage initialized")
    }
}