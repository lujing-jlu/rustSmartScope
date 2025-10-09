import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Rectangle {
    id: statusBar

    // 公共属性 - 从主窗口传入
    property string currentTime: ""
    property string currentDate: ""
    property int batteryLevel: 85
    property bool isCharging: false

    // 样式属性 - 从主窗口传入
    property int fontSize: 18
    property string mixedFontMedium: ""
    property color accentSuccess: "#10B981"
    property color accentWarning: "#F59E0B"
    property color accentError: "#EF4444"

    // 状态栏样式
    color: Qt.rgba(0.1, 0.1, 0.15, 0.8)
    radius: 0
    z: 100

    // 边框效果
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.1)
        radius: parent.radius
    }

    // 底部阴影
    Rectangle {
        anchors.top: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.2) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 0

        // 左侧：Logo区域
        Rectangle {
            Layout.preferredWidth: fontSize * 6
            Layout.fillHeight: true
            color: "transparent"

            Image {
                id: appIcon
                source: "qrc:/icons/EDDYSUN-logo.png"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height * 0.48  // 从0.6缩小到0.48 (减少20%)
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
            }

        }

        // 中间：时间区域
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            // color: "red"  // 调试用，可以取消注释查看区域

            Text {
                text: currentTime || "00:00"  // 如果currentTime为空，显示默认时间
                color: "#FFFFFF"
                font.pixelSize: fontSize * 1.2
                font.weight: Font.Bold
                anchors.centerIn: parent
                visible: true
            }
        }

        // 右侧：电池区域
        Rectangle {
            Layout.preferredWidth: fontSize * 6
            Layout.fillHeight: true
            color: "transparent"

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: fontSize * 0.3

                // 电池图标
                Rectangle {
                    width: fontSize * 2.0
                    height: fontSize * 1.1
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    color: "transparent"
                    border.color: "#FFFFFF"
                    border.width: 1.5

                    // 电池电量填充
                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 2
                        width: (parent.width - 4) * (batteryLevel / 100.0)
                        height: parent.height - 4
                        radius: 1
                        color: {
                            if (batteryLevel > 50) return accentSuccess
                            else if (batteryLevel > 20) return accentWarning
                            else return accentError
                        }
                    }

                    // 电池正极
                    Rectangle {
                        anchors.left: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: parent.height * 0.6
                        radius: 1
                        color: "#FFFFFF"
                    }
                }

                // 电量百分比
                Text {
                    text: batteryLevel + "%"
                    color: "#FFFFFF"
                    font.pixelSize: fontSize * 1.2
                    font.weight: Font.Medium
                    font.family: mixedFontMedium
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}