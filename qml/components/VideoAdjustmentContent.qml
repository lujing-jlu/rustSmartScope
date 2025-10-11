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
            var autoExposureSuccess = CameraParameterManager.setSingleCameraParameter(8, 1) // 8=AutoExposure, 1=Manual
            if (autoExposureSuccess) {
                // 设置曝光时间值
                var exposureSuccess = CameraParameterManager.setSingleCameraParameter(4, exposurePresets[exposurePresetIndex]) // 4=ExposureTime
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
                var autoSuccess = CameraParameterManager.setSingleCameraParameter(8, 3) // 8=AutoExposure, 3=Auto
                if (autoSuccess) {
                    console.log("成功设置自动曝光")
                } else {
                    console.error("设置自动曝光失败")
                }
            } else {
                console.log("曝光值:", exposurePresets[exposurePresetIndex])
                exposureChanged(0, exposurePresets[exposurePresetIndex])

                // 设置新的曝光时间值
                var success = CameraParameterManager.setSingleCameraParameter(4, exposurePresets[exposurePresetIndex]) // 4=ExposureTime
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

        // 设置相机亮度
        var success = CameraParameterManager.setSingleCameraParameter(0, brightnessValue) // 0=Brightness
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
        CameraParameterManager.setSingleCameraParameter(8, 3) // 8=AutoExposure, 3=Auto
        console.log("已还原自动曝光")

        // 还原亮度到0（相机默认值）
        CameraParameterManager.setSingleCameraParameter(0, 0) // 0=Brightness, 0=默认值
        console.log("已还原亮度到默认值0")

        applyTransformations()
        resetRequested()
    }
}