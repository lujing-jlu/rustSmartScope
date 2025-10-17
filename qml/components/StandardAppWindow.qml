import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
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

    // 底部关闭栏（参考3D测量页样式）
    Rectangle {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        // 整体高度适度提升（原96的120%）
        height: Math.round(96 * 1.2)
        color: backgroundSecondary
        border.color: borderPrimary
        border.width: 1


        RowLayout {
            anchors.fill: parent
            anchors.margins: margins
            spacing: margins

            Rectangle {
                id: closeButton
                // 参考3D测量页的动态按钮宽度策略：限制在一个舒适范围
                Layout.preferredWidth: Math.max(156, Math.min(234, (bottomBar.width - margins * 4) / 6))
                Layout.fillHeight: true
                color: Qt.rgba(0.7, 0.2, 0.2, 0.80)
                radius: 16
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.10)

                property bool hovered: false
                property int iconSize: Math.max(36, Math.round(height * 0.45))

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: stdWin.close()
                    onEntered: closeButton.hovered = true
                    onExited: closeButton.hovered = false
                }

                Row {
                    anchors.centerIn: parent
                    spacing: Math.max(12, Math.round(closeButton.height * 0.06))

                    // 使用 ColorOverlay 统一着色为白色，靠近3D测量页风格
                    Item {
                        width: closeButton.iconSize
                        height: closeButton.iconSize
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            id: closeIcon
                            source: "qrc:/icons/close.svg"
                            anchors.fill: parent
                            anchors.margins: Math.round(parent.width * 0.15)
                            fillMode: Image.PreserveAspectFit
                            visible: false
                        }
                        ColorOverlay {
                            anchors.fill: closeIcon
                            source: closeIcon
                            color: "#FFFFFF"
                        }
                    }

                    Text {
                        text: "关闭"
                        color: "#FFFFFF"
                        font.pixelSize: Math.max(28, stdWin.fontSize * 1.5)
                        font.weight: Font.Medium
                        font.family: stdWin.mixedFontMedium
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

    }

    // 作为与 bottomBar 同级的分割线，位于其上方边界，避免子项层级遮挡
    Rectangle {
        id: bottomBarTopSeparator
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomBar.top
        height: Math.max(1, Math.round(1 / Screen.devicePixelRatio))
        color: borderPrimary
        opacity: 1.0
    }
}
