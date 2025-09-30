import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: measurementPage
    color: "transparent"

    signal backRequested()
    signal navigateRequested(string pageType)

    Text {
        anchors.centerIn: parent
        text: "📏 3D测量页面"
        font.pixelSize: 48
        font.bold: true
        color: "#67E8F9"  // 青色主题
    }



    Component.onCompleted: {
        console.log("MeasurementPage initialized")
    }
}