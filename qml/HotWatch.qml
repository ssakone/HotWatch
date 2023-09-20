import QtQuick
import QtQuick.Controls
import HotWatchBackend

Item {
    id: control
    property string sourceFile: ""
    property bool active: false
    function getSource() {
        return "file:///" + HotWatcher.sourceFile(
                    ) + sourceFile + "?q=" + Math.random()
    }

    Connections {
        target: HotWatcher
        function onFileChange(path) {
            console.log("[HOTWATCH]", path)
            loader.reload()
        }
    }

    Loader {
        anchors.fill: parent
        id: loader
        source: control.getSource()
        asynchronous: true
        function reload() {
            HotWatcher.clearCache()
            source = ""
            source = control.getSource()
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: Loader.Loading == loader.status
        }
    }
    Component.onCompleted: {
        if (control.active) {
            HotWatcher.setup()
        }
    }
}
