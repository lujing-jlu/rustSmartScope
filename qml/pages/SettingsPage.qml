import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"
import RustSmartScope.Logger 1.0

Rectangle {
    id: settingsPage
    color: "transparent"

    signal backRequested()
    signal navigateRequested(string pageType)

    // 主布局
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 30

        // 页面标题
        Text {
            text: "⚙️ 系统设置"
            font.pixelSize: 48
            font.bold: true
            color: "#F59E0B"  // 橙色
            Layout.alignment: Qt.AlignHCenter
        }

        // 设置内容区域（滚动视图）
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 30

                // 相机参数设置已移至独立窗口

                // 其他设置区域（预留）
                GroupBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200

                    background: Rectangle {
                        color: Qt.rgba(20/255, 20/255, 20/255, 0.78)
                        border.color: "#8B5CF6"
                        border.width: 2
                        radius: 10
                    }

                    label: Text {
                        text: "🔧 系统设置"
                        font.pixelSize: 32
                        font.bold: true
                        color: "#8B5CF6"
                        padding: 10
                    }

                    ColumnLayout {
                        width: parent.width
                        spacing: 15

                        Text {
                            text: "其他系统设置（待实现）"
                            font.pixelSize: 20
                            color: "#9CA3AF"
                        }
                    }
                }

                // 外置存储检测
                GroupBox {
                    Layout.fillWidth: true
                    // 固定较为稳妥的高度，避免implicitHeight为0导致不可见
                    Layout.preferredHeight: 420

                    background: Rectangle {
                        color: Qt.rgba(20/255, 20/255, 20/255, 0.78)
                        border.color: "#10B981"
                        border.width: 2
                        radius: 10
                    }

                    label: Text {
                        text: "💽 外置存储 (U盘 / SD卡)"
                        font.pixelSize: 32
                        font.bold: true
                        color: "#10B981"
                        padding: 10
                    }

                    ColumnLayout {
                        id: externalStorageContent
                        width: parent.width
                        spacing: 16

                        // 保存位置选择
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16
                            Text { text: "保存位置："; color: "white"; font.pixelSize: 20 }
                            ButtonGroup { id: storageLocGroup }
                            RadioButton {
                                id: internalRadio
                                text: "机内存储"
                                checked: true
                                ButtonGroup.group: storageLocGroup
                                onToggled: if (checked && StorageManager) StorageManager.setStorageLocation(0)
                            }
                            RadioButton {
                                id: externalRadio
                                text: "外置硬盘"
                                checked: false
                                ButtonGroup.group: storageLocGroup
                                onToggled: if (checked && StorageManager) StorageManager.setStorageLocation(1)
                            }
                        }

                        // 外置设备选择（仅在外置模式下）
                        RowLayout {
                            visible: externalRadio.checked
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "选择设备："; color: "white"; font.pixelSize: 20 }
                            ComboBox {
                                id: deviceCombo
                                Layout.fillWidth: true
                                model: storageListModel
                                textRole: "_display"
                                delegate: ItemDelegate {
                                    text: (model.label && model.label.length>0) ? (model.label + " (" + model.device + ")") : model.device
                                }
                                onActivated: {
                                    var item = storageListModel.get(currentIndex)
                                    if (item && StorageManager) {
                                        StorageManager.setStorageExternalDevice(item.device)
                                    }
                                }
                            }
                            UniversalButton {
                                text: "刷新设备"
                                buttonStyle: "action"
                                onClicked: refreshStorageBtn.clicked()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            UniversalButton {
                                id: refreshStorageBtn
                                text: "刷新"
                                iconSource: "qrc:/icons/view.svg"
                                buttonStyle: "action"
                                onClicked: {
                                    try {
                                        var json = StorageManager ? StorageManager.refreshExternalStoragesJson() : "[]"
                                        var arr = JSON.parse(json)
                                        storageListModel.clear()
                                        for (var i = 0; i < arr.length; i++) {
                                            var disp = (arr[i].label && arr[i].label.length>0) ? (arr[i].label + " (" + arr[i].device + ")") : arr[i].device
                                            storageListModel.append({
                                                device: arr[i].device,
                                                label: arr[i].label || "",
                                                mountPoint: arr[i].mount_point,
                                                fsType: arr[i].fs_type,
                                                _display: disp
                                            })
                                        }
                                    } catch(e) {
                                        Logger.error("解析外置存储JSON失败: " + e)
                                    }
                                }
                            }

                            Text { text: "点击刷新以检测当前已挂载的外置存储"; font.pixelSize: 20; color: "#9CA3AF" }
                        }

                        ListModel { id: storageListModel }

                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(280, contentHeight > 0 ? contentHeight : 280)
                            clip: true
                            model: storageListModel
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 48
                                color: index % 2 === 0 ? "#111318" : "#0E1015"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 12
                                    // 组合显示字段，便于 ComboBox 使用 textRole
                                    property string _display: (label && label.length>0) ? (label + " (" + device + ")") : device
                                    Text { text: parent._display; color: "white"; font.pixelSize: 20; Layout.preferredWidth: 320; elide: Text.ElideRight }
                                    Text { text: mountPoint; color: "#A7F3D0"; font.pixelSize: 18; Layout.fillWidth: true; elide: Text.ElideRight }
                                    Text { text: fsType; color: "#34D399"; font.pixelSize: 18 }
                                }
                            }
                            visible: storageListModel.count > 0
                        }

                        Text { visible: storageListModel.count === 0; text: "暂无外置存储"; color: "#9CA3AF"; font.pixelSize: 20 }
                    }
                }

                // 底部留白
                Item {
                    Layout.fillHeight: true
                    Layout.minimumHeight: 50
                }
            }
        }
    }

    Component.onCompleted: {
        Logger.info("SettingsPage initialized")
        // 自动刷新一次外置存储列表，避免空白
        try {
            if (typeof StorageManager !== 'undefined' && StorageManager) {
                // 初始化存储配置选项
                var cfgJson = StorageManager.getStorageConfigJson()
                if (cfgJson && cfgJson.length>0) {
                    var cfg = JSON.parse(cfgJson)
                    internalRadio.checked = (cfg.location === 'internal')
                    externalRadio.checked = (cfg.location === 'external')
                }
                var json = StorageManager.refreshExternalStoragesJson()
                var arr = JSON.parse(json)
                storageListModel.clear()
                for (var i = 0; i < arr.length; i++) {
                    var disp = (arr[i].label && arr[i].label.length>0) ? (arr[i].label + " (" + arr[i].device + ")") : arr[i].device
                    storageListModel.append({
                        device: arr[i].device,
                        label: arr[i].label || "",
                        mountPoint: arr[i].mount_point,
                        fsType: arr[i].fs_type,
                        _display: disp
                    })
                }
                // 根据配置选择外置设备
                if (cfg && cfg.external_device && storageListModel.count>0) {
                    for (var j=0;j<storageListModel.count;j++) {
                        var it = storageListModel.get(j)
                        if (it.device === cfg.external_device) {
                            deviceCombo.currentIndex = j
                            break
                        }
                    }
                }
            }
        } catch(e) {
            Logger.error("初始化外置存储列表失败: " + e)
        }
    }
}
