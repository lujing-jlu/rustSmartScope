import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: settingsPage
    color: "transparent"

    signal backRequested()
    signal navigateRequested(string pageType)

    Text {
        anchors.centerIn: parent
        text: "⚙️ 设置页面"
        font.pixelSize: 48
        font.bold: true
        color: "#F59E0B"  // 橙色警告色
    }


    Component.onCompleted: {
        console.log("SettingsPage initialized")
    }
}