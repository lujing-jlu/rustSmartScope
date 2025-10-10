import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "components"

// 相机参数设置窗口 - 使用GlassPopupWindow包装
GlassPopupWindow {
    id: cameraParameterWindow

    // 设置窗口属性 - 横向布局
    windowTitle: "相机参数设置"
    windowWidth: 1400
    windowHeight: 700

    // 对外属性
    property int cameraMode: 0  // 0=NoCamera, 1=SingleCamera, 2=StereoCamera
    property real textScale: 1.5

    // 内部状态
    QtObject {
        id: internal

        // 获取当前相机属性枚举值
        function getCameraPropertyEnum(name) {
            var propertyMap = {
                "brightness": 0,
                "contrast": 1,
                "saturation": 2,
                "hue": 3,
                "gain": 4,
                "exposure": 5,
                "focus": 6,
                "white_balance": 7,
                "frame_rate": 8,
                "resolution": 9,
                "gamma": 10,
                "backlight": 11
            }
            return propertyMap[name] || 0
        }

        // 调用FFI设置参数
        function setParameter(propertyName, value) {
            var propertyEnum = getCameraPropertyEnum(propertyName)

            // 根据相机模式选择设置哪个相机
            if (cameraParameterWindow.cameraMode === 1) {
                // 单相机模式
                var success = CameraParameterManager.setSingleCameraParameter(propertyEnum, value)
                if (success) {
                    console.log("设置单相机参数成功:", propertyName, "=", value)
                } else {
                    console.error("设置单相机参数失败:", propertyName, "=", value)
                }
            } else if (cameraParameterWindow.cameraMode === 2) {
                // 双相机模式 - 同时设置左右相机
                var leftSuccess = CameraParameterManager.setLeftCameraParameter(propertyEnum, value)
                var rightSuccess = CameraParameterManager.setRightCameraParameter(propertyEnum, value)

                if (leftSuccess && rightSuccess) {
                    console.log("设置左右相机参数成功:", propertyName, "=", value)
                } else {
                    console.error("设置相机参数失败:", propertyName, "左:", leftSuccess, "右:", rightSuccess)
                }
            }
        }

        // 重置参数到默认值（从V4L2获取真实默认值）
        function resetParameters() {
            // 根据相机模式选择使用哪个相机的默认值
            if (cameraParameterWindow.cameraMode === 1) {
                // 单相机模式
                loadDefaultValues("single")
            } else if (cameraParameterWindow.cameraMode === 2) {
                // 双相机模式 - 使用左相机的默认值
                loadDefaultValues("left")
            }
        }

        // 从FFI加载默认值
        function loadDefaultValues(cameraType) {
            var properties = [
                "brightness", "contrast", "saturation",
                "backlight", "gamma", "exposure", "white_balance"
            ]

            for (var i = 0; i < properties.length; i++) {
                var propName = properties[i]
                var propEnum = getCameraPropertyEnum(propName)
                var range

                // 根据相机类型获取参数范围
                if (cameraType === "single") {
                    range = CameraParameterManager.getSingleCameraParameterRange(propEnum)
                } else if (cameraType === "left") {
                    range = CameraParameterManager.getLeftCameraParameterRange(propEnum)
                } else if (cameraType === "right") {
                    range = CameraParameterManager.getRightCameraParameterRange(propEnum)
                }

                // 设置滑块到默认值
                if (range && range.default_value !== undefined) {
                    switch(propName) {
                        case "brightness":
                            brightnessSlider.value = range.default_value
                            break
                        case "contrast":
                            contrastSlider.value = range.default_value
                            break
                        case "saturation":
                            saturationSlider.value = range.default_value
                            break
                        case "backlight":
                            backlightSlider.value = range.default_value
                            break
                        case "gamma":
                            gammaSlider.value = range.default_value
                            break
                        case "exposure":
                            exposureSlider.value = range.default_value
                            break
                        case "white_balance":
                            whiteBalanceSlider.value = range.default_value
                            break
                    }
                }
            }

            // 设置自动曝光和自动白平衡为默认状态
            autoExposureCheck.checked = true
            autoWhiteBalanceCheck.checked = true
        }
    }

    // 窗口内容
    content: ColumnLayout {
        anchors.fill: parent
        spacing: 20

        // 参数区域 - 横向两列布局
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 4
            columnSpacing: 30
            rowSpacing: 20

            // 左侧列 - 基础参数
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.columnSpan: 2
                spacing: 20

                Label {
                    text: "基础参数"
                    font.pixelSize: 22 * cameraParameterWindow.textScale
                    font.bold: true
                    color: "#4CAF50"
                }

                // 亮度
                RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "亮度:"
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 100
                        }

                        Slider {
                            id: brightnessSlider
                            Layout.fillWidth: true
                            from: -64
                            to: 64
                            value: 0
                            stepSize: 1
                            onValueChanged: {
                                internal.setParameter("brightness", value)
                            }

                            background: Rectangle {
                                x: brightnessSlider.leftPadding
                                y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 10
                                width: brightnessSlider.availableWidth
                                height: implicitHeight
                                radius: 5
                                color: "#555"

                                Rectangle {
                                    width: brightnessSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#FFC107"
                                    radius: 5
                                }
                            }

                            handle: Rectangle {
                                x: brightnessSlider.leftPadding + brightnessSlider.visualPosition * (brightnessSlider.availableWidth - width)
                                y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                                implicitWidth: 56
                                implicitHeight: 56
                                radius: 28
                                color: brightnessSlider.pressed ? "#666666" : "#888888"
                                border.color: "#000000"
                                border.width: 2
                            }
                        }

                        Label {
                            text: Math.round(brightnessSlider.value).toString()
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // 对比度
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "对比度:"
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 100
                        }

                        Slider {
                            id: contrastSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 95
                            value: 0
                            stepSize: 1
                            onValueChanged: {
                                internal.setParameter("contrast", value)
                            }

                            background: Rectangle {
                                x: contrastSlider.leftPadding
                                y: contrastSlider.topPadding + contrastSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 10
                                width: contrastSlider.availableWidth
                                height: implicitHeight
                                radius: 5
                                color: "#555"

                                Rectangle {
                                    width: contrastSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#9C27B0"
                                    radius: 5
                                }
                            }

                            handle: Rectangle {
                                x: contrastSlider.leftPadding + contrastSlider.visualPosition * (contrastSlider.availableWidth - width)
                                y: contrastSlider.topPadding + contrastSlider.availableHeight / 2 - height / 2
                                implicitWidth: 56
                                implicitHeight: 56
                                radius: 28
                                color: contrastSlider.pressed ? "#666666" : "#888888"
                                border.color: "#000000"
                                border.width: 2
                            }
                        }

                        Label {
                            text: Math.round(contrastSlider.value).toString()
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // 饱和度
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "饱和度:"
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 100
                        }

                        Slider {
                            id: saturationSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 50
                            stepSize: 1
                            onValueChanged: {
                                internal.setParameter("saturation", value)
                            }

                            background: Rectangle {
                                x: saturationSlider.leftPadding
                                y: saturationSlider.topPadding + saturationSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 10
                                width: saturationSlider.availableWidth
                                height: implicitHeight
                                radius: 5
                                color: "#555"

                                Rectangle {
                                    width: saturationSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#FF5722"
                                    radius: 5
                                }
                            }

                            handle: Rectangle {
                                x: saturationSlider.leftPadding + saturationSlider.visualPosition * (saturationSlider.availableWidth - width)
                                y: saturationSlider.topPadding + saturationSlider.availableHeight / 2 - height / 2
                                implicitWidth: 56
                                implicitHeight: 56
                                radius: 28
                                color: saturationSlider.pressed ? "#666666" : "#888888"
                                border.color: "#000000"
                                border.width: 2
                            }
                        }

                        Label {
                            text: Math.round(saturationSlider.value).toString()
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // 背光补偿
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "背光补偿:"
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 100
                        }

                        Slider {
                            id: backlightSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 8
                            value: 0
                            stepSize: 1
                            onValueChanged: {
                                internal.setParameter("backlight", value)
                            }

                            background: Rectangle {
                                x: backlightSlider.leftPadding
                                y: backlightSlider.topPadding + backlightSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 10
                                width: backlightSlider.availableWidth
                                height: implicitHeight
                                radius: 5
                                color: "#555"

                                Rectangle {
                                    width: backlightSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#00BCD4"
                                    radius: 5
                                }
                            }

                            handle: Rectangle {
                                x: backlightSlider.leftPadding + backlightSlider.visualPosition * (backlightSlider.availableWidth - width)
                                y: backlightSlider.topPadding + backlightSlider.availableHeight / 2 - height / 2
                                implicitWidth: 56
                                implicitHeight: 56
                                radius: 28
                                color: backlightSlider.pressed ? "#666666" : "#888888"
                                border.color: "#000000"
                                border.width: 2
                            }
                        }

                        Label {
                            text: Math.round(backlightSlider.value).toString()
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Gamma
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "Gamma:"
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 100
                        }

                        Slider {
                            id: gammaSlider
                            Layout.fillWidth: true
                            from: 32
                            to: 300
                            value: 100
                            stepSize: 1
                            onValueChanged: {
                                internal.setParameter("gamma", value)
                            }

                            background: Rectangle {
                                x: gammaSlider.leftPadding
                                y: gammaSlider.topPadding + gammaSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 10
                                width: gammaSlider.availableWidth
                                height: implicitHeight
                                radius: 5
                                color: "#555"

                                Rectangle {
                                    width: gammaSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#8BC34A"
                                    radius: 5
                                }
                            }

                            handle: Rectangle {
                                x: gammaSlider.leftPadding + gammaSlider.visualPosition * (gammaSlider.availableWidth - width)
                                y: gammaSlider.topPadding + gammaSlider.availableHeight / 2 - height / 2
                                implicitWidth: 56
                                implicitHeight: 56
                                radius: 28
                                color: gammaSlider.pressed ? "#666666" : "#888888"
                                border.color: "#000000"
                                border.width: 2
                            }
                        }

                        Label {
                            text: Math.round(gammaSlider.value).toString()
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                // 右侧列 - 曝光和白平衡
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.columnSpan: 2
                    spacing: 20

                    Label {
                        text: "高级参数"
                        font.pixelSize: 22 * cameraParameterWindow.textScale
                        font.bold: true
                        color: "#2196F3"
                    }

                    // 自动曝光
                    CheckBox {
                        id: autoExposureCheck
                        text: "自动曝光"
                        checked: true
                        font.pixelSize: 18 * cameraParameterWindow.textScale

                        contentItem: Text {
                            text: autoExposureCheck.text
                            font: autoExposureCheck.font
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: autoExposureCheck.indicator.width + autoExposureCheck.spacing
                        }

                        indicator: Rectangle {
                            implicitWidth: 24
                            implicitHeight: 24
                            x: autoExposureCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            border.color: "#000000"
                            border.width: 2
                            color: autoExposureCheck.checked ? "#555555" : "transparent"

                            Text {
                                visible: autoExposureCheck.checked
                                text: "✓"
                                color: "white"
                                font.pixelSize: 16 * cameraParameterWindow.textScale
                                anchors.centerIn: parent
                            }
                        }
                    }

                    // 曝光时间
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "曝光时间:"
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 100
                        }

                        Slider {
                            id: exposureSlider
                            Layout.fillWidth: true
                            from: 3
                            to: 2047
                            value: 3
                            enabled: !autoExposureCheck.checked
                            stepSize: 1
                            onValueChanged: {
                                if (!autoExposureCheck.checked) {
                                    internal.setParameter("exposure", value)
                                }
                            }


                            background: Rectangle {
                                x: exposureSlider.leftPadding
                                y: exposureSlider.topPadding + exposureSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 10
                                width: exposureSlider.availableWidth
                                height: implicitHeight
                                radius: 5
                                color: "#555"

                                Rectangle {
                                    width: exposureSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#4CAF50"
                                    radius: 5
                                }
                            }

                            handle: Rectangle {
                                x: exposureSlider.leftPadding + exposureSlider.visualPosition * (exposureSlider.availableWidth - width)
                                y: exposureSlider.topPadding + exposureSlider.availableHeight / 2 - height / 2
                                implicitWidth: 56
                                implicitHeight: 56
                                radius: 28
                                color: exposureSlider.pressed ? "#666666" : "#888888"
                                border.color: "#000000"
                                border.width: 2
                            }
                        }

                        Label {
                            text: Math.round(exposureSlider.value).toString()
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // 增益
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "增益:"
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 100
                        }

                        Slider {
                            id: gainSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 3
                            value: 0
                            stepSize: 1
                            onValueChanged: {
                                if (!autoExposureCheck.checked) {
                                    internal.setParameter("gain", value)
                                }
                            }


                            background: Rectangle {
                                x: gainSlider.leftPadding
                                y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 10
                                width: gainSlider.availableWidth
                                height: implicitHeight
                                radius: 5
                                color: "#555"

                                Rectangle {
                                    width: gainSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#E91E63"
                                    radius: 5
                                }
                            }

                            handle: Rectangle {
                                x: gainSlider.leftPadding + gainSlider.visualPosition * (gainSlider.availableWidth - width)
                                y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                                implicitWidth: 56
                                implicitHeight: 56
                                radius: 28
                                color: gainSlider.pressed ? "#666666" : "#888888"
                                border.color: "#000000"
                                border.width: 2
                            }
                        }

                        Label {
                            text: Math.round(gainSlider.value).toString()
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // 自动白平衡
                    CheckBox {
                        id: autoWhiteBalanceCheck
                        text: "自动白平衡"
                        checked: false
                        font.pixelSize: 18 * cameraParameterWindow.textScale

                        contentItem: Text {
                            text: autoWhiteBalanceCheck.text
                            font: autoWhiteBalanceCheck.font
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: autoWhiteBalanceCheck.indicator.width + autoWhiteBalanceCheck.spacing
                        }

                        indicator: Rectangle {
                            implicitWidth: 24
                            implicitHeight: 24
                            x: autoWhiteBalanceCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            border.color: "#000000"
                            border.width: 2
                            color: autoWhiteBalanceCheck.checked ? "#555555" : "transparent"

                            Text {
                                visible: autoWhiteBalanceCheck.checked
                                text: "✓"
                                color: "white"
                                font.pixelSize: 16 * cameraParameterWindow.textScale
                                anchors.centerIn: parent
                            }
                        }
                    }

                    // 白平衡温度
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "白平衡:"
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 100
                        }

                        Slider {
                            id: whiteBalanceSlider
                            Layout.fillWidth: true
                            from: 2000
                            to: 6500
                            value: 4500
                            enabled: !autoWhiteBalanceCheck.checked
                            stepSize: 100
                            onValueChanged: {
                                if (!autoWhiteBalanceCheck.checked) {
                                    internal.setParameter("white_balance", value)
                                }
                            }


                            background: Rectangle {
                                x: whiteBalanceSlider.leftPadding
                                y: whiteBalanceSlider.topPadding + whiteBalanceSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 10
                                width: whiteBalanceSlider.availableWidth
                                height: implicitHeight
                                radius: 5
                                color: "#555"

                                Rectangle {
                                    width: whiteBalanceSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#2196F3"
                                    radius: 5
                                }
                            }

                            handle: Rectangle {
                                x: whiteBalanceSlider.leftPadding + whiteBalanceSlider.visualPosition * (whiteBalanceSlider.availableWidth - width)
                                y: whiteBalanceSlider.topPadding + whiteBalanceSlider.availableHeight / 2 - height / 2
                                implicitWidth: 56
                                implicitHeight: 56
                                radius: 28
                                color: whiteBalanceSlider.pressed ? "#666666" : "#888888"
                                border.color: "#000000"
                                border.width: 2
                            }
                        }

                        Label {
                            text: Math.round(whiteBalanceSlider.value).toString()
                            font.pixelSize: 18 * cameraParameterWindow.textScale
                            color: "white"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // 按钮区域
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                spacing: 20

                Button {
                    text: "重置默认值"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50

                    background: Rectangle {
                        color: parent.pressed ? "#333333" : (parent.hovered ? "#777777" : "#555555")
                        border.color: "#000000"
                        border.width: 2
                        radius: 10
                    }

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 18 * cameraParameterWindow.textScale
                        font.bold: true
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        internal.resetParameters()
                        console.log("重置相机参数到默认值")
                        // 调用FFI重置
                        if (cameraParameterWindow.cameraMode === 1) {
                            CameraParameterManager.resetSingleCameraParameters()
                        } else if (cameraParameterWindow.cameraMode === 2) {
                            CameraParameterManager.resetLeftCameraParameters()
                            CameraParameterManager.resetRightCameraParameters()
                        }
                    }
                }

            }
        }

    // 窗口事件处理
    onVisibleChanged: {
        if (visible) {
            console.log("相机参数设置窗口已打开")
            raise()
            requestActivate()
        } else {
            console.log("相机参数设置窗口已关闭")
        }
    }

    // 重写 show 方法确保窗口正确显示
    function show() {
        visible = true
        raise()
        requestActivate()

        // 绑定相机模式
        cameraMode = Qt.binding(function() { return CameraManager.cameraMode })
    }
}
