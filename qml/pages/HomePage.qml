import QtQuick 2.15
import QtQuick.Controls 2.15
import RustSmartScope.Logger 1.0
import RustSmartScope.Video 1.0

Rectangle {
    id: homePage
    color: "#000000"  // 纯黑色背景

    signal backRequested()
    signal navigateRequested(string pageType)

    // 相机模式枚举（与C++端对应）
    readonly property int noCamera: 0
    readonly property int singleCamera: 1
    readonly property int stereoCamera: 2

    // 使用原生QPixmap渲染的视频显示组件
    VideoDisplay {
        id: videoDisplay
        anchors.fill: parent
    }

    // 监听相机模式变化，切换显示的信号源
    Connections {
        target: CameraManager
        function onCameraModeChanged() {
            updateVideoSource()
        }
    }

    function updateVideoSource() {
        if (!CameraManager) return

        var mode = CameraManager.cameraMode

        // 断开所有之前的连接
        try {
            CameraManager.leftPixmapUpdated.disconnect(videoDisplay.updateFrame)
        } catch(e) {}
        try {
            CameraManager.singlePixmapUpdated.disconnect(videoDisplay.updateFrame)
        } catch(e) {}

        // 根据模式连接对应的信号
        if (mode === stereoCamera) {
            // 双目模式：显示左相机
            CameraManager.leftPixmapUpdated.connect(videoDisplay.updateFrame)
        } else if (mode === singleCamera) {
            // 单目模式：显示单相机
            CameraManager.singlePixmapUpdated.connect(videoDisplay.updateFrame)
        } else {
            // 无相机模式：清空显示
            videoDisplay.clear()
        }
    }

    Component.onCompleted: {
        // 连接CameraManager信号
        if (CameraManager) {
            // 初始化视频源
            updateVideoSource()

            // 页面加载时自动启动相机
            if (!CameraManager.cameraRunning) {
                CameraManager.startCamera()
            }
        } else {
            Logger.error("CameraManager not available!")
        }
    }

    Component.onDestruction: {
        // 清理所有连接
        if (CameraManager) {
            try {
                CameraManager.leftPixmapUpdated.disconnect(videoDisplay.updateFrame)
            } catch(e) {}
            try {
                CameraManager.singlePixmapUpdated.disconnect(videoDisplay.updateFrame)
            } catch(e) {}
        }
    }
}