import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: reportPage
    objectName: "report"

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
                onClicked: reportPage.backRequested()
            }

            Text {
                text: "📊 测量报告"
                font.pixelSize: 24
                font.bold: true
                color: "#ECEFF4"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 80 }
        }

        // 主内容区域
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
                spacing: 20

                Text {
                    text: "📋 报告生成器"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#ECEFF4"
                    Layout.alignment: Qt.AlignHCenter
                }

                // 报告模板
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#FFFFFF"
                    border.color: "#D8DEE9"
                    border.width: 1
                    radius: 5

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 20

                        ColumnLayout {
                            width: parent.width - 40
                            spacing: 20

                            // 报告头部
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "RustSmartScope 测量报告"
                                    font.pixelSize: 24
                                    font.bold: true
                                    color: "#2E3440"
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 2
                                    color: "#5E81AC"
                                }

                                RowLayout {
                                    Layout.fillWidth: true

                                    ColumnLayout {
                                        Text {
                                            text: "项目名称: 样品检测"
                                            color: "#2E3440"
                                            font.pixelSize: 14
                                        }
                                        Text {
                                            text: "操作员: 用户"
                                            color: "#2E3440"
                                            font.pixelSize: 14
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    ColumnLayout {
                                        Text {
                                            text: "日期: " + Qt.formatDate(new Date(), "yyyy-MM-dd")
                                            color: "#2E3440"
                                            font.pixelSize: 14
                                        }
                                        Text {
                                            text: "时间: " + Qt.formatTime(new Date(), "hh:mm:ss")
                                            color: "#2E3440"
                                            font.pixelSize: 14
                                        }
                                    }
                                }
                            }

                            // 测量结果表格
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "测量结果"
                                    font.pixelSize: 18
                                    font.bold: true
                                    color: "#2E3440"
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 200
                                    border.color: "#D8DEE9"
                                    border.width: 1

                                    TableView {
                                        anchors.fill: parent
                                        model: measurementModel

                                        delegate: Rectangle {
                                            border.color: "#D8DEE9"
                                            border.width: 1
                                            color: (row % 2 === 0) ? "#ECEFF4" : "#FFFFFF"

                                            Text {
                                                anchors.centerIn: parent
                                                text: display || ""
                                                color: "#2E3440"
                                                font.pixelSize: 12
                                            }
                                        }
                                    }
                                }
                            }

                            // 统计信息
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "统计信息"
                                    font.pixelSize: 18
                                    font.bold: true
                                    color: "#2E3440"
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 20
                                    rowSpacing: 10

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 100
                                        border.color: "#D8DEE9"
                                        border.width: 1
                                        color: "#F8F9FA"

                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Text {
                                                text: "总测量数"
                                                color: "#4C566A"
                                                font.pixelSize: 12
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                            Text {
                                                text: "15"
                                                color: "#2E3440"
                                                font.pixelSize: 24
                                                font.bold: true
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 100
                                        border.color: "#D8DEE9"
                                        border.width: 1
                                        color: "#F8F9FA"

                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Text {
                                                text: "平均距离"
                                                color: "#4C566A"
                                                font.pixelSize: 12
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                            Text {
                                                text: "12.8 mm"
                                                color: "#2E3440"
                                                font.pixelSize: 20
                                                font.bold: true
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 100
                                        border.color: "#D8DEE9"
                                        border.width: 1
                                        color: "#F8F9FA"

                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Text {
                                                text: "最大值"
                                                color: "#4C566A"
                                                font.pixelSize: 12
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                            Text {
                                                text: "25.6 mm"
                                                color: "#2E3440"
                                                font.pixelSize: 20
                                                font.bold: true
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 100
                                        border.color: "#D8DEE9"
                                        border.width: 1
                                        color: "#F8F9FA"

                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Text {
                                                text: "最小值"
                                                color: "#4C566A"
                                                font.pixelSize: 12
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                            Text {
                                                text: "3.2 mm"
                                                color: "#2E3440"
                                                font.pixelSize: 20
                                                font.bold: true
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                        }
                                    }
                                }
                            }

                            // 备注
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "备注"
                                    font.pixelSize: 18
                                    font.bold: true
                                    color: "#2E3440"
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 100
                                    border.color: "#D8DEE9"
                                    border.width: 1
                                    color: "#FFFFFF"

                                    Text {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 10
                                        text: "本次测量使用双目立体视觉技术进行3D重建，\n测量精度达到0.1mm级别。\n所有数据已通过质量检查。"
                                        color: "#2E3440"
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }

                // 底部操作按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 15

                    Button {
                        Layout.fillWidth: true
                        text: "🖨️ 打印报告"
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "💾 保存为PDF"
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "📧 发送邮件"
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "📤 导出数据"
                    }
                }
            }
        }
    }

    // 测量数据模型
    ListModel {
        id: measurementModel

        ListElement {
            序号: "1"
            类型: "距离"
            数值: "15.2"
            单位: "mm"
            时间: "14:30:15"
        }
        ListElement {
            序号: "2"
            类型: "角度"
            数值: "45.6"
            单位: "°"
            时间: "14:30:18"
        }
        ListElement {
            序号: "3"
            类型: "面积"
            数值: "23.4"
            单位: "mm²"
            时间: "14:30:22"
        }
        ListElement {
            序号: "4"
            类型: "距离"
            数值: "8.7"
            单位: "mm"
            时间: "14:30:25"
        }
        ListElement {
            序号: "5"
            类型: "距离"
            数值: "12.1"
            单位: "mm"
            时间: "14:30:28"
        }
    }

    Component.onCompleted: {
        console.log("ReportPage initialized")
    }
}