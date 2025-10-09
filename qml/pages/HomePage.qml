import QtQuick 2.15
import QtQuick.Controls 2.15
import RustSmartScope.Logger 1.0
import RustSmartScope.Video 1.0

Rectangle {
    id: homePage
    color: "#2E3440"  // 深色背景

    signal backRequested()
    signal navigateRequested(string pageType)

    // 使用原生QPixmap渲染的视频显示组件
    VideoDisplay {
        id: videoDisplay
        anchors.fill: parent
    }

    Component.onCompleted: {
        Logger.info("HomePage initialized with VideoDisplay")

        // 连接CameraManager的QPixmap信号到VideoDisplay
        if (CameraManager) {
            CameraManager.leftPixmapUpdated.connect(videoDisplay.updateFrame)
            Logger.info("Connected CameraManager.leftPixmapUpdated to VideoDisplay")

            // 页面加载时自动启动相机
            if (!CameraManager.cameraRunning) {
                Logger.info("Starting camera...")
                CameraManager.startCamera()
            }
        } else {
            Logger.error("CameraManager not available!")
        }
    }

    Component.onDestruction: {
        // 清理连接
        if (CameraManager) {
            CameraManager.leftPixmapUpdated.disconnect(videoDisplay.updateFrame)
        }
    }
}