import QtQuick 2.15
import "../i18n" 1.0 as I18n

// Lightweight overlay that draws detection boxes on a cached layer
// Renders only when detections change; video frames run at full FPS.
Canvas {
    id: overlay
    anchors.fill: parent
    antialiasing: false
    renderTarget: Canvas.Image
    smooth: true
    clip: true

    // Cache to texture; recompute only when detections change
    layer.enabled: true
    layer.smooth: true

    // The underlying video item (QmlVideoItem)
    property var videoItem
    // Detection list: array of { left, top, right, bottom, class_id, label }
    property var detections: []
    // Model input size used by detector (should match VideoDisplay.modelInputSize)
    property int modelInputSize: 640
    // Limit redraw rate (Hz) to reduce UI load if updates are too frequent
    property int maxFps: 15

    // Throttle repaints
    property bool _pending: false
    onDetectionsChanged: {
        if (!throttle.running) {
            requestPaint()
            throttle.start()
        } else {
            _pending = true
        }
    }

    Timer {
        id: throttle
        interval: Math.max(10, Math.round(1000 / Math.max(1, overlay.maxFps)))
        repeat: false
        onTriggered: {
            if (overlay._pending) {
                overlay._pending = false
                overlay.requestPaint()
                throttle.start()
            }
        }
    }

    function classColor(cls) {
        // Tailwind-like palette
        var colors = [
            '#FF3B30', '#34C759', '#007AFF', '#FF9500', '#AF52DE',
            '#5AC8FA', '#FFD60A', '#FF2D55', '#30D158', '#64D2FF'
        ];
        var idx = Math.max(0, Math.floor(cls)) % colors.length;
        return colors[idx];
    }

    function mapBoxToDisplayRect(b) {
        if (!videoItem || videoItem.frameWidth <= 0 || videoItem.frameHeight <= 0)
            return null
        const ow = videoItem.frameWidth
        const oh = videoItem.frameHeight
        const boundsW = width
        const boundsH = height
        // Compute letterbox fit rect of video frame inside overlay bounds
        const k = Math.min(boundsW / ow, boundsH / oh)
        const dw = Math.round(ow * k)
        const dh = Math.round(oh * k)
        const dx = Math.round((boundsW - dw) / 2)
        const dy = Math.round((boundsH - dh) / 2)

        const scale = Math.min(modelInputSize / ow, modelInputSize / oh)
        const xOff = (modelInputSize - ow * scale) * 0.5
        const yOff = (modelInputSize - oh * scale) * 0.5

        // Back-project from model input space to source frame pixels
        let xl = (b.left  - xOff) / scale
        let yt = (b.top   - yOff) / scale
        let xr = (b.right - xOff) / scale
        let yb = (b.bottom- yOff) / scale
        xl = Math.max(0, Math.min(ow, xl))
        xr = Math.max(0, Math.min(ow, xr))
        yt = Math.max(0, Math.min(oh, yt))
        yb = Math.max(0, Math.min(oh, yb))

        // Map to display rect
        const rx = dx + xl * (dw / ow)
        const ry = dy + yt * (dh / oh)
        const rw = (xr - xl) * (dw / ow)
        const rh = (yb - yt) * (dh / oh)
        return { x: rx, y: ry, w: rw, h: rh }
    }

    onPaint: {
        const ctx = getContext('2d')
        ctx.reset()
        // Transparent clear
        ctx.clearRect(0, 0, width, height)
        if (!detections || detections.length === 0) return

        const pad = 8
        const fontSize = Math.max(28, Math.round(height * 0.04))
        ctx.font = 'bold ' + fontSize + 'px sans-serif'
        ctx.textBaseline = 'top'
        ctx.lineJoin = 'round'
        for (let i = 0; i < detections.length; i++) {
            const d = detections[i]
            const r = mapBoxToDisplayRect(d)
            if (!r) continue
            // Clamp to overlay bounds to ensure endpoints stay on-screen
            const x0 = Math.max(0, Math.min(boundsWidth(), r.x))
            const y0 = Math.max(0, Math.min(boundsHeight(), r.y))
            const x1 = Math.max(0, Math.min(boundsWidth(), r.x + r.w))
            const y1 = Math.max(0, Math.min(boundsHeight(), r.y + r.h))
            const cw = Math.max(0, x1 - x0)
            const ch = Math.max(0, y1 - y0)
            if (cw <= 0 || ch <= 0) continue

            // Colored box
            const color = classColor(d.class_id || 0)
            ctx.lineWidth = 3
            ctx.strokeStyle = color
            ctx.beginPath()
            ctx.rect(Math.round(x0) + 0.5, Math.round(y0) + 0.5, Math.round(cw), Math.round(ch))
            ctx.stroke()

            // Label (optional)
            // 根据语言动态选择标签
            let label = ''
            if (I18n.I18n.language === 'en') {
                label = d.label_en ? String(d.label_en) : (d.label ? String(d.label) : ('cls ' + (d.class_id||0)))
            } else {
                label = d.label_zh ? String(d.label_zh) : (d.label ? String(d.label) : ('cls ' + (d.class_id||0)))
            }
            const tw = Math.round(ctx.measureText(label).width)
            const th = fontSize + pad
            // Place inside top-left of the box, keep fully on screen
            let lbx = Math.round(x0) + 1
            let lby = Math.round(y0) + 1
            if (lbx + tw + pad*2 > width) lbx = Math.max(0, width - (tw + pad*2))
            if (lby + th + pad > height) lby = Math.max(0, height - (th + pad))

            // Label background (semi-transparent based on class color)
            ctx.fillStyle = 'rgba(0,0,0,0.55)'
            ctx.fillRect(lbx, lby, tw + pad*2, th)
            // Label border for better contrast
            ctx.strokeStyle = color
            ctx.lineWidth = 1
            ctx.strokeRect(lbx + 0.5, lby + 0.5, tw + pad*2, th)
            // Label text
            ctx.fillStyle = '#FFFFFF'
            ctx.fillText(label, lbx + pad, lby + Math.round(pad/2))
        }
    }

    // 语言切换时强制重绘
    Connections {
        target: I18n.I18n
        function onLanguageChanged() { overlay.requestPaint() }
    }

    function boundsWidth() { return width }
    function boundsHeight() { return height }
}
