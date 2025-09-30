import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    property string cameraName: ""
    property bool connected: false

    color: "#1E1E1E"
    border.color: connected ? "#A3BE8C" : "#BF616A"
    border.width: 2
    radius: 8

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 5

        Text {
            text: cameraName
            color: "#ECEFF4"
            font.pixelSize: 14
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#2E3440"
            radius: 5

            // 预览内容
            Item {
                anchors.fill: parent

                // 当相机未连接时显示占位符
                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    visible: !connected

                    Text {
                        text: "📷"
                        font.pixelSize: 48
                        color: "#4C566A"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: "相机未连接"
                        color: "#88C0D0"
                        font.pixelSize: 12
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                // 相机连接时显示模拟预览
                Rectangle {
                    anchors.fill: parent
                    visible: connected
                    color: "#4C566A"

                    Text {
                        anchors.centerIn: parent
                        text: "实时预览\n" + cameraName
                        color: "#ECEFF4"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                    }

                    // 模拟视频帧更新
                    SequentialAnimation on color {
                        running: connected
                        loops: Animation.Infinite
                        ColorAnimation { to: "#5E81AC"; duration: 2000 }
                        ColorAnimation { to: "#4C566A"; duration: 2000 }
                    }
                }
            }
        }

        Text {
            text: connected ? "🟢 已连接" : "🔴 未连接"
            color: connected ? "#A3BE8C" : "#BF616A"
            font.pixelSize: 10
            Layout.alignment: Qt.AlignHCenter
        }
    }
}