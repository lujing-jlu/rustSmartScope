import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "components"

// 使用标准窗口模板承载3D测量页面内容
StandardAppWindow {
    id: measurement3DWindow
    windowTitle: "SmartScope Industrial - 3D测量"
    contentSource: "pages/MeasurementPage.qml"
}

