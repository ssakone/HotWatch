import QtQuick
import QtQuick.Controls
import HotWatch

Item {
    id: root

    property bool active: false
    property string sourceFile
    property alias loaderItem: loader
    property bool hasError: false
    property string errorMessage: ""
    property alias defaultHost: client.defaultHost

    HotWatchClient {
        id: client
        sourceFile: root.sourceFile

        onFileChanged: function (path) {
            console.log("File changed, reloading:", path)
            loader.reload()
        }

        onError: function (message) {
            console.error("Error:", message)
            root.hasError = true
            root.errorMessage = message
        }

        onConnectedChanged: function () {
            console.log("Connection state changed:", connected)
            if (connected && loader.status === Loader.Null) {
                loader.reload()
            }
        }

        Component.onCompleted: {
            console.log("Starting server discovery")
            if (defaultHost == "")
                findServer()
        }
    }

    Loader {
        id: loader
        anchors.fill: parent
        asynchronous: true
        source: client.getFileUrl()

        onStatusChanged: {
            console.log("Loader status:", status, source)
            if (status === Loader.Error) {
                console.error("Failed to load:", source)
                root.hasError = true
                root.errorMessage = "Failed to load: " + source
            } else if (status === Loader.Ready) {
                root.hasError = false
                root.errorMessage = ""
            }
        }

        onLoaded: {
            console.log("Successfully loaded:", source)
        }

        function reload() {
            console.log("Starting reload sequence")
            // Forcer le déchargement
            source = ""
            client.clearCache()

            // Attendre que le composant soit déchargé
            if (status === Loader.Null) {
                reloadTimer.restart()
            } else {
                // Si pas encore déchargé, attendre le prochain changement de status
                statusChangeConnection.target = loader
            }
        }
    }

    // Connexion pour gérer le déchargement asynchrone
    Connections {
        id: statusChangeConnection
        target: null

        function onStatusChanged() {
            if (loader.status === Loader.Null) {
                target = null
                reloadTimer.restart()
            }
        }
    }

    Timer {
        id: reloadTimer
        interval: 100
        onTriggered: {
            console.log("Reloading with new source:", client.getFileUrl())
            loader.source = client.getFileUrl()
        }
    }

    Rectangle {
        visible: root.hasError
        anchors.fill: parent
        color: "#33FF0000"
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 10

            Text {
                text: "Error Loading Component"
                color: "#FF0000"
                font.bold: true
            }

            Text {
                text: root.errorMessage
                color: "#FF0000"
                width: parent.width
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                text: "Reload"
                width: 120
                height: 50
                palette.text: "#FFFFFF"
                background: Rectangle {
                    color: parent.hovered ? "#FF0000" : "#FF3333"
                    radius: 5
                }
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: {
                    root.errorMessage = "Reloading..."
                    client.findServer()
                    loader.reload()
                }
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: loader.status === Loader.Loading
    }

    function reload() {
        loader.reload()
    }

    onActiveChanged: {
        if (active) {
            client.findServer()
        } else {
            client.disconnect()
        }
    }

    Component.onCompleted: {
        if (active) {
            client.findServer()
        }
    }
}
