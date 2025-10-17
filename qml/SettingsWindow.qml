import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "components"
import "i18n" 1.0 as I18n

// 使用标准窗口模板承载设置页面内容
StandardAppWindow {
    id: settingsWindow
    windowTitle: I18n.I18n.tr("win.settings.title", "SmartScope Industrial - 系统设置")
    contentSource: "pages/SettingsPage.qml"
}
