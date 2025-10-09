import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import RustSmartScope.Logger 1.0
import "../components"

Rectangle {
    id: homePage
    color: "transparent"

    signal backRequested()
    signal navigateRequested(string pageType)

    Text {
        anchors.centerIn: parent
        text: "🏠 主页"
        font.pixelSize: 48
        font.bold: true
        color: "#0EA5E9"  // 专业蓝色主题
    }

    Component.onCompleted: {
        Logger.info("HomePage initialized")
    }
}