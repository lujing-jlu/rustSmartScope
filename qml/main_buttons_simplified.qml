                    // 主页按钮
                    NavigationButton {
                        id: homeButton
                        text: "主页"
                        iconSource: "qrc:/icons/home.svg"
                        isActive: true
                        onClicked: {
                            console.log("Home clicked")
                            setActiveTab(homeButton)
                        }
                    }

                    // 预览按钮
                    NavigationButton {
                        id: detectionButton
                        text: "预览"
                        iconSource: "qrc:/icons/preview.svg"
                        activeColor: "#38BDF8"
                        onClicked: {
                            console.log("Detection clicked")
                            setActiveTab(detectionButton)
                        }
                    }

                    // 报告按钮
                    NavigationButton {
                        id: reportButton
                        text: "报告"
                        iconSource: "qrc:/icons/report.svg"
                        onClicked: {
                            console.log("Report clicked")
                            setActiveTab(reportButton)
                        }
                    }

                    // 设置按钮
                    NavigationButton {
                        id: settingsButton
                        text: "设置"
                        iconSource: "qrc:/icons/setting.svg"
                        onClicked: {
                            console.log("Settings clicked")
                            setActiveTab(settingsButton)
                        }
                    }

                    // 3D测量按钮
                    NavigationButton {
                        id: measurementButton
                        text: "3D测量"
                        iconSource: "qrc:/icons/3D.svg"
                        onClicked: {
                            console.log("Measurement clicked")
                            setActiveTab(measurementButton)
                        }
                    }

                    // 退出按钮
                    NavigationButton {
                        id: exitButton
                        text: "退出"
                        isExitButton: true
                        onClicked: {
                            console.log("Exit clicked")
                            exitDialog.visible = true
                        }
                    }