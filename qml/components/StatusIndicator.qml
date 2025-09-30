import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

RowLayout {
    property string label: ""
    property string status: "disconnected" // connected, disconnected, error, disabled
    property string description: ""

    spacing: 5

    Rectangle {
        width: 8
        height: 8
        radius: 4
        color: {
            switch(status) {
                case "connected": return "#A3BE8C"
                case "error": return "#BF616A"
                case "disabled": return "#4C566A"
                default: return "#D08770"
            }
        }

        // 连接状态的呼吸灯效果
        SequentialAnimation on opacity {
            running: status === "connected"
            loops: Animation.Infinite
            PropertyAnimation { to: 0.3; duration: 1000 }
            PropertyAnimation { to: 1.0; duration: 1000 }
        }
    }

    Text {
        text: label
        color: "#D8DEE9"
        font.pixelSize: 12
    }
}