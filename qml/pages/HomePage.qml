import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"
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
        transformOrigin: Item.Center
    }

    // 捏合缩放（双指触控）；桌面可配合左侧+/-按钮
    PinchArea {
        anchors.fill: videoDisplay
        pinch.target: videoDisplay
        pinch.minimumScale: 1.0
        pinch.maximumScale: 3.0
        enabled: true
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
        // 确保容器尺寸与帧宽高比严格一致，并取整到像素，避免与内部缩略图出现1px偏差
        width: Math.round(rotated90 ? (pipBase * (fw / fh)) : pipBase)
        height: Math.round(rotated90 ? pipBase : (pipBase * (fh / fw)))
        radius: 0
        color: Qt.rgba(0,0,0,0.35)
        // 由叠加边框绘制显示边框，避免被子项覆盖
        border.width: 0
        border.color: "transparent"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.topMargin: 120   // 往下移动，避开状态栏（可按需调整）
        z: 30

        VideoDisplay {
            id: pipDisplay
            anchors.fill: parent
            // 去掉内边距，避免容器边框与缩略图之间的多余留白，确保两者完全重合
            anchors.margins: 0
        }

        // 边框叠加层：绘制在内容之上，确保边框可见
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 3
            border.color: "#FFFFFF"
            radius: 0
            z: 1000
        }

        // 根据主视频缩放更新PIP视窗白框
        function updatePipViewWindow() {
            if (pipDisplay.frameWidth <= 0 || pipDisplay.frameHeight <= 0) return
            var fw = pipDisplay.frameWidth
            var fh = pipDisplay.frameHeight
            var mw = videoDisplay.width
            var mh = videoDisplay.height
            var k = Math.min(mw / fw, mh / fh)   // letterbox缩放因子
            var s = Math.max(1.0, videoDisplay.scale)
            var vw = mw / (k * s)   // 可见区域在帧坐标系的宽度
            var vh = mh / (k * s)   // 可见区域在帧坐标系的高度
            // 居中窗口（当前未实现平移）
            var vx = (fw - vw) / 2
            var vy = (fh - vh) / 2
            pipDisplay.viewWindow = Qt.rect(vx, vy, vw, vh)
        }
    }

    // 左侧缩放工具栏（自下而上显示）
    Item {
        id: leftToolBar
        property int sideMargin: 16
        property int topGap: 100       // 近似状态栏高度
        property int bottomGap: 120    // 近似导航栏高度
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: sideMargin
        anchors.topMargin: topGap
        anchors.bottomMargin: bottomGap
        width: 100
        z: 200

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            // 填充占位，将按钮推到底部
            Item { Layout.fillHeight: true }

            // 缩放百分比显示已移除

            // 上：缩小
            UniversalButton {
                id: zoomOutButton
                text: ""
                iconSource: "qrc:/icons/zoom_out.svg"
                buttonStyle: "toolbar"
                onClicked: {
                    const newScale = Math.max(1.0, videoDisplay.scale * 0.9)
                    videoDisplay.scale = newScale
                }
                // 长按持续缩小
                Timer {
                    id: zoomOutRepeat
                    interval: 80
                    repeat: true
                    running: zoomOutButton.isPressed
                    onTriggered: {
                        const ns = Math.max(1.0, videoDisplay.scale * 0.96)
                        videoDisplay.scale = ns
                    }
                }
            }

            // 中：重置
            UniversalButton {
                id: zoomResetButton
                text: ""
                iconSource: "qrc:/icons/zoom_reset.svg"
                buttonStyle: "toolbar"
                onClicked: {
                    videoDisplay.scale = 1.0
                }
            }

            // 下：放大
            UniversalButton {
                id: zoomInButton
                text: ""
                iconSource: "qrc:/icons/zoom_in.svg"
                buttonStyle: "toolbar"
                onClicked: {
                    const newScale = Math.min(3.0, videoDisplay.scale * 1.1)
                    videoDisplay.scale = newScale
                }
                // 长按持续放大
                Timer {
                    id: zoomInRepeat
                    interval: 80
                    repeat: true
                    running: zoomInButton.isPressed
                    onTriggered: {
                        const ns = Math.min(3.0, videoDisplay.scale * 1.04)
                        videoDisplay.scale = ns
                    }
                }
            }
        }
    }

    // 监听相机模式变化，切换显示的信号源
    Connections {
        target: CameraManager
        function onCameraModeChanged() {
            updateVideoSource()
        }
    }

    // 监听缩放与帧尺寸变化，更新PIP白框
    Connections {
        target: videoDisplay
        function onScaleChanged() { pipContainer.updatePipViewWindow() }
        function onWidthChanged() { pipContainer.updatePipViewWindow() }
        function onHeightChanged() { pipContainer.updatePipViewWindow() }
    }
    Connections {
        target: pipDisplay
        function onFrameSizeChanged() { pipContainer.updatePipViewWindow() }
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

        // 初始化PIP视窗白框
        pipContainer.updatePipViewWindow()

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
