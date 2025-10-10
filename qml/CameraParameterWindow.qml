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
        property bool syncing: true
        property bool autoWhiteBalanceSupported: true

        // 获取当前相机属性枚举值 - 匹配C FFI枚举定义
        function getCameraPropertyEnum(name) {
            var propertyMap = {
                "brightness": 0,
                "contrast": 1,
                "saturation": 2,
                "gain": 3,
                "exposure_time": 4,
                // white_balance: 5 - 不支持，已移除
                "gamma": 6,
                "backlight": 7,
                "auto_exposure": 8
            }
            return propertyMap[name] || 0
        }

        function getCurrentParameterValue(propertyEnum) {
            if (cameraParameterWindow.cameraMode === 1) {
                return CameraParameterManager.getSingleCameraParameter(propertyEnum)
            } else if (cameraParameterWindow.cameraMode === 2) {
                var leftValue = CameraParameterManager.getLeftCameraParameter(propertyEnum)
                if (leftValue !== -1) {
                    return leftValue
                }
                return CameraParameterManager.getRightCameraParameter(propertyEnum)
            }
            return null
        }

        function getParameterRange(propertyEnum) {
            if (cameraParameterWindow.cameraMode === 1) {
                return CameraParameterManager.getSingleCameraParameterRange(propertyEnum)
            } else if (cameraParameterWindow.cameraMode === 2) {
                var leftRange = CameraParameterManager.getLeftCameraParameterRange(propertyEnum)
                if (leftRange && leftRange.min !== undefined && leftRange.max !== undefined && leftRange.max > leftRange.min) {
                    return leftRange
                }
                return CameraParameterManager.getRightCameraParameterRange(propertyEnum)
            }
            return null
        }

        function applyParameterToSlider(slider, propertyName) {
            if (!slider) {
                return
            }

            var propertyEnum = getCameraPropertyEnum(propertyName)
            var range = getParameterRange(propertyEnum)

            if (range) {
                if (range.min !== undefined && range.max !== undefined && range.min !== range.max) {
                    slider.from = range.min
                    slider.to = range.max
                }
                if (range.step !== undefined && range.step > 0) {
                    slider.stepSize = range.step
                }
            }

            var current = getCurrentParameterValue(propertyEnum)
            if (current !== null && current !== undefined && current !== -1) {
                slider.value = current
            }
        }

        function syncParametersFromCamera() {
            syncing = true
            if (typeof brightnessSlider === "undefined") {
                syncing = false
                return
            }
            if (cameraParameterWindow.cameraMode === 0) {
                syncing = false
                return
            }

            applyParameterToSlider(brightnessSlider, "brightness")
            if (typeof contrastSlider !== "undefined") applyParameterToSlider(contrastSlider, "contrast")
            if (typeof saturationSlider !== "undefined") applyParameterToSlider(saturationSlider, "saturation")
            if (typeof backlightSlider !== "undefined") applyParameterToSlider(backlightSlider, "backlight")
            if (typeof gammaSlider !== "undefined") applyParameterToSlider(gammaSlider, "gamma")
            if (typeof exposureSlider !== "undefined") applyParameterToSlider(exposureSlider, "exposure_time")  // 修改：exposure -> exposure_time
            if (typeof gainSlider !== "undefined") applyParameterToSlider(gainSlider, "gain")

            var autoExposureEnum = getCameraPropertyEnum("auto_exposure")
            var autoExposureValue = getCurrentParameterValue(autoExposureEnum)
            if (autoExposureValue !== null && autoExposureValue !== undefined && autoExposureValue !== -1) {
                var isAutoExposure = autoExposureValue !== 1
                autoExposureCheck.checked = isAutoExposure
                if (typeof exposureSlider !== "undefined") {
                    exposureSlider.enabled = !isAutoExposure
                }
            }
            syncing = false
        }

        function handleSliderChange(propertyName, value) {
            if (syncing) {
                return
            }
            var intValue = Math.round(value)
            var propertyEnum = getCameraPropertyEnum(propertyName)
            
            // 调试日志：打印滑杆原始值、枚举值、四舍五入后的值
            console.log("🔧 handleSliderChange:", propertyName, 
                       "原始value =", value, 
                       "四舍五入 =", intValue,
                       "枚举ID =", propertyEnum)
            
            var current = getCurrentParameterValue(propertyEnum)
            if (current !== null && current !== undefined && current === intValue) {
                return
            }

            var range = getParameterRange(propertyEnum)
            if (range) {
                var min = range.min !== undefined ? range.min : intValue
                var max = range.max !== undefined ? range.max : intValue
                intValue = Math.max(min, Math.min(max, intValue))
                console.log("🔧 经过范围限制:", propertyName, "最终值 =", intValue, "范围:", min, "-", max)
            }

            var applied = setParameter(propertyName, intValue)
            if (!applied) {
                console.error("参数同步失败:", propertyName, intValue)
                return
            }

            if (range && (range.current !== intValue)) {
                range.current = intValue
            }
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
                return success
            } else if (cameraParameterWindow.cameraMode === 2) {
                // 双相机模式 - 同时设置左右相机
                var leftSuccess = CameraParameterManager.setLeftCameraParameter(propertyEnum, value)
                var rightSuccess = CameraParameterManager.setRightCameraParameter(propertyEnum, value)

                if (leftSuccess && rightSuccess) {
                    console.log("设置左右相机参数成功:", propertyName, "=", value)
                } else {
                    console.error("设置相机参数失败:", propertyName, "左:", leftSuccess, "右:", rightSuccess)
                }
                return leftSuccess && rightSuccess
            }
            return false
        }

        // 重置参数到默认值（逐个参数单独设置）
        function resetParameters() {
            console.log("重置相机参数到默认值")

            // 根据相机模式选择重置哪个相机
            if (cameraParameterWindow.cameraMode === 1) {
                // 单相机模式 - 重置单相机
                resetSingleCameraParameters()
            } else if (cameraParameterWindow.cameraMode === 2) {
                // 双相机模式 - 分别重置左右相机
                resetSingleCameraParameters()
                resetSingleCameraParameters() // 第二次调用重置右相机
            }

            // 重置后同步UI状态
            Qt.callLater(function() {
                syncParametersFromCamera()

                // 设置UI状态
                if (typeof autoExposureCheck !== "undefined") {
                    autoExposureCheck.checked = true
                }
                if (typeof exposureSlider !== "undefined") {
                    exposureSlider.enabled = false
                }
            })
        }

        // 重置单个相机参数到默认值
        function resetSingleCameraParameters() {
            // 逐个设置参数到默认值，减少每个操作的影响
            var parameterNames = [
                "brightness",
                "contrast",
                "saturation",
                "backlight",
                "gamma",
                "gain"
            ]

            // 先设置自动曝光模式
            setParameter("auto_exposure", 3)

            // 然后设置其他参数
            for (var i = 0; i < parameterNames.length; i++) {
                var paramName = parameterNames[i]
                var propertyEnum = getCameraPropertyEnum(paramName)

                // 尝试从相机获取默认值
                var range
                if (cameraParameterWindow.cameraMode === 1) {
                    range = CameraParameterManager.getSingleCameraParameterRange(propertyEnum)
                } else {
                    range = CameraParameterManager.getLeftCameraParameterRange(propertyEnum)
                }

                if (range && range.default_value !== undefined) {
                    // 如果有默认值，使用默认值
                    setParameter(paramName, range.default_value)
                } else {
                    // 如果没有默认值，使用合理的默认值
                    var defaultValue = getReasonableDefaultValue(paramName)
                    setParameter(paramName, defaultValue)
                }
            }
        }

        // 获取参数的合理默认值
        function getReasonableDefaultValue(paramName) {
            var defaults = {
                "brightness": 0,
                "contrast": 0,
                "saturation": 50,
                "backlight": 0,
                "gamma": 100,
                "gain": 0
            }
            return defaults[paramName] || 0
        }

        // 从FFI加载默认值
        function loadDefaultValues(cameraType) {
            var properties = [
                "brightness", "contrast", "saturation",
                "backlight", "gamma", "exposure_time"  // 修改：exposure -> exposure_time
            ]

            syncing = true

            var sliders = {}
            if (typeof brightnessSlider !== "undefined") sliders.brightness = brightnessSlider
            if (typeof contrastSlider !== "undefined") sliders.contrast = contrastSlider
            if (typeof saturationSlider !== "undefined") sliders.saturation = saturationSlider
            if (typeof backlightSlider !== "undefined") sliders.backlight = backlightSlider
            if (typeof gammaSlider !== "undefined") sliders.gamma = gammaSlider
            if (typeof exposureSlider !== "undefined") sliders.exposure_time = exposureSlider  // 修改：exposure -> exposure_time

            for (var i = 0; i < properties.length; i++) {
                var propName = properties[i]
                var control = sliders[propName]
                if (!control) {
                    continue
                }

                var propEnum = getCameraPropertyEnum(propName)
                var range

                if (cameraType === "single") {
                    range = CameraParameterManager.getSingleCameraParameterRange(propEnum)
                } else if (cameraType === "left") {
                    range = CameraParameterManager.getLeftCameraParameterRange(propEnum)
                } else if (cameraType === "right") {
                    range = CameraParameterManager.getRightCameraParameterRange(propEnum)
                }

                if (range && range.default_value !== undefined) {
                    if (!(range.min === 0 && range.max === 0 && range.default_value === 0 && range.current === 0)) {
                        control.value = range.default_value
                    }
                }
            }
            // 自动曝光和自动白平衡状态将在后续设置中处理
            syncing = false
        }
    }

    Timer {
        id: parameterPoller
        interval: 1000
        repeat: true
        running: cameraParameterWindow.visible
        onTriggered: internal.syncParametersFromCamera()
    }

    // 窗口内容
    content: ColumnLayout {
        anchors.fill: parent
        spacing: 20

        // 参数区域 - 横向两列布局
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 30

            // 左侧列 - 基础参数
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width / 2 - 15
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
                            if (!internal.syncing) {
                                internal.handleSliderChange("brightness", value)
                            }
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
                            if (!internal.syncing) {
                                internal.handleSliderChange("contrast", value)
                            }
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
                            if (!internal.syncing) {
                                internal.handleSliderChange("saturation", value)
                            }
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
                            if (!internal.syncing) {
                                internal.handleSliderChange("backlight", value)
                            }
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
                            if (!internal.syncing) {
                                internal.handleSliderChange("gamma", value)
                            }
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
                Layout.preferredWidth: parent.width / 2 - 15
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

                    onToggled: {
                        if (internal.syncing) {
                            if (typeof exposureSlider !== "undefined") {
                                exposureSlider.enabled = !checked
                            }
                            return
                        }

                        var targetValue = checked ? 3 : 1
                        var success = internal.setParameter("auto_exposure", targetValue)
                        if (!success) {
                            console.error("切换自动曝光失败, 恢复原状态")
                            internal.syncing = true
                            autoExposureCheck.checked = !checked
                            internal.syncing = false
                            if (typeof exposureSlider !== "undefined") {
                                exposureSlider.enabled = !autoExposureCheck.checked
                            }
                            Qt.callLater(function() { internal.syncParametersFromCamera() })
                            return
                        }

                        if (typeof exposureSlider !== "undefined") {
                            exposureSlider.enabled = !checked
                            
                            // 切换到自动曝光时，将滑杆重置到默认值
                            if (checked) {
                                internal.syncing = true
                                exposureSlider.value = 3  // 重置到默认值
                                internal.syncing = false
                                console.log("切换到自动曝光，曝光时间滑杆重置为默认值: 3")
                            } else {
                                // 切换到手动曝光时，设置当前滑杆值
                                internal.handleSliderChange("exposure_time", exposureSlider.value)
                            }
                        }

                        Qt.callLater(function() {
                            internal.syncParametersFromCamera()
                        })
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
                            if (!internal.syncing && !autoExposureCheck.checked) {
                                internal.handleSliderChange("exposure_time", value)
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
                            if (!internal.syncing) {
                                internal.handleSliderChange("gain", value)
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
                }
            }
            }

        Connections {
            target: CameraParameterManager
            function onParameterChanged() {
                internal.syncParametersFromCamera()
            }
        }
    }

    Component.onCompleted: {
        cameraMode = Qt.binding(function() { return CameraManager.cameraMode })
        internal.syncParametersFromCamera()
        internal.syncing = false
    }

    onCameraModeChanged: {
        internal.syncParametersFromCamera()
    }

    // 窗口事件处理
    onVisibleChanged: {
        if (visible) {
            console.log("相机参数设置窗口已打开")
            raise()
            requestActivate()
            internal.syncParametersFromCamera()
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
        internal.syncParametersFromCamera()
    }
}
