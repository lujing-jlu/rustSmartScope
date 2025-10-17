import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Rectangle {
    id: navButton

    // 公共属性
    property string text: ""
    property string iconSource: ""
    property bool isActive: false
    property bool isExitButton: false
    property bool iconOnly: false
    property bool isSquareButton: false  // 新增：方形按钮模式
    property color activeColor: "#0EA5E9"
    property color hoverColor: Qt.rgba(0.31, 0.31, 0.31, 0.7)

    // 计算的尺寸属性，基于按钮高度
    property int fontSize: buttonHeight * 0.25  // 字体大小为按钮高度的25%
    property int iconSize: buttonHeight * 0.5   // 图标大小为按钮高度的50%
    property int cornerRadius: buttonHeight * 0.15  // 圆角为按钮高度的15%
    property string mixedFontRegular: ""  // 字体族名

    // 统一的按钮样式
    property color buttonColor: isExitButton ?
        (hovered ? Qt.rgba(0.8, 0.2, 0.2, 0.8) : Qt.rgba(0.6, 0.15, 0.15, 0.6)) :
        (isActive ? Qt.rgba(0.25, 0.25, 0.25, 0.9) : Qt.rgba(0.12, 0.12, 0.12, 0.6))

    property color borderColor: isExitButton ?
        Qt.rgba(1, 0.4, 0.4, 0.3) :
        (isActive ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1))

    // 信号
    signal clicked()

    // 私有属性
    property bool hovered: false

    // 样式设置 - 使用计算值，基于传入的属性
    property int buttonHeight: parent.height || 120  // 使用父组件高度
    property int buttonWidth: (parent.height || 120) * 2  // 宽度为高度的2倍
    width: isSquareButton ? buttonHeight : buttonWidth
    height: buttonHeight
    color: buttonColor
    radius: cornerRadius
    border.width: 3
    border.color: borderColor

    // 鼠标交互
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: navButton.clicked()
        onPressed: navButton.scale = 0.92
        onReleased: navButton.scale = 1.0
        onEntered: navButton.hovered = true
        onExited: navButton.hovered = false
    }

    // 动画效果
    Behavior on scale {
        SpringAnimation { spring: 4; damping: 0.6; duration: 200 }
    }
    Behavior on color {
        ColorAnimation { duration: 300; easing.type: Easing.OutCubic }
    }
    Behavior on border.color {
        ColorAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    // 悬停背景效果
    Rectangle {
        anchors.fill: parent
        color: hoverColor
        radius: parent.radius
        opacity: navButton.hovered ? 1 : 0
        Behavior on opacity { PropertyAnimation { duration: 200 } }
    }

    // 按钮内容
    Row {
        anchors.centerIn: parent
        spacing: iconOnly ? 0 : 8  // 固定8px间距，不使用计算值

        // 图标容器
        Item {
            width: iconSize
            height: iconSize
            anchors.verticalCenter: parent.verticalCenter

            // 图标加载器
            Loader {
                anchors.fill: parent
                sourceComponent: iconSource ? iconComponent : textIconComponent
            }

            // 图标组件
            Component {
                id: iconComponent
                Item {
                    Image {
                        id: icon
                        source: iconSource
                        anchors.fill: parent
                        anchors.margins: parent.width * 0.15
                        fillMode: Image.PreserveAspectFit
                        visible: false
                    }

                    ColorOverlay {
                        anchors.fill: icon
                        source: icon
                        color: "#FFFFFF"
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                }
            }

            // 文字图标组件（用于退出按钮等）
            Component {
                id: textIconComponent
                Text {
                    text: "✕"
                    color: "#FFFFFF"
                    font.pixelSize: iconSize * 0.6
                    font.weight: Font.Bold
                    anchors.centerIn: parent
                }
            }
        }

        // 按钮文字 (只在非图标模式显示)
        Text {
            id: labelText
            text: navButton.text
            color: "#FFFFFF"
            // 自动按可用宽度缩放字号，防止英文溢出
            readonly property int baseSize: Math.round(fontSize * 1.4)
            readonly property int maxWidth: navButton.width - (navButton.iconOnly ? 0 : (navButton.iconSize + 8 + 16))
            font.pixelSize: Math.max(12, Math.min(baseSize, Math.floor(baseSize * (maxWidth / Math.max(1, paintedWidth)))))
            font.weight: Font.Medium
            font.family: mixedFontRegular
            elide: Text.ElideRight
            anchors.verticalCenter: parent.verticalCenter
            visible: !iconOnly
            horizontalAlignment: Text.AlignHCenter
            Behavior on color { ColorAnimation { duration: 300 } }
        }
    }
}
