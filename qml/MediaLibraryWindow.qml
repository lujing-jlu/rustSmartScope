import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "components"
import "i18n" 1.0 as I18n
import RustSmartScope.Logger 1.0

StandardAppWindow {
    id: mediaLibWindow
    windowTitle: I18n.I18n.tr("win.media.title", "SmartScope Industrial - 媒体库")
    contentSource: "pages/MediaLibraryPage.qml"
}
