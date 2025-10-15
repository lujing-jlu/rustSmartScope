import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "components"
import RustSmartScope.Logger 1.0

ApplicationWindow {
    id: settingsWindow
    visible: false
    title: "SmartScope Industrial - 系统设置"

    // 与主窗口一致的窗口属性
    property int targetWidth: 1920
    property int targetHeight: 1200

    width: Screen.width || targetWidth
    height: Screen.height || targetHeight
    flags: Qt.FramelessWindowHint | Qt.Window
    x: 0; y: 0

    // 颜色系统（与主窗口保持一致）
    property color backgroundColor: "#0A0A0F"
    property color backgroundSecondary: "#121218"
    property color textPrimary: "#FFFFFF"
    property color borderPrimary: "#334155"

    // 由主窗口传入，确保与主窗口完全一致
    property int statusBarHeight: 0
    property int margins: 12

    // 与主窗口一致的状态栏属性（由主窗口绑定传入）
    property string currentTime: ""
    property string currentDate: ""
    property int batteryLevel: 85
    property bool isCharging: false
    property int fontSize: 18
    property string mixedFontMedium: ""

    Rectangle { anchors.fill: parent; color: backgroundColor }

    // 状态栏（与主窗口同一组件）
    StatusBar {
        id: statusBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: statusBarHeight
        z: 100

        currentTime: settingsWindow.currentTime
        currentDate: settingsWindow.currentDate
        batteryLevel: settingsWindow.batteryLevel
        isCharging: settingsWindow.isCharging
        fontSize: settingsWindow.fontSize * 1.5
        mixedFontMedium: settingsWindow.mixedFontMedium
    }

    // 主内容（嵌入原设置页面内容）
    Item {
        anchors.top: statusBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: navigationContainer.top
        anchors.margins: margins

        // 直接加载原设置页内容，复用已有布局与交互
        Loader {
            anchors.fill: parent
            source: "pages/SettingsPage.qml"
        }
    }

    // 底部关闭栏（简单关闭按钮）
    Item {
        id: navigationContainer
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 96
        z: 95

        Rectangle {
            anchors.fill: parent
            color: backgroundSecondary
            border.color: borderPrimary
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: margins
                spacing: margins

                Rectangle {
                    Layout.preferredWidth: 160
                    Layout.fillHeight: true
                    color: Qt.rgba(0.7, 0.2, 0.2, 0.8)
                    radius: 12
                    border.width: 1
                    border.color: Qt.rgba(1,1,1,0.1)

                    MouseArea {
                        anchors.fill: parent
                        onClicked: settingsWindow.close()
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        Image { source: "qrc:/icons/close.svg"; width: 28; height: 28; fillMode: Image.PreserveAspectFit }
                        Text { text: "关闭"; color: "#FFFFFF"; font.pixelSize: 22; font.weight: Font.Medium }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }
    }
}
