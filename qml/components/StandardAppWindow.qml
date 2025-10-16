import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "../components"

// 标准独立窗口模板：顶部状态栏 + 内容区 + 底部关闭栏
ApplicationWindow {
    id: stdWin
    visible: false
    title: windowTitle
    flags: Qt.FramelessWindowHint | Qt.Window
    x: 0; y: 0
    width: Screen.width || 1920
    height: Screen.height || 1200

    // 传入属性
    property string windowTitle: ""
    property url contentSource: ""

    // 颜色与尺寸（由宿主传入，保持与主窗口风格一致）
    property color backgroundColor: "#0A0A0F"
    property color backgroundSecondary: "#121218"
    property color textPrimary: "#FFFFFF"
    property color borderPrimary: "#334155"
    property int statusBarHeight: 80
    property int margins: 12
    // 状态栏显示信息
    property string currentTime: ""
    property string currentDate: ""
    property int batteryLevel: 100
    property bool isCharging: false
    property int fontSize: 18
    property string mixedFontMedium: ""

    Rectangle { anchors.fill: parent; color: backgroundColor }

    // 顶部状态栏
    StatusBar {
        id: statusBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: statusBarHeight
        z: 100

        currentTime: stdWin.currentTime
        currentDate: stdWin.currentDate
        batteryLevel: stdWin.batteryLevel
        isCharging: stdWin.isCharging
        fontSize: stdWin.fontSize * 1.5
        mixedFontMedium: stdWin.mixedFontMedium
    }

    // 主内容
    Loader {
        id: contentLoader
        anchors.top: statusBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomBar.top
        anchors.margins: margins
        source: contentSource
    }

    // 底部关闭栏
    Rectangle {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 96
        color: backgroundSecondary
        border.color: borderPrimary
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: margins
            spacing: margins

            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: Qt.rgba(0.7, 0.2, 0.2, 0.85)
                radius: 14
                border.width: 1
                border.color: Qt.rgba(1,1,1,0.12)

                MouseArea { anchors.fill: parent; onClicked: stdWin.close() }

                Text {
                    text: "关闭"
                    color: "#FFFFFF"
                    font.pixelSize: Math.max(28, stdWin.fontSize * 1.6)
                    anchors.centerIn: parent
                }
            }

            Item { Layout.fillWidth: true }
        }
    }
}

