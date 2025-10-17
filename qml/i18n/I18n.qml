pragma Singleton
import QtQuick 2.15
import Qt.labs.settings 1.1
import "strings_zh.js" as Zh
import "strings_en.js" as En

Item {
    id: root

    // zh or en
    property string language: settings.language
    property var dict: ({})

    Settings { id: settings; category: "i18n"; property string language: "zh" }

    function loadDict(lang) {
        dict = (lang === 'en') ? En.strings : Zh.strings
    }

    function setLanguage(lang) {
        if (lang !== 'zh' && lang !== 'en') lang = 'zh'
        if (settings.language === lang) return
        settings.language = lang
        language = lang
        loadDict(language)
    }

    function tr(key, fallback) {
        if (!dict || !dict.hasOwnProperty(key)) return fallback
        return dict[key]
    }

    Component.onCompleted: {
        loadDict(language)
    }
}
