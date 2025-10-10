import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"

// 相机参数设置窗口 - 使用通用弹窗组件
GlassPopupWindow {
    id: cameraParameterWindow

    // 设置窗口属性
    windowTitle: "相机参数设置"
    windowWidth: 600
    windowHeight: 900

    // 相机参数面板内容
    content: CameraParameterPanel {
        id: parameterPanel

        cameraMode: CameraManager.cameraMode

        onApplySettings: {
            console.log("应用相机参数设置")
        }

        onResetToDefaults: {
            console.log("重置相机参数到默认值")
            // 调用FFI重置
            if (parameterPanel.cameraMode === 1) {
                CameraParameterManager.resetSingleCameraParameters()
            } else if (parameterPanel.cameraMode === 2) {
                CameraParameterManager.resetLeftCameraParameters()
                CameraParameterManager.resetRightCameraParameters()
            }
        }

        onPanelClosed: {
            cameraParameterWindow.close()
        }
    }

    // 窗口事件处理
    onOpened: {
        console.log("相机参数设置窗口已打开")
        raise()
        requestActivate()
    }

    onClosed: {
        console.log("相机参数设置窗口已关闭")
    }

    // 重写 show 方法确保窗口正确显示
    function show() {
        visible = true
        raise()
        requestActivate()
    }
}
