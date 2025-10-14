import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import RustSmartScope.Logger 1.0

// 相机参数调节面板组件
Rectangle {
    id: root

    // 面板尺寸
    width: 550
    height: 800

    // 样式
    color: Qt.rgba(30/255, 30/255, 30/255, 240/255)
    border.color: "#444444"
    border.width: 2
    radius: 10

    // 对外属性
    property int cameraMode: 0  // 0=NoCamera, 1=SingleCamera, 2=StereoCamera

    // 信号
    signal applySettings()
    signal resetToDefaults()
    signal panelClosed()

    // 内部状态
    QtObject {
        id: internal

        // 获取当前相机属性枚举值 - 匹配C FFI枚举定义
        function getCameraPropertyEnum(name) {
            var propertyMap = {
                "brightness": 0,
                "contrast": 1,
                "saturation": 2,
                "gain": 3,
                "exposure": 4,
                // white_balance: 5 - 不支持，已移除
                "gamma": 6,
                "backlight": 7,
                "auto_exposure": 8
            }
            return propertyMap[name] || 0
        }

        // 调用FFI设置参数
        function setParameter(propertyName, value) {
            var propertyEnum = getCameraPropertyEnum(propertyName)

            // 根据相机模式选择设置哪个相机
            if (root.cameraMode === 1) {
                // 单相机模式
                var success = CameraParameterManager.setSingleCameraParameter(propertyEnum, value)
                if (success) {
                    Logger.info("设置单相机参数成功:", propertyName, "=", value)
                } else {
                    Logger.error("设置单相机参数失败:", propertyName, "=", value)
                }
            } else if (root.cameraMode === 2) {
                // 双相机模式 - 同时设置左右相机
                var leftSuccess = CameraParameterManager.setLeftCameraParameter(propertyEnum, value)
                var rightSuccess = CameraParameterManager.setRightCameraParameter(propertyEnum, value)

                if (leftSuccess && rightSuccess) {
                    Logger.info("设置左右相机参数成功:", propertyName, "=", value)
                } else {
                    Logger.error("设置相机参数失败:", propertyName, "左:", leftSuccess, "右:", rightSuccess)
                }
            }
        }

        // 重置参数到默认值（从V4L2获取真实默认值）
        function resetParameters() {
            // 根据相机模式选择使用哪个相机的默认值
            if (root.cameraMode === 1) {
                // 单相机模式
                loadDefaultValues("single")
            } else if (root.cameraMode === 2) {
                // 双相机模式 - 使用左相机的默认值
                loadDefaultValues("left")
            }
        }

        // 从FFI加载默认值
        function loadDefaultValues(cameraType) {
            var properties = [
                "brightness", "contrast", "saturation",
                "backlight", "gamma", "exposure"
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
                    }
                }
            }

            // 设置自动曝光为默认状态
            autoExposureCheck.checked = true
        }

        // 应用所有设置
        function applyAllSettings() {
            // 亮度
            setParameter("brightness", brightnessSlider.value)

            // 对比度
            setParameter("contrast", contrastSlider.value)

            // 饱和度
            setParameter("saturation", saturationSlider.value)

            // 背光补偿
            setParameter("backlight", backlightSlider.value)

            // Gamma
            setParameter("gamma", gammaSlider.value)

            // 增益
            if (!autoExposureCheck.checked) {
                setParameter("gain", gainSlider.value)
            }

            // 曝光时间
            if (!autoExposureCheck.checked) {
                setParameter("exposure", exposureSlider.value)
            }
        }
    }

    // 主布局
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 25

        // 标题
        Label {
            text: "相机参数调节"
            font.pixelSize: 28
            font.bold: true
            color: "white"
            Layout.alignment: Qt.AlignHCenter
        }

        // 参数表单
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            columnSpacing: 15
            rowSpacing: 25

            // 自动曝光
            Item {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                Layout.preferredHeight: 40

                CheckBox {
                    id: autoExposureCheck
                    text: "自动曝光"
                    checked: true

                    font.pixelSize: 22

                    contentItem: Text {
                        text: autoExposureCheck.text
                        font: autoExposureCheck.font
                        color: "white"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: autoExposureCheck.indicator.width + autoExposureCheck.spacing
                    }

                    indicator: Rectangle {
                        implicitWidth: 30
                        implicitHeight: 30
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
                            font.pixelSize: 20
                            anchors.centerIn: parent
                        }
                    }
                }
            }

            // 曝光时间
            Label {
                text: "曝光时间:"
                font.pixelSize: 22
                font.bold: true
                color: "white"
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Slider {
                    id: exposureSlider
                    Layout.fillWidth: true
                    from: 3
                    to: 2047
                    value: 3
                    enabled: !autoExposureCheck.checked
                    stepSize: 1

                    background: Rectangle {
                        x: exposureSlider.leftPadding
                        y: exposureSlider.topPadding + exposureSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 14
                        width: exposureSlider.availableWidth
                        height: implicitHeight
                        radius: 7
                        color: "#555"

                        Rectangle {
                            width: exposureSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#4CAF50"
                            radius: 7
                        }
                    }

                    handle: Rectangle {
                        x: exposureSlider.leftPadding + exposureSlider.visualPosition * (exposureSlider.availableWidth - width)
                        y: exposureSlider.topPadding + exposureSlider.availableHeight / 2 - height / 2
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: exposureSlider.pressed ? "#666666" : "#888888"
                        border.color: "#000000"
                        border.width: 2
                    }
                }

                Label {
                    text: Math.round(exposureSlider.value).toString()
                    font.pixelSize: 22
                    color: "white"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // 亮度
            Label {
                text: "亮度:"
                font.pixelSize: 22
                font.bold: true
                color: "white"
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Slider {
                    id: brightnessSlider
                    Layout.fillWidth: true
                    from: -64
                    to: 64
                    value: 0
                    stepSize: 1

                    background: Rectangle {
                        x: brightnessSlider.leftPadding
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 14
                        width: brightnessSlider.availableWidth
                        height: implicitHeight
                        radius: 7
                        color: "#555"

                        Rectangle {
                            width: brightnessSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#FFC107"
                            radius: 7
                        }
                    }

                    handle: Rectangle {
                        x: brightnessSlider.leftPadding + brightnessSlider.visualPosition * (brightnessSlider.availableWidth - width)
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: brightnessSlider.pressed ? "#666666" : "#888888"
                        border.color: "#000000"
                        border.width: 2
                    }
                }

                Label {
                    text: Math.round(brightnessSlider.value).toString()
                    font.pixelSize: 22
                    color: "white"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // 对比度
            Label {
                text: "对比度:"
                font.pixelSize: 22
                font.bold: true
                color: "white"
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Slider {
                    id: contrastSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 95
                    value: 0
                    stepSize: 1

                    background: Rectangle {
                        x: contrastSlider.leftPadding
                        y: contrastSlider.topPadding + contrastSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 14
                        width: contrastSlider.availableWidth
                        height: implicitHeight
                        radius: 7
                        color: "#555"

                        Rectangle {
                            width: contrastSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#9C27B0"
                            radius: 7
                        }
                    }

                    handle: Rectangle {
                        x: contrastSlider.leftPadding + contrastSlider.visualPosition * (contrastSlider.availableWidth - width)
                        y: contrastSlider.topPadding + contrastSlider.availableHeight / 2 - height / 2
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: contrastSlider.pressed ? "#666666" : "#888888"
                        border.color: "#000000"
                        border.width: 2
                    }
                }

                Label {
                    text: Math.round(contrastSlider.value).toString()
                    font.pixelSize: 22
                    color: "white"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // 饱和度
            Label {
                text: "饱和度:"
                font.pixelSize: 22
                font.bold: true
                color: "white"
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Slider {
                    id: saturationSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 50
                    stepSize: 1

                    background: Rectangle {
                        x: saturationSlider.leftPadding
                        y: saturationSlider.topPadding + saturationSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 14
                        width: saturationSlider.availableWidth
                        height: implicitHeight
                        radius: 7
                        color: "#555"

                        Rectangle {
                            width: saturationSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#FF5722"
                            radius: 7
                        }
                    }

                    handle: Rectangle {
                        x: saturationSlider.leftPadding + saturationSlider.visualPosition * (saturationSlider.availableWidth - width)
                        y: saturationSlider.topPadding + saturationSlider.availableHeight / 2 - height / 2
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: saturationSlider.pressed ? "#666666" : "#888888"
                        border.color: "#000000"
                        border.width: 2
                    }
                }

                Label {
                    text: Math.round(saturationSlider.value).toString()
                    font.pixelSize: 22
                    color: "white"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // 背光补偿
            Label {
                text: "背光补偿:"
                font.pixelSize: 22
                font.bold: true
                color: "white"
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Slider {
                    id: backlightSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 8
                    value: 0
                    stepSize: 1

                    background: Rectangle {
                        x: backlightSlider.leftPadding
                        y: backlightSlider.topPadding + backlightSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 14
                        width: backlightSlider.availableWidth
                        height: implicitHeight
                        radius: 7
                        color: "#555"

                        Rectangle {
                            width: backlightSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#00BCD4"
                            radius: 7
                        }
                    }

                    handle: Rectangle {
                        x: backlightSlider.leftPadding + backlightSlider.visualPosition * (backlightSlider.availableWidth - width)
                        y: backlightSlider.topPadding + backlightSlider.availableHeight / 2 - height / 2
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: backlightSlider.pressed ? "#666666" : "#888888"
                        border.color: "#000000"
                        border.width: 2
                    }
                }

                Label {
                    text: Math.round(backlightSlider.value).toString()
                    font.pixelSize: 22
                    color: "white"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // Gamma
            Label {
                text: "Gamma:"
                font.pixelSize: 22
                font.bold: true
                color: "white"
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Slider {
                    id: gammaSlider
                    Layout.fillWidth: true
                    from: 32
                    to: 300
                    value: 100
                    stepSize: 1

                    background: Rectangle {
                        x: gammaSlider.leftPadding
                        y: gammaSlider.topPadding + gammaSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 14
                        width: gammaSlider.availableWidth
                        height: implicitHeight
                        radius: 7
                        color: "#555"

                        Rectangle {
                            width: gammaSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#8BC34A"
                            radius: 7
                        }
                    }

                    handle: Rectangle {
                        x: gammaSlider.leftPadding + gammaSlider.visualPosition * (gammaSlider.availableWidth - width)
                        y: gammaSlider.topPadding + gammaSlider.availableHeight / 2 - height / 2
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: gammaSlider.pressed ? "#666666" : "#888888"
                        border.color: "#000000"
                        border.width: 2
                    }
                }

                Label {
                    text: Math.round(gammaSlider.value).toString()
                    font.pixelSize: 22
                    color: "white"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // 增益
            Label {
                text: "增益:"
                font.pixelSize: 22
                font.bold: true
                color: "white"
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Slider {
                    id: gainSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 3
                    value: 0
                    stepSize: 1

                    background: Rectangle {
                        x: gainSlider.leftPadding
                        y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 14
                        width: gainSlider.availableWidth
                        height: implicitHeight
                        radius: 7
                        color: "#555"

                        Rectangle {
                            width: gainSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#E91E63"
                            radius: 7
                        }
                    }

                    handle: Rectangle {
                        x: gainSlider.leftPadding + gainSlider.visualPosition * (gainSlider.availableWidth - width)
                        y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: gainSlider.pressed ? "#666666" : "#888888"
                        border.color: "#000000"
                        border.width: 2
                    }
                }

                Label {
                    text: Math.round(gainSlider.value).toString()
                    font.pixelSize: 22
                    color: "white"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 按钮区域
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            spacing: 30

            Button {
                text: "重置默认值"
                Layout.fillWidth: true
                Layout.preferredHeight: 60

                background: Rectangle {
                    color: parent.pressed ? "#333333" : (parent.hovered ? "#777777" : "#555555")
                    border.color: "#000000"
                    border.width: 2
                    radius: 15
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 22
                    font.bold: true
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    internal.resetParameters()
                    root.resetToDefaults()
                }
            }

            Button {
                text: "应用设置"
                Layout.fillWidth: true
                Layout.preferredHeight: 60

                background: Rectangle {
                    color: parent.pressed ? "#333333" : (parent.hovered ? "#777777" : "#555555")
                    border.color: "#000000"
                    border.width: 2
                    radius: 15
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 22
                    font.bold: true
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    internal.applyAllSettings()
                    root.applySettings()
                }
            }
        }
    }

    // 关闭面板的鼠标区域（点击面板外部关闭）
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: {
            mouse.accepted = false
        }
    }
}
