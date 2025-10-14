import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Rectangle {
    id: universalButton

    // 公共属性
    property string text: ""
    property string iconSource: ""
    property bool isActive: false

    // 样式预设
    property string buttonStyle: "toolbar"  // "toolbar", "navigation", "action"

    // 可自定义的颜色属性
    property color activeColor: "#0EA5E9"
    property color hoverColor: Qt.rgba(0.31, 0.31, 0.31, 0.7)
    property color backgroundColor: isActive ?
        Qt.rgba(0.2, 0.2, 0.2, 0.8) : Qt.rgba(0.12, 0.12, 0.12, 0.6)
    property color textColor: "#FFFFFF"
    property color iconColor: isActive ? activeColor : "#FFFFFF"

    // 尺寸预设
    property var styleConfig: {
        if (buttonStyle === "toolbar") {
            return {
                buttonWidth: 82,
                buttonHeight: 82,
                iconSize: 56,
                borderRadius: 8,
                borderWidth: 1.5
            }
        } else if (buttonStyle === "navigation") {
            return {
                buttonWidth: 120,
                buttonHeight: 60,
                iconSize: 32,
                borderRadius: 6,
                borderWidth: 1
            }
        } else if (buttonStyle === "action") {
            return {
                buttonWidth: 100,
                buttonHeight: 40,
                iconSize: 24,
                borderRadius: 4,
                borderWidth: 1
            }
        }
        // 默认工具栏样式
        return {
            buttonWidth: 82,
            buttonHeight: 82,
            iconSize: 56,
            borderRadius: 8,
            borderWidth: 1.5
        }
    }

    // 可选自定义尺寸（>0 时优先于预设）
    property int customButtonWidth: 0
    property int customButtonHeight: 0
    property int customIconSize: 0
    readonly property int effectiveButtonWidth: customButtonWidth > 0 ? customButtonWidth : styleConfig.buttonWidth
    readonly property int effectiveButtonHeight: customButtonHeight > 0 ? customButtonHeight : styleConfig.buttonHeight
    readonly property int effectiveIconSize: customIconSize > 0 ? customIconSize : styleConfig.iconSize

    // 信号
    signal clicked()
    signal pressed()
    signal released()

    // 私有/公开交互属性
    property bool hovered: false
    property bool pressed: false

    // 应用样式配置
    width: effectiveButtonWidth
    height: effectiveButtonHeight
    color: backgroundColor
    radius: styleConfig.borderRadius
    border.width: styleConfig.borderWidth
    border.color: isActive ?
        Qt.rgba(14/255, 165/255, 233/255, 0.5) : Qt.rgba(1, 1, 1, 0.1)

    // 鼠标交互
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: universalButton.clicked()
        onPressed: {
            universalButton.scale = 0.92
            universalButton.pressed = true
            universalButton.pressed()
        }
        onReleased: {
            universalButton.scale = 1.0
            universalButton.pressed = false
            universalButton.released()
        }
        onCanceled: {
            universalButton.scale = 1.0
            universalButton.pressed = false
            universalButton.released()
        }
        onEntered: universalButton.hovered = true
        onExited: universalButton.hovered = false
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
        opacity: universalButton.hovered ? 1 : 0
        Behavior on opacity { PropertyAnimation { duration: 200 } }
    }

    // 图标内容
    Item {
        anchors.centerIn: parent
        width: effectiveIconSize + 14  // 图标容器稍大一些
        height: effectiveIconSize + 14

        // 图标
        Image {
            id: icon
            source: iconSource
            anchors.centerIn: parent
            width: effectiveIconSize
            height: effectiveIconSize
            fillMode: Image.PreserveAspectFit
            visible: false
        }

        ColorOverlay {
            anchors.centerIn: parent
            width: effectiveIconSize
            height: effectiveIconSize
            source: icon
            color: iconColor
            Behavior on color { ColorAnimation { duration: 300 } }
        }

        DropShadow {
            anchors.centerIn: parent
            width: effectiveIconSize
            height: effectiveIconSize
            horizontalOffset: 0
            verticalOffset: 2
            radius: 8
            samples: 17
            color: activeColor
            opacity: universalButton.isActive ? 0.4 : 0
            source: icon
            Behavior on opacity { PropertyAnimation { duration: 300 } }
        }
    }

    // 文本标签（可选显示）
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        text: universalButton.text
        color: textColor
        font.pixelSize: 10
        font.family: "Inter"
        visible: text !== "" && buttonStyle !== "toolbar"  // 工具栏按钮不显示文本
        opacity: 0.8
    }

    // 触觉反馈动画
    SequentialAnimation {
        id: hapticFeedback
        PropertyAnimation {
            target: universalButton
            property: "border.width"
            to: styleConfig.borderWidth + 1
            duration: 100
        }
        PropertyAnimation {
            target: universalButton
            property: "border.width"
            to: styleConfig.borderWidth
            duration: 200
        }
    }

    // 点击触发触觉反馈
    onClicked: hapticFeedback.start()
}
