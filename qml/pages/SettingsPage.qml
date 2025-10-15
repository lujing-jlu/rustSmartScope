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
                        // 当前选择的设备（空字符串表示机内存储）
                        property string currentDevice: ""

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
                            Text { text: "数据存储位置"; font.pixelSize: st.sectionSize; font.bold: true; color: "#FFFFFF" }
                        }

                        // 保存位置选择（互斥按钮：机内 + 动态外置设备）
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text { text: "保存位置："; color: "#FFFFFF"; font.pixelSize: st.labelSize }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 16

                                // 机内存储按钮
                                UniversalButton {
                                    id: btnInternal
                                    text: "机内存储"
                                    buttonStyle: "navigation"
                                    isActive: externalStorageContent.currentDevice === ""
                                    customButtonWidth: 240
                                    customButtonHeight: 80
                                    customIconSize: 0
                                    customTextSize: st.buttonSize
                                    onClicked: {
                                        externalStorageContent.currentDevice = ""
                                        if (StorageManager) StorageManager.setStorageLocation(0)
                                    }
                                }

                                // 动态外置设备按钮
                                Repeater {
                                    model: storageListModel
                                    delegate: UniversalButton {
                                        // 展示 label(设备) 或设备
                                        property string devicePath: model.device
                                        text: model._display
                                        buttonStyle: "navigation"
                                        isActive: externalStorageContent.currentDevice === devicePath
                                        customButtonWidth: 300
                                        customButtonHeight: 80
                                        customIconSize: 0
                                        customTextSize: st.buttonSize
                                        onClicked: {
                                            externalStorageContent.currentDevice = devicePath
                                            if (StorageManager) {
                                                StorageManager.setStorageLocation(1)
                                                StorageManager.setStorageExternalDevice(devicePath)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 刷新设备按钮
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
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

                        // 隐含刷新动作（供“刷新设备”触发）
                        UniversalButton {
                            id: refreshStorageBtn
                            visible: false
                            onClicked: {
                                try {
                                    var json = StorageManager ? StorageManager.refreshExternalStoragesJson() : "[]"
                                    var arr = JSON.parse(json)
                                    var prev = externalStorageContent.currentDevice
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
                                    // 保持当前选择（若不在列表则回退到机内存储）
                                    var found = false
                                    for (var j=0;j<storageListModel.count;j++) {
                                        if (storageListModel.get(j).device === prev) { found = true; break }
                                    }
                                    if (!found) externalStorageContent.currentDevice = ""
                                } catch(e) {
                                    Logger.error("解析外置存储JSON失败: " + e)
                                }
                            }
                        }

                        ListModel { id: storageListModel }

                        // 移除列表展示
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
