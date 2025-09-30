import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: mainWindow
    visible: true
    title: "RustSmartScope"
    width: 1200
    height: 800

    // 主布局
    Rectangle {
        anchors.fill: parent
        color: "#1E1E1E"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // 顶部状态栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#2B2B2B"
                border.color: "#3D3D3D"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 15

                    Text {
                        text: "🦀 RustSmartScope"
                        color: "#ECEFF4"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "Rust核心: ✅"
                        color: "#A3BE8C"
                        font.pixelSize: 12
                    }

                    Text {
                        text: "相机: ❌"
                        color: "#BF616A"
                        font.pixelSize: 12
                    }

                    Text {
                        id: timeLabel
                        color: "#ECEFF4"
                        font.pixelSize: 14

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

            // 主内容区域
            StackView {
                id: pageStack
                Layout.fillWidth: true
                Layout.fillHeight: true

                initialItem: Rectangle {
                    color: "#1E1E1E"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 20

                        // 左侧相机预览
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

                                Text {
                                    text: "📹 相机预览"
                                    color: "#ECEFF4"
                                    font.pixelSize: 18
                                    font.bold: true
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: 10

                                    // 左相机
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        color: "#1E1E1E"
                                        border.color: "#BF616A"
                                        border.width: 2
                                        radius: 8

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 10

                                            Text {
                                                text: "📷"
                                                font.pixelSize: 48
                                                color: "#4C566A"
                                                anchors.horizontalCenter: parent.horizontalCenter
                                            }

                                            Text {
                                                text: "左相机未连接"
                                                color: "#88C0D0"
                                                font.pixelSize: 14
                                                anchors.horizontalCenter: parent.horizontalCenter
                                            }
                                        }
                                    }

                                    // 右相机
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        color: "#1E1E1E"
                                        border.color: "#BF616A"
                                        border.width: 2
                                        radius: 8

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 10

                                            Text {
                                                text: "📷"
                                                font.pixelSize: 48
                                                color: "#4C566A"
                                                anchors.horizontalCenter: parent.horizontalCenter
                                            }

                                            Text {
                                                text: "右相机未连接"
                                                color: "#88C0D0"
                                                font.pixelSize: 14
                                                anchors.horizontalCenter: parent.horizontalCenter
                                            }
                                        }
                                    }
                                }

                                // 控制按钮
                                RowLayout {
                                    Layout.alignment: Qt.AlignHCenter
                                    spacing: 10

                                    Button {
                                        text: "连接相机"
                                        onClicked: console.log("连接相机")
                                    }

                                    Button {
                                        text: "📷 拍照"
                                        onClicked: console.log("拍照")
                                    }

                                    Button {
                                        text: "🎬 录制"
                                        onClicked: console.log("录制")
                                    }
                                }
                            }
                        }

                        // 右侧控制面板
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

                                Text {
                                    text: "🎛️ 功能控制"
                                    color: "#ECEFF4"
                                    font.pixelSize: 18
                                    font.bold: true
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 100
                                    color: "#3B4252"
                                    radius: 5

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10

                                        Text {
                                            text: "AI检测"
                                            color: "#ECEFF4"
                                            font.bold: true
                                        }

                                        Switch {
                                            text: "启用AI检测"
                                            checked: false
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 100
                                    color: "#3B4252"
                                    radius: 5

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10

                                        Text {
                                            text: "测量工具"
                                            color: "#ECEFF4"
                                            font.bold: true
                                        }

                                        Button {
                                            Layout.fillWidth: true
                                            text: "📏 3D测量"
                                            onClicked: console.log("3D测量")
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Button {
                                        Layout.fillWidth: true
                                        text: "⚙️ 设置"
                                        onClicked: console.log("设置")
                                    }

                                    Button {
                                        Layout.fillWidth: true
                                        text: "🔍 预览"
                                        onClicked: console.log("预览")
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 底部导航栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: "#2B2B2B"
                border.color: "#3D3D3D"
                border.width: 1

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 20

                    Repeater {
                        model: [
                            {icon: "🏠", text: "主页"},
                            {icon: "📏", text: "测量"},
                            {icon: "🔍", text: "预览"},
                            {icon: "⚙️", text: "设置"},
                            {icon: "📊", text: "报告"},
                            {icon: "🐛", text: "调试"}
                        ]

                        delegate: Button {
                            implicitWidth: 80
                            implicitHeight: 50

                            background: Rectangle {
                                color: parent.pressed ? "#5E81AC" : (parent.hovered ? "#4C566A" : "transparent")
                                radius: 25
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 2

                                Text {
                                    text: modelData.icon
                                    font.pixelSize: 20
                                    color: "#ECEFF4"
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }

                                Text {
                                    text: modelData.text
                                    font.pixelSize: 10
                                    color: "#88C0D0"
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }

                            onClicked: {
                                console.log("切换到:", modelData.text)
                            }
                        }
                    }
                }
            }
        }
    }

    // 键盘快捷键
    focus: true
    Keys.onPressed: function(event) {
        switch(event.key) {
            case Qt.Key_F9:
                console.log("F9 - 拍照")
                event.accepted = true
                break
            case Qt.Key_F2:
                console.log("F2 - 设置")
                event.accepted = true
                break
            case Qt.Key_F3:
                console.log("F3 - 预览")
                event.accepted = true
                break
            case Qt.Key_Escape:
                Qt.quit()
                event.accepted = true
                break
        }
    }

    Component.onCompleted: {
        console.log("RustSmartScope 界面加载完成")
    }
}