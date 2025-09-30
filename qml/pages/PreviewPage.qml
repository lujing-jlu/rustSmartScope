import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: previewPage
    objectName: "preview"

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
                onClicked: previewPage.backRequested()
            }

            Text {
                text: "🔍 图像预览"
                font.pixelSize: 24
                font.bold: true
                color: "#ECEFF4"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 80 }
        }

        // 主内容区域
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            // 左侧：文件浏览器
            Rectangle {
                Layout.preferredWidth: 250
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
                        text: "📁 文件浏览"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#ECEFF4"
                    }

                    ComboBox {
                        Layout.fillWidth: true
                        model: ["图片", "视频", "点云数据", "测量结果"]
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: fileModel

                        delegate: Rectangle {
                            width: parent.width
                            height: 50
                            color: mouseArea.containsMouse ? "#4C566A" : "transparent"
                            radius: 5

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Text {
                                    text: model.icon
                                    font.pixelSize: 16
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: model.name
                                        color: "#ECEFF4"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: model.date
                                        color: "#88C0D0"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    // 选择文件进行预览
                                    console.log("Selected file:", model.name)
                                }
                            }
                        }
                    }
                }
            }

            // 中间：预览区域
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

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "🖼️ 预览区域"
                            font.pixelSize: 16
                            font.bold: true
                            color: "#ECEFF4"
                        }

                        Item { Layout.fillWidth: true }

                        RowLayout {
                            spacing: 5

                            Button {
                                text: "🔍+"
                                implicitWidth: 40
                                implicitHeight: 30
                            }

                            Button {
                                text: "🔍-"
                                implicitWidth: 40
                                implicitHeight: 30
                            }

                            Button {
                                text: "↻"
                                implicitWidth: 40
                                implicitHeight: 30
                            }

                            Button {
                                text: "⊡"
                                implicitWidth: 40
                                implicitHeight: 30
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Rectangle {
                            width: Math.max(600, previewPage.width * 0.4)
                            height: Math.max(400, previewPage.height * 0.4)
                            color: "#1E1E1E"
                            border.color: "#4C566A"
                            radius: 5

                            // 预览内容占位符
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 20

                                Text {
                                    text: "🖼️"
                                    font.pixelSize: 64
                                    color: "#4C566A"
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Text {
                                    text: "选择文件进行预览"
                                    color: "#88C0D0"
                                    font.pixelSize: 16
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Text {
                                    text: "支持格式: PNG, JPEG, TIFF, MP4, AVI, PLY, PCD"
                                    color: "#4C566A"
                                    font.pixelSize: 12
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }
                }
            }

            // 右侧：信息面板
            Rectangle {
                Layout.preferredWidth: 250
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
                        text: "ℹ️ 文件信息"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#ECEFF4"
                    }

                    // 文件属性
                    GroupBox {
                        Layout.fillWidth: true
                        title: "属性"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            InfoRow { label: "文件名:"; value: "image_001.png" }
                            InfoRow { label: "大小:"; value: "2.4 MB" }
                            InfoRow { label: "分辨率:"; value: "1920x1080" }
                            InfoRow { label: "格式:"; value: "PNG" }
                            InfoRow { label: "创建时间:"; value: "2023-12-01 14:30" }
                        }
                    }

                    // 相机信息
                    GroupBox {
                        Layout.fillWidth: true
                        title: "相机信息"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            InfoRow { label: "曝光:"; value: "1/60s" }
                            InfoRow { label: "ISO:"; value: "100" }
                            InfoRow { label: "焦距:"; value: "35mm" }
                            InfoRow { label: "光圈:"; value: "f/2.8" }
                        }
                    }

                    // 测量信息
                    GroupBox {
                        Layout.fillWidth: true
                        title: "测量信息"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            InfoRow { label: "测量点:"; value: "5 个" }
                            InfoRow { label: "距离:"; value: "15.2 mm" }
                            InfoRow { label: "角度:"; value: "45.6°" }
                            InfoRow { label: "面积:"; value: "23.4 mm²" }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    // 操作按钮
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            Layout.fillWidth: true
                            text: "📂 打开文件夹"
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "🔄 刷新"
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "🗑️ 删除文件"
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "📤 导出"
                        }
                    }
                }
            }
        }
    }

    // 信息行组件
    component InfoRow: RowLayout {
        property string label: ""
        property string value: ""

        Text {
            text: label
            color: "#D8DEE9"
            font.pixelSize: 11
            Layout.preferredWidth: 80
        }

        Text {
            text: value
            color: "#88C0D0"
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }

    // 文件模型
    ListModel {
        id: fileModel

        Component.onCompleted: {
            append({name: "stereo_001.png", icon: "🖼️", date: "2023-12-01 14:30"})
            append({name: "stereo_002.png", icon: "🖼️", date: "2023-12-01 14:31"})
            append({name: "recording_001.mp4", icon: "🎬", date: "2023-12-01 14:32"})
            append({name: "pointcloud_001.ply", icon: "🎯", date: "2023-12-01 14:33"})
            append({name: "measurement_001.json", icon: "📏", date: "2023-12-01 14:34"})
        }
    }

    Component.onCompleted: {
        console.log("PreviewPage initialized")
    }
}