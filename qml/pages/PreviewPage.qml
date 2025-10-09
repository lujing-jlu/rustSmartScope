import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: previewPage
    color: "transparent"

    signal backRequested()
    signal navigateRequested(string pageType)

    Text {
        anchors.centerIn: parent
        text: "📚 媒体库"
        font.pixelSize: 48
        font.bold: true
        color: "#38BDF8"  // 亮蓝色主题
    }



    Component.onCompleted: {
        console.log("PreviewPage initialized")
    }
}