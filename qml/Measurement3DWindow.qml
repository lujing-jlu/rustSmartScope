import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "components"
import "i18n" 1.0 as I18n

// 使用标准窗口模板承载3D测量页面内容
StandardAppWindow {
    id: measurement3DWindow
    windowTitle: I18n.I18n.tr("win.measure.title", "SmartScope Industrial - 3D测量")
    contentSource: "pages/MeasurementPage.qml"
}
