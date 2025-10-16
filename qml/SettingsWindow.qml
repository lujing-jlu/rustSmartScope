import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "components"

// 使用标准窗口模板承载设置页面内容
StandardAppWindow {
    id: settingsWindow
    windowTitle: "SmartScope Industrial - 系统设置"
    contentSource: "pages/SettingsPage.qml"
}
