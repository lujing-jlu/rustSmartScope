# 通用弹窗组件使用指南

## GlassPopupWindow 通用弹窗组件

这是一个全屏透明窗口，中间显示毛玻璃卡片效果的通用弹窗组件，支持点击外部关闭。

### 基本用法

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"

// 基本弹窗
GlassPopupWindow {
    id: myPopup

    // 基本属性
    windowTitle: "我的弹窗"
    windowWidth: 800
    windowHeight: 600
    showCloseButton: true

    // 内容组件
    content: Rectangle {
        anchors.fill: parent
        color: "transparent"

        Text {
            anchors.centerIn: parent
            text: "这是弹窗内容"
            color: "white"
            font.pixelSize: 24
        }
    }

    // 事件处理
    onOpened: {
        console.log("弹窗已打开")
    }

    onClosed: {
        console.log("弹窗已关闭")
    }
}
```

### 自定义内容组件

```qml
// 自定义内容组件
Item {
    property alias text: textElement.text

    Text {
        id: textElement
        anchors.centerIn: parent
        color: "white"
        font.pixelSize: 20
    }

    Button {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        text: "确定"
        onClicked: {
            // 发送信号或执行操作
        }
    }
}
```

### 属性说明

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| windowTitle | string | "弹窗" | 窗口标题 |
| windowWidth | int | 1100 | 弹窗宽度 |
| windowHeight | int | 850 | 弹窗高度 |
| showCloseButton | bool | true | 是否显示关闭按钮 |
| content | Component | null | 内容组件 |
| accentPrimary | color | "#0EA5E9" | 主题色 |
| textPrimary | color | "#FFFFFF" | 主文字颜色 |
| cornerRadius | int | 24 | 圆角半径 |

### 信号

| 信号名 | 参数 | 说明 |
|--------|------|------|
| closeRequested | - | 关闭请求（点击外部或关闭按钮） |
| opened | - | 弹窗打开时发出 |
| closed | - | 弹窗关闭时发出 |

### 公共方法

| 方法名 | 参数 | 说明 |
|--------|------|------|
| show() | - | 显示弹窗 |
| hide() | - | 隐藏弹窗 |
| close() | - | 关闭弹窗 |

### 使用场景示例

#### 1. 确认对话框

```qml
GlassPopupWindow {
    id: confirmDialog

    windowTitle: "确认操作"
    windowWidth: 600
    windowHeight: 400

    content: Item {
        Text {
            anchors.centerIn: parent
            text: "确定要执行此操作吗？"
            color: "white"
            font.pixelSize: 20
        }

        Row {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 20

            Button {
                text: "取消"
                onClicked: confirmDialog.close()
            }

            Button {
                text: "确定"
                onClicked: {
                    // 执行操作
                    confirmDialog.close()
                }
            }
        }
    }
}
```

#### 2. 设置面板

```qml
GlassPopupWindow {
    id: settingsPanel

    windowTitle: "设置"
    windowWidth: 900
    windowHeight: 700

    content: ScrollView {
        anchors.fill: parent

        Column {
            width: parent.width
            spacing: 20

            // 设置项...
        }
    }
}
```

## VideoAdjustmentContent 视频调整内容组件

专门用于视频画面调整的内容组件，包含旋转、翻转、曝光、亮度等功能。

### 使用方法

```qml
GlassPopupWindow {
    id: videoPopup

    windowTitle: "画面调整"

    content: VideoAdjustmentContent {
        onTransformationChanged: {
            // 处理变换变化
        }

        onExposureChanged: function(mode, value) {
            // 处理曝光变化
        }

        onBrightnessChanged: function(value) {
            // 处理亮度变化
        }

        onResetRequested: {
            // 处理重置请求
        }

        onAdvancedSettingsRequested: {
            // 处理高级设置请求
        }
    }
}
```

### 信号说明

| 信号名 | 参数 | 说明 |
|--------|------|------|
| transformationChanged | - | 变换参数改变时发出 |
| exposureChanged | int mode, int value | 曝光参数改变时发出 |
| brightnessChanged | int value | 亮度改变时发出 |
| resetRequested | - | 重置请求时发出 |
| advancedSettingsRequested | - | 高级设置请求时发出 |

### 注意事项

1. 组件已经处理了所有视频变换的逻辑
2. 通过信号系统与外部通信
3. 可以通过属性获取当前状态

## 样式定制

GlassPopupWindow 支持通过属性定制样式：

```qml
GlassPopupWindow {
    windowTitle: "自定义样式"

    // 自定义颜色
    accentPrimary: "#FF6B6B"
    textPrimary: "#FFFFFF"
    borderPrimary: "#FF6B6B"

    // 自定义圆角
    cornerRadius: 32

    // 自定义字体
    fontFamily: "Arial"
}
```