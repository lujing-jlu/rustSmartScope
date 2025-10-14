import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import RustSmartScope.Logger 1.0

Rectangle {
    id: reportPage
    color: "transparent"

    signal backRequested()
    signal navigateRequested(string pageType)

    Text {
        anchors.centerIn: parent
        text: "📊 报告页面"
        font.pixelSize: 48
        font.bold: true
        color: "#10B981"  // 成功绿色主题
    }



    Component.onCompleted: {
        Logger.info("ReportPage initialized")
    }
}