import QtQuick 2.15
import QtQuick.Controls 2.15

// 媒体库占位页面（后续可填充内容列表、缩略图等）
Rectangle {
    id: root
    color: "transparent"

    Label {
        anchors.centerIn: parent
        text: "媒体库（开发中）"
        color: "#FFFFFF"
        font.pixelSize: 32
    }
}
