import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: debugPage
    objectName: "debug"

    signal backRequested()

    background: Rectangle {
        color: "#1E1E1E"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // 页面标题和返回按钮
        RowLayout {
            Layout.fillWidth: true

            Button {
                text: "← 返回"
                onClicked: debugPage.backRequested()
            }

            Text {
                text: "🐛 系统调试"
                font.pixelSize: 24
                font.bold: true
                color: "#ECEFF4"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 80 }
        }

        // 主内容区域
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            // 左侧：系统状态
            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                color: "#2B2B2B"
                border.color: "#3D3D3D"
                border.width: 2
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 15

                    Text {
                        text: "🖥️ 系统状态"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#ECEFF4"
                    }

                    // Rust核心状态
                    GroupBox {
                        Layout.fillWidth: true
                        title: "Rust核心"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            StatusItem { label: "状态:"; value: "运行中"; status: "good" }
                            StatusItem { label: "内存:"; value: "45.2 MB"; status: "good" }
                            StatusItem { label: "CPU:"; value: "12%"; status: "good" }
                            StatusItem { label: "线程数:"; value: "4"; status: "good" }
                        }
                    }

                    // 相机状态
                    GroupBox {
                        Layout.fillWidth: true
                        title: "相机状态"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            StatusItem { label: "左相机:"; value: "未连接"; status: "error" }
                            StatusItem { label: "右相机:"; value: "未连接"; status: "error" }
                            StatusItem { label: "帧率:"; value: "0 FPS"; status: "error" }
                            StatusItem { label: "分辨率:"; value: "N/A"; status: "error" }
                        }
                    }

                    // AI推理状态
                    GroupBox {
                        Layout.fillWidth: true
                        title: "AI推理"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            StatusItem { label: "模型:"; value: "未加载"; status: "warning" }
                            StatusItem { label: "推理时间:"; value: "N/A"; status: "warning" }
                            StatusItem { label: "GPU使用:"; value: "0%"; status: "warning" }
                            StatusItem { label: "检测数:"; value: "0"; status: "warning" }
                        }
                    }
                }
            }

            // 中间：日志输出
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#2B2B2B"
                border.color: "#3D3D3D"
                border.width: 2
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "📋 系统日志"
                            font.pixelSize: 16
                            font.bold: true
                            color: "#ECEFF4"
                        }

                        Item { Layout.fillWidth: true }

                        ComboBox {
                            model: ["全部", "INFO", "WARNING", "ERROR", "DEBUG"]
                        }

                        Button {
                            text: "🗑️ 清空"
                            onClicked: logModel.clear()
                        }

                        Button {
                            text: "📁 保存"
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            model: logModel
                            spacing: 2

                            delegate: Rectangle {
                                width: parent.width
                                height: 25
                                color: {
                                    switch(model.level) {
                                        case "ERROR": return "#BF616A"
                                        case "WARNING": return "#D08770"
                                        case "INFO": return "#88C0D0"
                                        case "DEBUG": return "#4C566A"
                                        default: return "transparent"
                                    }
                                }
                                opacity: 0.2
                                radius: 3

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 10

                                    Text {
                                        text: model.time
                                        color: "#88C0D0"
                                        font.pixelSize: 10
                                        font.family: "monospace"
                                        Layout.preferredWidth: 80
                                    }

                                    Text {
                                        text: model.level
                                        color: {
                                            switch(model.level) {
                                                case "ERROR": return "#BF616A"
                                                case "WARNING": return "#D08770"
                                                case "INFO": return "#88C0D0"
                                                case "DEBUG": return "#4C566A"
                                                default: return "#D8DEE9"
                                            }
                                        }
                                        font.pixelSize: 10
                                        font.bold: true
                                        Layout.preferredWidth: 60
                                    }

                                    Text {
                                        text: model.message
                                        color: "#ECEFF4"
                                        font.pixelSize: 10
                                        font.family: "monospace"
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 右侧：调试工具
            Rectangle {
                Layout.preferredWidth: 250
                Layout.fillHeight: true
                color: "#2B2B2B"
                border.color: "#3D3D3D"
                border.width: 2
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 15

                    Text {
                        text: "🔧 调试工具"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#ECEFF4"
                    }

                    // 相机调试
                    GroupBox {
                        Layout.fillWidth: true
                        title: "相机调试"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            Button {
                                Layout.fillWidth: true
                                text: "🔍 扫描设备"
                                onClicked: {
                                    addLog("INFO", "开始扫描相机设备...")
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "📷 测试左相机"
                                onClicked: {
                                    addLog("INFO", "测试左相机连接...")
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "📹 测试右相机"
                                onClicked: {
                                    addLog("INFO", "测试右相机连接...")
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "🎯 标定测试"
                                onClicked: {
                                    addLog("WARNING", "相机标定数据缺失")
                                }
                            }
                        }
                    }

                    // Rust核心调试
                    GroupBox {
                        Layout.fillWidth: true
                        title: "Rust核心"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            Button {
                                Layout.fillWidth: true
                                text: "🔄 重启核心"
                                onClicked: {
                                    addLog("INFO", "正在重启Rust核心...")
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "🧠 内存分析"
                                onClicked: {
                                    addLog("DEBUG", "内存使用: 45.2MB, 峰值: 52.1MB")
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "📊 性能统计"
                                onClicked: {
                                    addLog("INFO", "平均处理时间: 23.5ms")
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "⚠️ 故障诊断"
                                onClicked: {
                                    addLog("ERROR", "发现内存泄漏风险")
                                }
                            }
                        }
                    }

                    // AI调试
                    GroupBox {
                        Layout.fillWidth: true
                        title: "AI调试"

                        background: Rectangle {
                            color: "#3B4252"
                            border.color: "#4C566A"
                            radius: 5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            Button {
                                Layout.fillWidth: true
                                text: "🤖 加载模型"
                                onClicked: {
                                    addLog("INFO", "正在加载YOLOv8模型...")
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "🎯 推理测试"
                                onClicked: {
                                    addLog("INFO", "推理时间: 45.2ms")
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "🖥️ GPU状态"
                                onClicked: {
                                    addLog("DEBUG", "GPU: 运行中, 温度: 65°C")
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    // 系统操作
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            Layout.fillWidth: true
                            text: "📋 导出日志"
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "🔄 重启系统"
                        }
                    }
                }
            }
        }
    }

    // 状态项组件
    component StatusItem: RowLayout {
        property string label: ""
        property string value: ""
        property string status: "good" // good, warning, error

        Text {
            text: label
            color: "#D8DEE9"
            font.pixelSize: 11
            Layout.preferredWidth: 60
        }

        Text {
            text: value
            color: {
                switch(status) {
                    case "good": return "#A3BE8C"
                    case "warning": return "#D08770"
                    case "error": return "#BF616A"
                    default: return "#88C0D0"
                }
            }
            font.pixelSize: 11
            font.bold: true
            Layout.fillWidth: true
        }

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: {
                switch(status) {
                    case "good": return "#A3BE8C"
                    case "warning": return "#D08770"
                    case "error": return "#BF616A"
                    default: return "#4C566A"
                }
            }
        }
    }

    // 日志模型
    ListModel {
        id: logModel

        Component.onCompleted: {
            append({time: "14:30:15", level: "INFO", message: "RustSmartScope 启动成功"})
            append({time: "14:30:16", level: "INFO", message: "Rust核心初始化完成"})
            append({time: "14:30:17", level: "WARNING", message: "左相机设备未找到"})
            append({time: "14:30:18", level: "WARNING", message: "右相机设备未找到"})
            append({time: "14:30:19", level: "ERROR", message: "AI模型加载失败: 文件不存在"})
            append({time: "14:30:20", level: "DEBUG", message: "内存分配: 45.2MB"})
            append({time: "14:30:21", level: "INFO", message: "QML界面加载完成"})
        }
    }

    // 添加日志的函数
    function addLog(level, message) {
        var now = new Date()
        var timeString = Qt.formatTime(now, "hh:mm:ss")
        logModel.append({
            time: timeString,
            level: level,
            message: message
        })
    }

    Component.onCompleted: {
        console.log("DebugPage initialized")
    }
}