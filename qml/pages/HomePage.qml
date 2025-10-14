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

    // 左上角画中画缩略图（原图）
    Rectangle {
        id: pipContainer
        // 放大到1.2倍，根据旋转与当前帧宽高比自适应
        // 规则：未旋转时以宽为基准计算高；旋转90/270度时以高为基准计算宽，避免高度过大
        property real pipBase: Math.min(parent.width * (0.22 * 1.2), 340 * 1.2)
        property bool rotated90: (VideoTransformManager && (VideoTransformManager.rotationDegrees % 180 !== 0))
        property real fw: pipDisplay.frameWidth  > 0 ? pipDisplay.frameWidth  : 16
        property real fh: pipDisplay.frameHeight > 0 ? pipDisplay.frameHeight : 9
        width: rotated90 ? (pipBase * (fw / fh)) : pipBase
        height: rotated90 ? pipBase : (pipBase * (fh / fw))
        radius: 10
        color: Qt.rgba(0,0,0,0.35)
        border.width: 3
        border.color: "#FFFFFF"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.topMargin: 120   // 往下移动，避开状态栏（可按需调整）
        z: 30

        VideoDisplay {
            id: pipDisplay
            anchors.fill: parent
            anchors.margins: 4
        }
    }

    // 监听相机模式变化，切换显示的信号源
    Connections {
        target: CameraManager
        function onCameraModeChanged() {
            updateVideoSource()
        }
    }

    // 连接AI检测结果，用于绘制覆盖层
    Connections {
        target: AiDetectionManager
        function onDetectionsUpdated(detections) {
            videoDisplay.setDetections(detections)
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
            CameraManager.leftPixmapUpdated.connect(pipDisplay.updateFrame)
        } else if (mode === singleCamera) {
            // 单目模式：显示单相机
            CameraManager.singlePixmapUpdated.connect(videoDisplay.updateFrame)
            CameraManager.singlePixmapUpdated.connect(pipDisplay.updateFrame)
        } else {
            // 无相机模式：清空显示
            videoDisplay.clear()
            pipDisplay.clear()
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

        // 默认连接一次（防止初始无触发）
        if (CameraManager) {
            if (CameraManager.cameraMode === stereoCamera) {
                CameraManager.leftPixmapUpdated.connect(pipDisplay.updateFrame)
            } else if (CameraManager.cameraMode === singleCamera) {
                CameraManager.singlePixmapUpdated.connect(pipDisplay.updateFrame)
            }
        }
    }

    Component.onDestruction: {
        // 清理所有连接
        if (CameraManager) {
            try {
                CameraManager.leftPixmapUpdated.disconnect(videoDisplay.updateFrame)
            } catch(e) {}
            try { CameraManager.singlePixmapUpdated.disconnect(videoDisplay.updateFrame) } catch(e) {}
            try { CameraManager.leftPixmapUpdated.disconnect(pipDisplay.updateFrame) } catch(e) {}
            try { CameraManager.singlePixmapUpdated.disconnect(pipDisplay.updateFrame) } catch(e) {}
        }
    }
}
