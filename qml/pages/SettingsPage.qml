import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.15
import QtGraphicalEffects 1.15
import "../components"
import RustSmartScope.Logger 1.0

Rectangle {
    id: settingsPage
    color: "transparent"

    signal backRequested()
    signal navigateRequested(string pageType)

    // Settings页专用文字层级（集中管理）
    QtObject {
        id: st
        property int titleSize: 64
        property int sectionSize: 40
        property int labelSize: 40
        property int controlSize: 32
        property int hintSize: 40
        property int listPrimary: 32
        property int listSecondary: 28
        property int buttonSize: 32
    }

    // 主布局
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 56
        spacing: 40

        // 页面标题（使用SVG而非emoji）
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16

            Item {
                width: st.titleSize * 0.9
                height: st.titleSize * 0.9
                Image { id: titleIcon; anchors.fill: parent; source: "qrc:/icons/setting.svg"; fillMode: Image.PreserveAspectFit; visible: false }
                ColorOverlay { anchors.fill: titleIcon; source: titleIcon; color: "#FFFFFF" }
            }

            Text {
                text: "系统设置"
                font.pixelSize: st.titleSize
                font.bold: true
                color: "#FFFFFF"
            }
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

                    ColumnLayout {
                        width: parent.width
                        spacing: 15

                        // 子标题与设置项在同一布局
                        Row {
                            spacing: 10
                            Layout.alignment: Qt.AlignLeft
                            Layout.bottomMargin: 8
                            Item {
                                width: st.sectionSize * 0.8
                                height: st.sectionSize * 0.8
                                Image { id: sectionIcon1; anchors.fill: parent; source: "qrc:/icons/setting.svg"; fillMode: Image.PreserveAspectFit; visible: false }
                                ColorOverlay { anchors.fill: sectionIcon1; source: sectionIcon1; color: "#FFFFFF" }
                            }
                            Text { text: "系统设置"; font.pixelSize: st.sectionSize; font.bold: true; color: "#FFFFFF" }
                        }

                        Text {
                            text: "其他系统设置（待实现）"
                            font.pixelSize: st.controlSize
                            color: "#FFFFFF"
                        }
                    }
                }

                // 外置存储检测
                GroupBox {
                    Layout.fillWidth: true
                    // 固定较为稳妥的高度，避免implicitHeight为0导致不可见
                    Layout.preferredHeight: 500

                    background: Rectangle {
                        color: Qt.rgba(20/255, 20/255, 20/255, 0.78)
                        border.color: "#10B981"
                        border.width: 2
                        radius: 10
                    }

                    ColumnLayout {
                        id: externalStorageContent
                        width: parent.width
                        spacing: 16
                        // 外置设备区可见性（由互斥按钮控制/初始化同步配置）
                        property bool externalSectionVisible: false

                        // 子标题与设置项在同一布局
                        Row {
                            spacing: 10
                            Layout.alignment: Qt.AlignLeft
                            Layout.bottomMargin: 8
                            Item {
                                width: st.sectionSize * 0.8
                                height: st.sectionSize * 0.8
                                Image { id: storageIcon; anchors.fill: parent; source: "qrc:/icons/save.svg"; fillMode: Image.PreserveAspectFit; visible: false }
                                ColorOverlay { anchors.fill: storageIcon; source: storageIcon; color: "#FFFFFF" }
                            }
                            Text { text: "外置存储 (U盘 / SD卡)"; font.pixelSize: st.sectionSize; font.bold: true; color: "#FFFFFF" }
                        }

                        // 保存位置选择（互斥按钮）
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 20

                            // 左侧标签（加大一倍）
                            Text { text: "保存位置："; color: "#FFFFFF"; font.pixelSize: st.labelSize }

                            // 机内存储按钮
                            UniversalButton {
                                id: btnInternal
                                text: "机内存储"
                                buttonStyle: "navigation"
                                isActive: true
                                customButtonWidth: 240
                                customButtonHeight: 80
                                customIconSize: 0
                                customTextSize: st.buttonSize
                                onClicked: {
                                    if (!isActive) {
                                        isActive = true
                                        btnExternal.isActive = false
                                    }
                                    externalStorageContent.externalSectionVisible = false
                                    if (StorageManager) StorageManager.setStorageLocation(0)
                                }
                            }

                            // 外置硬盘按钮
                            UniversalButton {
                                id: btnExternal
                                text: "外置硬盘"
                                buttonStyle: "navigation"
                                isActive: false
                                customButtonWidth: 240
                                customButtonHeight: 80
                                customIconSize: 0
                                customTextSize: st.buttonSize
                                onClicked: {
                                    if (!isActive) {
                                        isActive = true
                                        btnInternal.isActive = false
                                    }
                                    externalStorageContent.externalSectionVisible = true
                                    if (StorageManager) StorageManager.setStorageLocation(1)
                                }
                            }
                        }

                        // 外置设备选择（仅在外置模式下）
                        RowLayout {
                            visible: externalStorageContent.externalSectionVisible
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "选择设备："; color: "#FFFFFF"; font.pixelSize: st.labelSize }
                            ComboBox {
                                id: deviceCombo
                                Layout.fillWidth: true
                                model: storageListModel
                                textRole: "_display"
                                font.pixelSize: st.controlSize + 16
                                implicitHeight: 72
                                delegate: ItemDelegate {
                                    text: (model.label && model.label.length>0) ? (model.label + " (" + model.device + ")") : model.device
                                    font.pixelSize: st.controlSize + 16
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
                                customButtonWidth: 220
                                customButtonHeight: 72
                                customIconSize: 32
                                customTextSize: st.buttonSize
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
                                customButtonWidth: 220
                                customButtonHeight: 72
                                customIconSize: 36
                                customTextSize: st.buttonSize
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

                            Text { text: "点击刷新以检测当前已挂载的外置存储"; font.pixelSize: st.hintSize; color: "#FFFFFF" }
                        }

                        ListModel { id: storageListModel }

                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(420, contentHeight > 0 ? contentHeight : 420)
                            clip: true
                            model: storageListModel
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 88
                                color: index % 2 === 0 ? "#111318" : "#0E1015"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 16
                                    // 组合显示字段，便于 ComboBox 使用 textRole
                                    property string _display: (label && label.length>0) ? (label + " (" + device + ")") : device
                                    Text { text: parent._display; color: "#FFFFFF"; font.pixelSize: st.listPrimary; Layout.preferredWidth: 460; elide: Text.ElideRight }
                                    Text { text: mountPoint; color: "#FFFFFF"; font.pixelSize: st.listSecondary; Layout.fillWidth: true; elide: Text.ElideRight }
                                    Text { text: fsType; color: "#FFFFFF"; font.pixelSize: st.listSecondary }
                                }
                            }
                            visible: storageListModel.count > 0
                        }

                        Text { visible: storageListModel.count === 0; text: "暂无外置存储"; color: "#FFFFFF"; font.pixelSize: st.hintSize }
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
                var cfg = null
                if (cfgJson && cfgJson.length>0) {
                    cfg = JSON.parse(cfgJson)
                    var isExt = (cfg.location === 'external')
                    btnInternal.isActive = !isExt
                    btnExternal.isActive = isExt
                    externalStorageContent.externalSectionVisible = isExt
                } else {
                    btnInternal.isActive = true
                    btnExternal.isActive = false
                    externalStorageContent.externalSectionVisible = false
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
