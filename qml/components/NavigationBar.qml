import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
import RustSmartScope.Logger 1.0

Rectangle {
    id: navigationBar

    // 公共属性 - 从主窗口传入
    property int navigationBarHeight: 100
    property int margins: 12
    property int cornerRadius: 16
    property string mixedFontRegular: ""

    // 信号
    signal homeClicked()
    signal previewClicked()
    signal reportClicked()
    signal settingsClicked()
    signal measurementClicked()
    signal exitClicked()

    // 活动状态管理
    property string activeTab: "home"
    // 仅在双相机(立体)模式下显示3D测量入口
    property bool stereoAvailable: (typeof CameraManager !== 'undefined' && CameraManager) ? (CameraManager.cameraMode === 2) : false

    function setActiveTab(tabName) {
        activeTab = tabName
        homeButton.isActive = (tabName === "home")
        detectionButton.isActive = (tabName === "media")
        reportButton.isActive = (tabName === "report")
        settingsButton.isActive = (tabName === "settings")
        measurementButton.isActive = (tabName === "measurement")
    }

    // 导航栏样式 - 移除背景和边框
    color: "transparent"
    radius: 0
    border.width: 0

    // 固定单排导航按钮布局
    Row {
        anchors.centerIn: parent
        spacing: 10

        // 主页按钮
        UnifiedNavigationButton {
            id: homeButton
            text: "主页"
            iconSource: "qrc:/icons/home.svg"
            isActive: activeTab === "home"
            iconOnly: true
            isSquareButton: true
            onClicked: {
                Logger.info("Home clicked")
                setActiveTab("home")
                navigationBar.homeClicked()
            }
        }

        // 媒体库按钮
        UnifiedNavigationButton {
            id: detectionButton
            text: "媒体库"
            iconSource: "qrc:/icons/preview.svg"
            isActive: activeTab === "media"
            activeColor: "#38BDF8"
            onClicked: {
                Logger.info("Media Library clicked")
                navigationBar.previewClicked()
            }
        }

        // 报告按钮（已隐藏）
        UnifiedNavigationButton {
            visible: false
            width: 0
            height: 0

            id: reportButton
            text: "报告"
            iconSource: "qrc:/icons/report.svg"
            isActive: activeTab === "report"
            onClicked: {
                Logger.info("Report clicked")
                setActiveTab("report")
                navigationBar.reportClicked()
            }
        }

        // 设置按钮
        UnifiedNavigationButton {
            id: settingsButton
            text: "设置"
            iconSource: "qrc:/icons/setting.svg"
            isActive: activeTab === "settings"
            onClicked: {
                Logger.info("Settings clicked")
                setActiveTab("settings")
                navigationBar.settingsClicked()
            }
        }

        // 3D测量按钮
        UnifiedNavigationButton {
            id: measurementButton
            text: "3D测量"
            iconSource: "qrc:/icons/3D.svg"
            isActive: activeTab === "measurement"
            visible: navigationBar.stereoAvailable
            onClicked: {
                Logger.info("3D测量窗口打开")
                navigationBar.measurementClicked()
            }
        }

        // 退出按钮
        UnifiedNavigationButton {
            id: exitButton
            text: "退出"
            iconSource: "qrc:/icons/close.svg"
            isExitButton: true
            iconOnly: true
            isSquareButton: true
            onClicked: {
                Logger.info("Exit clicked")
                navigationBar.exitClicked()
            }
        }
    }
}
