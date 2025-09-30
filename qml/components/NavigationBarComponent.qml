import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: navigationBar
    color: "#2B2B2B"
    border.color: "#3D3D3D"
    border.width: 1
    radius: 40

    // 信号：页面切换请求
    signal pageRequested(string pageType)

    // 当前页面
    property string currentPage: "home"

    // 导航按钮数据
    property var navigationItems: [
        {
            id: "home",
            icon: "🏠",
            label: "主页",
            shortcut: "F8"
        },
        {
            id: "measurement",
            icon: "📏",
            label: "测量",
            shortcut: "F5"
        },
        {
            id: "preview",
            icon: "🔍",
            label: "预览",
            shortcut: "F3"
        },
        {
            id: "settings",
            icon: "⚙️",
            label: "设置",
            shortcut: "F2"
        },
        {
            id: "report",
            icon: "📊",
            label: "报告",
            shortcut: ""
        },
        {
            id: "debug",
            icon: "🐛",
            label: "调试",
            shortcut: ""
        }
    ]

    // 导航按钮布局
    RowLayout {
        anchors.centerIn: parent
        spacing: 10

        Repeater {
            model: navigationBar.navigationItems

            delegate: NavigationButton {
                id: navButton
                icon: modelData.icon
                label: modelData.label
                shortcut: modelData.shortcut
                isActive: navigationBar.currentPage === modelData.id

                onClicked: {
                    navigationBar.currentPage = modelData.id
                    navigationBar.pageRequested(modelData.id)
                }
            }
        }
    }

    // 公共方法
    function setCurrentPage(pageType) {
        currentPage = pageType
    }

    function highlightButton(pageType) {
        // 临时高亮效果，可用于键盘导航等
        for (var i = 0; i < navigationItems.length; i++) {
            if (navigationItems[i].id === pageType) {
                // 这里可以添加临时高亮逻辑
                break
            }
        }
    }

    // 呼吸效果（可选）
    property bool breathingEffect: false

    SequentialAnimation on opacity {
        running: breathingEffect
        loops: Animation.Infinite
        PropertyAnimation { to: 0.7; duration: 2000 }
        PropertyAnimation { to: 1.0; duration: 2000 }
    }
}