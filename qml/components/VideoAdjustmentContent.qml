import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

// 视频调整内容组件
Item {
    id: root

    // 当前变换状态
    property int rotationDegrees: 0
    property bool flipHorizontal: false
    property bool flipVertical: false
    property bool invertColors: false

    // 曝光控制状态
    property bool autoExposureEnabled: true
    property int exposurePresetIndex: 0
    property var exposurePresets: [50, 100, 300, 500, 1000, 1500]

    // 亮度控制状态
    property int brightnessLevel: 0
    property var brightnessLevels: [0, 25, 50, 75, 100]

    // 信号
    signal transformationChanged()
    signal exposureChanged(int mode, int value)
    signal brightnessChanged(int value)
    signal resetRequested()
    signal advancedSettingsRequested()

    // 初始化相机参数控制
    function initializeCameraParameters() {
        // 确保相机参数管理器已初始化
        console.log("初始化相机参数控制，相机模式:", CameraManager.cameraMode)

        // 检查相机模式
        if (CameraManager.cameraMode === 0) {
            console.log("相机未连接或模式无效")
            return false
        }

        // 通过获取参数范围来初始化相机参数控制
        try {
            var brightnessRange = null
            var exposureRange = null

            if (CameraManager.cameraMode === 1) {
                // 单相机模式
                brightnessRange = CameraParameterManager.getSingleCameraParameterRange(0) // brightness
                exposureRange = CameraParameterManager.getSingleCameraParameterRange(8) // auto_exposure
                console.log("单相机模式参数范围检查 - 亮度:", brightnessRange, "自动曝光:", exposureRange)
            } else if (CameraManager.cameraMode === 2) {
                // 双相机模式
                var leftBrightnessRange = CameraParameterManager.getLeftCameraParameterRange(0)
                var rightBrightnessRange = CameraParameterManager.getRightCameraParameterRange(0)
                var leftExposureRange = CameraParameterManager.getLeftCameraParameterRange(8)
                var rightExposureRange = CameraParameterManager.getRightCameraParameterRange(8)

                console.log("双相机模式参数范围检查:")
                console.log("  左相机亮度范围:", leftBrightnessRange)
                console.log("  右相机亮度范围:", rightBrightnessRange)
                console.log("  左相机曝光范围:", leftExposureRange)
                console.log("  右相机曝光范围:", rightExposureRange)

                // 以左相机为准
                brightnessRange = leftBrightnessRange
                exposureRange = leftExposureRange
            }

            // 检查参数范围是否有效
            var brightnessValid = brightnessRange && brightnessRange.min !== undefined && brightnessRange.max !== undefined
            var exposureValid = exposureRange && exposureRange.min !== undefined && exposureRange.max !== undefined

            if (brightnessValid && exposureValid) {
                console.log("相机参数控制初始化成功")
                return true
            } else {
                console.log("相机参数初始化部分失败 - 亮度有效:", brightnessValid, "曝光有效:", exposureValid)
                return false
            }
        } catch (error) {
            console.error("相机参数初始化异常:", error)
            return false
        }
    }

    // 设置相机参数的统一方法
    function setCameraParameter(propertyName, value) {
        // 先确保初始化
        if (!initializeCameraParameters()) {
            console.error("相机参数未初始化，无法设置:", propertyName)
            return false
        }

        // 获取参数枚举值
        var propertyEnum = getCameraPropertyEnum(propertyName)
        if (propertyEnum === undefined) {
            console.error("未知的相机参数:", propertyName)
            return false
        }

        var intValue = Math.round(value)
        var success = false

        // 根据相机模式选择设置方法
        if (CameraManager.cameraMode === 1) {
            // 单相机模式
            success = CameraParameterManager.setSingleCameraParameter(propertyEnum, intValue)
            if (success) {
                console.log("设置单相机参数成功:", propertyName, "=", intValue)
            } else {
                console.error("设置单相机参数失败:", propertyName, "=", intValue)
            }
        } else if (CameraManager.cameraMode === 2) {
            // 双相机模式 - 同时设置左右相机
            var leftSuccess = CameraParameterManager.setLeftCameraParameter(propertyEnum, intValue)
            var rightSuccess = CameraParameterManager.setRightCameraParameter(propertyEnum, intValue)
            success = leftSuccess && rightSuccess
            if (success) {
                console.log("设置左右相机参数成功:", propertyName, "=", intValue)
            } else {
                console.error("设置相机参数失败:", propertyName, "左:", leftSuccess, "右:", rightSuccess)
            }
        } else {
            console.error("无效的相机模式:", CameraManager.cameraMode)
            return false
        }

        return success
    }

    // 获取相机参数枚举值
    function getCameraPropertyEnum(propertyName) {
        var propertyMap = {
            "brightness": 0,
            "contrast": 1,
            "saturation": 2,
            "auto_exposure": 8,
            "exposure_time_absolute": 4,
            // 可以根据需要添加更多参数
        }
        return propertyMap[propertyName]
    }

    // 按钮网格布局（3x3）
    Grid {
        anchors.centerIn: parent
        columns: 3
        rows: 3
        columnSpacing: 20
        rowSpacing: 18

        // 第一行
        // 旋转按钮
        OperationButton {
            text: "旋转"
            iconSource: "qrc:/icons/rotate.svg"
            onClicked: {
                VideoTransformManager.applyRotation()
                rotationDegrees = VideoTransformManager.rotationDegrees
                applyTransformations()
            }
        }

        // 水平翻转按钮
        OperationButton {
            text: "水平翻转"
            iconSource: "qrc:/icons/horizontal_filp.svg"
            isActive: flipHorizontal
            onClicked: {
                VideoTransformManager.toggleFlipHorizontal()
                flipHorizontal = VideoTransformManager.flipHorizontal
                applyTransformations()
            }
        }

        // 垂直翻转按钮
        OperationButton {
            text: "垂直翻转"
            iconSource: "qrc:/icons/vertical_filp.svg"
            isActive: flipVertical
            onClicked: {
                VideoTransformManager.toggleFlipVertical()
                flipVertical = VideoTransformManager.flipVertical
                applyTransformations()
            }
        }

        // 第二行
        // 反色按钮
        OperationButton {
            text: "反色"
            iconSource: "qrc:/icons/invert_color.svg"
            isActive: invertColors
            onClicked: {
                VideoTransformManager.toggleInvert()
                invertColors = VideoTransformManager.invertColors
                applyTransformations()
            }
        }

        // 自动曝光按钮
        OperationButton {
            id: exposureButton
            text: autoExposureEnabled ? "自动曝光" : ("曝光 " + exposurePresets[exposurePresetIndex])
            iconSource: autoExposureEnabled ? "qrc:/icons/auto_exposure.svg" : "qrc:/icons/exposure.svg"
            onClicked: {
                cycleExposure()
            }
        }

        // 亮度按钮
        OperationButton {
            id: brightnessButton
            text: "亮度 " + brightnessLevels[brightnessLevel] + "%"
            iconSource: "qrc:/icons/brightness.svg"
            onClicked: {
                cycleBrightness()
            }
        }

        // 第三行
        // 畸变校正按钮
        OperationButton {
            text: "畸变校正"
            iconSource: "qrc:/icons/distortion.svg"
            isActive: VideoTransformManager.distortionCorrectionEnabled
            onClicked: {
                toggleDistortionCorrection()
            }
        }

        // 还原按钮
        OperationButton {
            text: "还原"
            iconSource: "qrc:/icons/restore.svg"
            buttonColor: Qt.rgba(0.7, 0.2, 0.2, 0.6)
            hoverColor: Qt.rgba(0.85, 0.25, 0.25, 0.8)
            onClicked: {
                resetAll()
            }
        }

        // 高级模式按钮
        OperationButton {
            text: "高级模式"
            iconSource: "qrc:/icons/advanced_settings.svg"
            onClicked: {
                advancedSettingsRequested()
            }
        }
    }

    // 辅助函数：获取当前状态文本
    function getCurrentStatusText() {
        var status = []

        if (rotationDegrees !== 0) {
            status.push("旋转: " + rotationDegrees + "°")
        }

        if (flipHorizontal) {
            status.push("水平翻转")
        }

        if (flipVertical) {
            status.push("垂直翻转")
        }

        if (invertColors) {
            status.push("反色")
        }

        if (status.length === 0) {
            return "未应用任何变换"
        }

        return "当前变换: " + status.join(" | ")
    }

    // 应用变换
    function applyTransformations() {
        console.log("应用变换:")
        console.log("  旋转:", rotationDegrees, "度")
        console.log("  水平翻转:", flipHorizontal)
        console.log("  垂直翻转:", flipVertical)
        console.log("  反色:", invertColors)
        transformationChanged()
        // 变换会自动在C++端应用，这里仅用于日志
    }

    // 曝光循环：自动曝光 -> 50 -> 100 -> 300 -> 500 -> 1000 -> 1500 -> 自动曝光
    function cycleExposure() {
        if (autoExposureEnabled) {
            // 从自动切换到手动第一档（50）
            autoExposureEnabled = false
            exposurePresetIndex = 0
            console.log("曝光模式: 手动", exposurePresets[exposurePresetIndex])
            exposureChanged(0, exposurePresets[exposurePresetIndex])

            // 设置手动曝光模式
            var autoExposureSuccess = setCameraParameter("auto_exposure", 1) // 1=Manual
            if (autoExposureSuccess) {
                console.log("成功设置手动曝光模式")
                // 设置曝光时间值
                var exposureSuccess = setCameraParameter("exposure_time_absolute", exposurePresets[exposurePresetIndex])
                if (exposureSuccess) {
                    console.log("成功设置手动曝光:", exposurePresets[exposurePresetIndex])
                } else {
                    console.error("设置曝光时间失败")
                }
            } else {
                console.error("设置手动曝光模式失败")
            }
        } else {
            // 手动模式下循环档位
            exposurePresetIndex++
            if (exposurePresetIndex >= exposurePresets.length) {
                // 超过最后一档，回到自动曝光
                autoExposureEnabled = true
                exposurePresetIndex = 0
                console.log("曝光模式: 自动")
                exposureChanged(1, 0)

                // 设置自动曝光模式
                var autoSuccess = setCameraParameter("auto_exposure", 3) // 3=Auto
                if (autoSuccess) {
                    console.log("成功设置自动曝光")
                } else {
                    console.error("设置自动曝光失败")
                }
            } else {
                console.log("曝光值:", exposurePresets[exposurePresetIndex])
                exposureChanged(0, exposurePresets[exposurePresetIndex])

                // 设置新的曝光时间值
                var success = setCameraParameter("exposure_time_absolute", exposurePresets[exposurePresetIndex])
                if (success) {
                    console.log("成功设置曝光值:", exposurePresets[exposurePresetIndex])
                } else {
                    console.error("设置曝光值失败")
                }
            }
        }
    }

    // 亮度循环：0% -> 25% -> 50% -> 75% -> 100% -> 0%
    function cycleBrightness() {
        brightnessLevel = (brightnessLevel + 1) % brightnessLevels.length
        var brightnessPercent = brightnessLevels[brightnessLevel]
        console.log("亮度:", brightnessPercent + "%")
        brightnessChanged(brightnessPercent)

        // 将百分比转换为相机亮度值（范围：-64到64）
        // 0% -> -64, 50% -> 0, 100% -> 64
        var brightnessValue = Math.round((brightnessPercent / 100.0) * 128 - 64)

        // 添加调试信息
        console.log("计算出的亮度值:", brightnessValue, "从百分比:", brightnessPercent + "%")

        // 使用新的参数设置方法
        var success = setCameraParameter("brightness", brightnessValue)
        if (success) {
            console.log("成功设置亮度:", brightnessValue, "(", brightnessPercent + "% )")
        } else {
            console.error("设置亮度失败")
        }
    }

    // 畸变校正开关
    function toggleDistortionCorrection() {
        VideoTransformManager.toggleDistortionCorrection()
        console.log("畸变校正: ", VideoTransformManager.distortionCorrectionEnabled ? "启用" : "禁用")
    }

    // 还原所有设置
    function resetAll() {
        VideoTransformManager.resetAll()
        rotationDegrees = VideoTransformManager.rotationDegrees
        flipHorizontal = VideoTransformManager.flipHorizontal
        flipVertical = VideoTransformManager.flipVertical
        invertColors = VideoTransformManager.invertColors
        autoExposureEnabled = true
        exposurePresetIndex = 0
        brightnessLevel = 0
        console.log("已还原所有设置")

        // 还原相机参数到默认值
        // 设置自动曝光模式
        var autoSuccess = setCameraParameter("auto_exposure", 3) // 3=Auto
        if (autoSuccess) {
            console.log("已还原自动曝光")
        } else {
            console.error("还原自动曝光失败")
        }

        // 还原亮度到0（相机默认值）
        var brightnessSuccess = setCameraParameter("brightness", 0) // 0=默认值
        if (brightnessSuccess) {
            console.log("已还原亮度到默认值0")
        } else {
            console.error("还原亮度失败")
        }

        applyTransformations()
        resetRequested()
    }
}