import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"
import RustSmartScope.Logger 1.0

// 视频操作窗口 - 使用通用弹窗组件
GlassPopupWindow {
    id: videoOperationsWindow

    // 设置窗口属性
    windowTitle: "画面调整"
    windowWidth: 1100
    windowHeight: 850

    // 添加信号
    signal openCameraParameters()

    // 视频调整内容
    content: VideoAdjustmentContent {
        id: videoContent

        // 连接信号
        onAdvancedSettingsRequested: {
            Logger.info("打开高级设置面板")
            videoOperationsWindow.close()
            // 发出信号打开相机参数窗口
            videoOperationsWindow.openCameraParameters()
        }
    }

    // 窗口事件处理
    onOpened: {
        Logger.info("画面调整窗口已打开")
        raise()
        requestActivate()
    }

    onClosed: {
        Logger.info("画面调整窗口已关闭")
    }

    // 重写 show 方法确保窗口正确显示
    function show() {
        visible = true
        raise()
        requestActivate()
    }
}
