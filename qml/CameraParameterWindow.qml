import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

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
        property var parameterRanges: ({})
        property var supportedParameters: ({})

        // 可观察的参数值属性 - UI绑定到这些属性
        property int brightnessValue: 0
        property int contrastValue: 0
        property int saturationValue: 50
        property int backlightValue: 0
        property int gammaValue: 100
        property int exposureTimeValue: 100
        property int gainValue: 0

        // 清除参数范围缓存
        function clearParameterRanges() {
            parameterRanges = {}
        }

        // 清除支持的参数缓存
        function clearSupportedParameters() {
            supportedParameters = {}
        }

        // 检查参数是否被硬件支持
        function isParameterSupported(propertyName) {
            if (supportedParameters[propertyName] !== undefined) {
                return supportedParameters[propertyName]
            }

            // 动态检查参数支持
            var propertyEnum = getCameraPropertyEnum(propertyName)
            var range = getParameterRange(propertyEnum)

            var isSupported = range &&
                             range.min !== undefined &&
                             range.max !== undefined &&
                             range.min !== range.max &&
                             !isNaN(range.min) &&
                             !isNaN(range.max)

            supportedParameters[propertyName] = isSupported
            console.log("检查参数", propertyName, "支持状态:", isSupported)
            return isSupported
        }

        // 获取当前相机属性枚举值 - 匹配FFI层定义
        function getCameraPropertyEnum(name) {
            var propertyMap = {
                "brightness": 0,
                "contrast": 1,
                "saturation": 2,
                "gain": 3,
                "exposure_time": 4,      // 修改：从"exposure"改为"exposure_time"
                "white_balance": 5,      // 白平衡参数（虽然不使用，但要保持枚举对齐）
                "gamma": 6,              // 修正：gamma对应FFI的6
                "backlight": 7,          // 修正：backlight对应FFI的7（BacklightCompensation）
                "auto_exposure": 8       // 修正：auto_exposure对应FFI的8
            }
            return propertyMap[name] || 0
        }

        function getCurrentParameterValue(propertyEnum) {
            var value
            if (cameraParameterWindow.cameraMode === 1) {
                value = CameraParameterManager.getSingleCameraParameter(propertyEnum)
                console.log("获取单相机参数 propertyEnum:", propertyEnum, "value:", value)
                return value
            } else if (cameraParameterWindow.cameraMode === 2) {
                var leftValue = CameraParameterManager.getLeftCameraParameter(propertyEnum)
                console.log("获取左相机参数 propertyEnum:", propertyEnum, "value:", leftValue)
                if (leftValue !== -1) {
                    return leftValue
                }
                var rightValue = CameraParameterManager.getRightCameraParameter(propertyEnum)
                console.log("获取右相机参数 propertyEnum:", propertyEnum, "value:", rightValue)
                return rightValue
            }
            console.log("无法获取参数值 propertyEnum:", propertyEnum)
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
                console.log("滑块不存在:", propertyName)
                return
            }

            var propertyEnum = getCameraPropertyEnum(propertyName)
            var range = getParameterRange(propertyEnum)

            if (range) {
                // 动态设置滑块范围
                if (range.min !== undefined && range.max !== undefined && range.min !== range.max) {
                    slider.from = range.min
                    slider.to = range.max
                    console.log("设置", propertyName, "范围:", range.min, "-", range.max)
                }
                if (range.step !== undefined && range.step > 0) {
                    slider.stepSize = range.step
                    console.log("设置", propertyName, "步长:", range.step)
                }
            }

            var current = getCurrentParameterValue(propertyEnum)
            if (current !== null && current !== undefined && current !== -1) {
                console.log("更新", propertyName, "值从", slider.value, "到", current)

                // 更新对应的属性值，这会触发UI更新
                switch(propertyName) {
                    case "brightness":
                        internal.brightnessValue = current
                        break
                    case "contrast":
                        internal.contrastValue = current
                        break
                    case "saturation":
                        internal.saturationValue = current
                        break
                    case "backlight":
                        internal.backlightValue = current
                        break
                    case "gamma":
                        internal.gammaValue = current
                        break
                    case "exposure_time":
                        internal.exposureTimeValue = current
                        break
                    case "gain":
                        internal.gainValue = current
                        break
                }

                console.log("设置", propertyName, "当前值:", current)
            } else {
                console.log("无法获取", propertyName, "的当前值")
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

            console.log("同步相机参数，相机模式:", cameraParameterWindow.cameraMode)

            // 清除支持状态缓存，重新检查
            clearSupportedParameters()

            // 检查并应用每个参数，隐藏不支持的参数
            if (isParameterSupported("brightness")) {
                applyParameterToSlider(brightnessSlider, "brightness")
                brightnessSlider.visible = true
            } else {
                brightnessSlider.visible = false
                console.log("亮度参数不被硬件支持，已隐藏")
            }

            if (isParameterSupported("contrast")) {
                applyParameterToSlider(contrastSlider, "contrast")
                contrastSlider.visible = true
            } else {
                contrastSlider.visible = false
                console.log("对比度参数不被硬件支持，已隐藏")
            }

            if (isParameterSupported("saturation")) {
                applyParameterToSlider(saturationSlider, "saturation")
                saturationSlider.visible = true
            } else {
                saturationSlider.visible = false
                console.log("饱和度参数不被硬件支持，已隐藏")
            }

            if (isParameterSupported("backlight")) {
                applyParameterToSlider(backlightSlider, "backlight")
                backlightSlider.visible = true
            } else {
                backlightSlider.visible = false
                console.log("背光补偿参数不被硬件支持，已隐藏")
            }

            if (isParameterSupported("gamma")) {
                applyParameterToSlider(gammaSlider, "gamma")
                gammaSlider.visible = true
            } else {
                gammaSlider.visible = false
                console.log("伽马参数不被硬件支持，已隐藏")
            }

            // 处理曝光相关参数
            var exposureSupported = isParameterSupported("exposure_time")
            if (exposureSupported) {
                applyParameterToSlider(exposureSlider, "exposure_time")
                exposureSlider.visible = true
            } else {
                exposureSlider.visible = false
                console.log("曝光时间参数不被硬件支持，已隐藏")
            }

            // 检查增益参数（通常不被UVC相机支持）
            if (isParameterSupported("gain")) {
                applyParameterToSlider(gainSlider, "gain")
                gainSlider.visible = true
            } else {
                gainSlider.visible = false
                console.log("增益参数不被硬件支持，已隐藏")
            }

            // 处理自动曝光状态
            var autoExposureSupported = isParameterSupported("auto_exposure")
            if (autoExposureSupported) {
                var autoExposureEnum = getCameraPropertyEnum("auto_exposure")
                var autoExposureValue = getCurrentParameterValue(autoExposureEnum)
                if (autoExposureValue !== null && autoExposureValue !== undefined && autoExposureValue !== -1) {
                    var isAutoExposure = autoExposureValue !== 1
                    autoExposureCheck.checked = isAutoExposure
                    autoExposureCheck.visible = true

                    if (typeof exposureSlider !== "undefined" && exposureSlider.visible) {
                        exposureSlider.enabled = !isAutoExposure
                        console.log("自动曝光状态:", isAutoExposure, "曝光滑块启用:", !isAutoExposure)
                    }
                }
            } else {
                autoExposureCheck.visible = false
                console.log("自动曝光参数不被硬件支持，已隐藏")
            }

            syncing = false
            console.log("相机参数同步完成，已隐藏不支持的参数")
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

            // 更新对应的属性值（这样用户手动拖动滑块也会触发响应式更新）
            switch(propertyName) {
                case "brightness":
                    internal.brightnessValue = intValue
                    break
                case "contrast":
                    internal.contrastValue = intValue
                    break
                case "saturation":
                    internal.saturationValue = intValue
                    break
                case "backlight":
                    internal.backlightValue = intValue
                    break
                case "gamma":
                    internal.gammaValue = intValue
                    break
                case "exposure_time":
                    internal.exposureTimeValue = intValue
                    break
                case "gain":
                    internal.gainValue = intValue
                    break
            }

            if (range && (range.current !== intValue)) {
                range.current = intValue
            }
        }

        // 调用FFI设置参数
        function setParameter(propertyName, value) {
            var propertyEnum = getCameraPropertyEnum(propertyName)
            var intValue = Math.round(value)
            var success = false

            // 根据相机模式选择设置哪个相机
            if (cameraParameterWindow.cameraMode === 1) {
                // 单相机模式
                success = CameraParameterManager.setSingleCameraParameter(propertyEnum, intValue)
                if (success) {
                    console.log("设置单相机参数成功:", propertyName, "=", intValue)
                } else {
                    console.error("设置单相机参数失败:", propertyName, "=", intValue)
                }
            } else if (cameraParameterWindow.cameraMode === 2) {
                // 双相机模式 - 同时设置左右相机
                var leftSuccess = CameraParameterManager.setLeftCameraParameter(propertyEnum, intValue)
                var rightSuccess = CameraParameterManager.setRightCameraParameter(propertyEnum, intValue)

                if (leftSuccess && rightSuccess) {
                    console.log("设置左右相机参数成功:", propertyName, "=", intValue)
                    success = true
                } else {
                    console.error("设置相机参数失败:", propertyName, "左:", leftSuccess, "右:", rightSuccess)
                }
            }

            // 如果设置成功，更新对应的属性值（这会触发UI更新）
            if (success) {
                switch(propertyName) {
                    case "brightness":
                        internal.brightnessValue = intValue
                        break
                    case "contrast":
                        internal.contrastValue = intValue
                        break
                    case "saturation":
                        internal.saturationValue = intValue
                        break
                    case "backlight":
                        internal.backlightValue = intValue
                        break
                    case "gamma":
                        internal.gammaValue = intValue
                        break
                    case "exposure_time":
                        internal.exposureTimeValue = intValue
                        break
                    case "gain":
                        internal.gainValue = intValue
                        break
                }
            }

            return success
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

            // 重置后强制同步UI状态 - 先清除缓存再同步
            Qt.callLater(function() {
                // 清除所有缓存，强制重新读取相机当前值
                clearParameterRanges()
                clearSupportedParameters()

                // 等待一小段时间让相机参数生效
                Qt.callLater(function() {
                    syncParametersFromCamera()

                    // 设置UI状态
                    if (typeof autoExposureCheck !== "undefined") {
                        autoExposureCheck.checked = true
                    }
                    if (typeof exposureSlider !== "undefined") {
                        exposureSlider.enabled = false
                    }

                    console.log("重置完成，UI已同步到相机当前值")
                })
            })
        }

        // 重置单个相机参数到默认值（仅重置硬件支持的参数）
        function resetSingleCameraParameters() {
            console.log("开始重置相机参数到固件默认值")

            // 先设置自动曝光模式（如果支持）- 重置到自动曝光
            if (isParameterSupported("auto_exposure")) {
                var autoExposureSuccess = setParameter("auto_exposure", 3)
                console.log("重置自动曝光:", autoExposureSuccess ? "成功" : "失败")
            }

            // 只重置基础参数（不包括曝光时间，因为重置时启用自动曝光）
            var parameterNames = [
                "brightness",
                "contrast",
                "saturation",
                "backlight",
                "gamma"
            ]

            for (var i = 0; i < parameterNames.length; i++) {
                var paramName = parameterNames[i]

                // 检查参数是否被硬件支持
                if (!isParameterSupported(paramName)) {
                    console.log("跳过不支持的参数:", paramName)
                    continue
                }

                var propertyEnum = getCameraPropertyEnum(paramName)

                // 从相机获取参数范围和默认值
                var range
                if (cameraParameterWindow.cameraMode === 1) {
                    range = CameraParameterManager.getSingleCameraParameterRange(propertyEnum)
                } else {
                    range = CameraParameterManager.getLeftCameraParameterRange(propertyEnum)
                }

                if (range && range.default_value !== undefined) {
                    // 验证默认值是否合理，如果不合理则使用合理的默认值
                    var defaultValue = range.default_value
                    if (paramName === "exposure_time" && (defaultValue < range.min || defaultValue > range.max)) {
                        console.warn("曝光时间默认值不合理:", defaultValue, "使用合理默认值")
                        defaultValue = Math.round((range.min + range.max) / 2)
                    }

                    // 使用相机固件默认值
                    var success = setParameter(paramName, defaultValue)
                    console.log("重置", paramName, "到默认值", defaultValue, ":", success ? "成功" : "失败")

                    // 显示参数信息用于调试
                    console.log("  - 当前值:", range.current, "默认值:", defaultValue, "范围:", range.min + "-" + range.max)
                } else {
                    console.warn("无法获取", paramName, "的默认值信息")
                }
            }

            console.log("相机参数重置完成")
        }

        // 强制刷新所有参数信息（包括范围、默认值、当前值）
        function forceRefreshAllParameters() {
            console.log("强制刷新相机参数信息")

            // 清除所有内部缓存
            clearParameterRanges()
            clearSupportedParameters()

            // 重新同步所有参数，这会强制从相机重新获取范围和当前值
            syncParametersFromCamera()
        }

        // 获取参数的合理默认值（备用，当无法获取相机默认值时使用）
        function getReasonableDefaultValue(paramName) {
            var defaults = {
                "brightness": 0,      // 默认值: 0
                "contrast": 2,        // 默认值: 2
                "saturation": 64,     // 默认值: 64
                "backlight": 1,       // 默认值: 1 (backlight_compensation)
                "gamma": 100,        // 默认值: 100
                "gain": 0            // 增益不在支持列表中，设为0
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
                        value: internal.brightnessValue
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
                                color: "#2196F3"
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
                        value: internal.contrastValue
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
                        value: internal.saturationValue
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
                        value: internal.backlightValue
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
                        value: internal.gammaValue
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
                        value: internal.exposureTimeValue
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
                        value: internal.gainValue
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
        console.log("相机模式已切换到:", cameraMode)
        // 强制重新同步所有参数，包括范围和默认值
        Qt.callLater(function() {
            internal.forceRefreshAllParameters()
        })
    }

    // 窗口事件处理
    onVisibleChanged: {
        if (visible) {
            console.log("相机参数设置窗口已打开")
            raise()
            requestActivate()

            // 强制清除缓存并重新同步参数
            internal.clearParameterRanges()
            internal.clearSupportedParameters()
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
