import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: measurementPage
    objectName: "measurement"

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
                onClicked: measurementPage.backRequested()
            }

            Text {
                text: "📏 3D测量工具"
                font.pixelSize: 24
                font.bold: true
                color: "#ECEFF4"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 80 } // 占位，保持标题居中
        }

        // 主内容区域
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            // 左侧：3D视图
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#2B2B2B"
                border.color: "#3D3D3D"
                border.width: 2
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    Text {
                        text: "🎯 3D点云视图"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#ECEFF4"
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#1E1E1E"
                        border.color: "#4C566A"
                        border.width: 1
                        radius: 5

                        // 3D视图占位符
                        Item {
                            anchors.fill: parent

                            Text {
                                anchors.centerIn: parent
                                text: "3D点云渲染区域\n\n🔲 点击添加测量点\n📏 拖拽进行测量\n🎯 双击删除测量点"
                                color: "#88C0D0"
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                            }

                            // 模拟3D网格
                            Grid {
                                anchors.centerIn: parent
                                columns: 10
                                rows: 10
                                spacing: 20

                                Repeater {
                                    model: 100
                                    Rectangle {
                                        width: 2
                                        height: 2
                                        color: "#4C566A"
                                        opacity: 0.3
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 右侧：测量工具面板
            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                color: "#2B2B2B"
                border.color: "#3D3D3D"
                border.width: 2
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 15

                    Text {
                        text: "🛠️ 测量工具"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#ECEFF4"
                    }

                    // 测量模式选择
                    GroupBox {
                        Layout.fillWidth: true
                        title: "测量模式"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            RadioButton {
                                text: "📏 线性距离"
                                checked: true
                            }
                            RadioButton {
                                text: "📐 角度测量"
                            }
                            RadioButton {
                                text: "📊 面积计算"
                            }
                            RadioButton {
                                text: "🎯 体积计算"
                            }
                        }
                    }

                    // 测量结果
                    GroupBox {
                        Layout.fillWidth: true
                        title: "测量结果"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            ListView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 150
                                model: measurementModel

                                delegate: Rectangle {
                                    width: parent.width
                                    height: 40
                                    color: "#4C566A"
                                    radius: 3
                                    border.color: "#5E81AC"
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8

                                        Text {
                                            text: model.name
                                            color: "#ECEFF4"
                                            font.pixelSize: 12
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: model.value
                                            color: "#88C0D0"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }

                                        Button {
                                            text: "🗑️"
                                            implicitWidth: 30
                                            implicitHeight: 25
                                            onClicked: {
                                                measurementModel.remove(index)
                                            }
                                        }
                                    }
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "清除所有测量"
                                onClicked: {
                                    measurementModel.clear()
                                }
                            }
                        }
                    }

                    // 精度设置
                    GroupBox {
                        Layout.fillWidth: true
                        title: "精度设置"

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
                                    text: "单位:"
                                    color: "#D8DEE9"
                                    font.pixelSize: 12
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["毫米 (mm)", "厘米 (cm)", "米 (m)"]
                                }
                            }

                            RowLayout {
                                Text {
                                    text: "精度:"
                                    color: "#D8DEE9"
                                    font.pixelSize: 12
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["0.1", "0.01", "0.001"]
                                }
                            }

                            CheckBox {
                                text: "显示实时测量"
                                checked: true
                            }

                            CheckBox {
                                text: "自动保存结果"
                                checked: false
                            }
                        }
                    }

                    // 弹性空间
                    Item {
                        Layout.fillHeight: true
                    }

                    // 操作按钮
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            Layout.fillWidth: true
                            text: "📊 生成报告"
                            enabled: measurementModel.count > 0
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "💾 保存测量数据"
                            enabled: measurementModel.count > 0
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "📁 加载测量数据"
                        }
                    }
                }
            }
        }
    }

    // 测量数据模型
    ListModel {
        id: measurementModel

        Component.onCompleted: {
            // 添加示例数据
            append({name: "距离-1", value: "15.2 mm"})
            append({name: "角度-1", value: "45.6°"})
            append({name: "面积-1", value: "23.4 mm²"})
        }
    }

    Component.onCompleted: {
        console.log("MeasurementPage initialized")
    }
}