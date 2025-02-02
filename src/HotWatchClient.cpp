#include "HotWatchClient.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QNetworkInterface>

// Initialize static member
HotWatchClient *HotWatchClient::instance = nullptr;

HotWatchClient::HotWatchClient(QQmlEngine *engine, QObject *parent)
    : QObject(parent), m_engine(engine), m_connected(false), m_discoveryAttempts(0), m_defaultHost("")
{
    instance = this;

    // Store original message handler
    originalMessageHandler = qInstallMessageHandler(HotWatchClient::messageHandler);

    // Si le moteur n'est pas fourni, essayer de le récupérer du contexte QML
    if (!m_engine)
    {
        m_engine = qmlEngine(this);
        if (!m_engine)
        {
            QQmlContext *context = qmlContext(this);
            if (context)
            {
                m_engine = context->engine();
            }
        }
    }

    QObject::connect(&m_webSocket, &QWebSocket::connected,
                     this, &HotWatchClient::handleConnected);
    QObject::connect(&m_webSocket, &QWebSocket::disconnected,
                     this, &HotWatchClient::handleDisconnected);
    QObject::connect(&m_webSocket, &QWebSocket::textMessageReceived,
                     this, &HotWatchClient::handleTextMessage);
    QObject::connect(&m_webSocket, &QWebSocket::errorOccurred,
                     this, &HotWatchClient::handleError);

    // Configuration du timer de découverte
    m_discoveryTimer.setSingleShot(true);
    m_discoveryTimer.setInterval(1000);
    QObject::connect(&m_discoveryTimer, &QTimer::timeout,
                     this, &HotWatchClient::handleDiscoveryTimeout);

    setupDiscoverySocket();
}

HotWatchClient::~HotWatchClient()
{
    disconnect();
    qDeleteAll(m_discoverySocketList);
}

void HotWatchClient::setupDiscoverySocket()
{
    qDebug() << "Setting up discovery sockets...";

    // Nettoyer les anciens sockets
    qDeleteAll(m_discoverySocketList);
    m_discoverySocketList.clear();

    // Créer un socket pour chaque interface réseau
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces)
    {
        // qDebug() << "\nChecking interface:" << interface.name()
        //          << "\n - Hardware Address:" << interface.hardwareAddress()
        //          << "\n - Flags:" << interface.flags();

        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            interface.flags().testFlag(QNetworkInterface::CanBroadcast) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {

            QList<QNetworkAddressEntry> entries = interface.addressEntries();

            for (const QNetworkAddressEntry &entry : entries)
            {
                QHostAddress address = entry.ip();
                if (address.protocol() == QAbstractSocket::IPv4Protocol)
                {
                    qDebug() << " - IP Address:" << address.toString();
                    //  << "\n - Netmask:" << entry.netmask().toString()
                    //  << "\n - Broadcast:" << entry.broadcast().toString();

                    QUdpSocket *socket = new QUdpSocket(this);
                    socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
                    socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);

                    if (socket->bind(address, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
                    {
                        socket->setMulticastInterface(interface);
                        QObject::connect(socket, &QUdpSocket::readyRead,
                                         this, &HotWatchClient::handleDiscoveryResponse);
                        m_discoverySocketList.append(socket);
                        qDebug() << " - Successfully bound discovery socket";
                    }
                    else
                    {
                        qDebug() << " - Failed to bind socket:" << socket->errorString();
                        delete socket;
                    }
                }
            }
        }
    }

    if (m_discoverySocketList.isEmpty())
    {
        qDebug() << "\nNo interfaces available, trying fallback to any address...";
        // Fallback: créer un socket sur toutes les interfaces
        QUdpSocket *socket = new QUdpSocket(this);
        socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
        socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);

        if (socket->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        {
            QObject::connect(socket, &QUdpSocket::readyRead,
                             this, &HotWatchClient::handleDiscoveryResponse);
            m_discoverySocketList.append(socket);
            qDebug() << "Successfully bound fallback discovery socket";
        }
        else
        {
            qDebug() << "Failed to bind fallback socket:" << socket->errorString();
            delete socket;
        }
    }

    qDebug() << "\nDiscovery setup completed with" << m_discoverySocketList.size() << "sockets";
}

void HotWatchClient::registerQml()
{
    qmlRegisterType<HotWatchClient>("HotWatch", 1, 0, "HotWatchClient");
}

void HotWatchClient::setServerUrl(const QString &url)
{
    if (m_serverUrl != url)
    {
        m_serverUrl = url;
        emit serverUrlChanged();
        updateConnection();
    }
}

void HotWatchClient::setSourceFile(const QString &file)
{
    if (m_sourceFile != file)
    {
        m_sourceFile = file;
        emit sourceFileChanged();
    }
}

QString HotWatchClient::getFileUrl() const
{
    if (m_serverUrl.isEmpty() || m_sourceFile.isEmpty())
    {
        return QString();
    }

    // Nettoyer l'URL du serveur
    QString cleanServerUrl = m_serverUrl;
    if (cleanServerUrl.startsWith(":"))
    {
        cleanServerUrl = cleanServerUrl.mid(1);
    }

    // Construire l'URL complète
    QUrl baseUrl(cleanServerUrl);
    if (!baseUrl.isValid())
    {
        qDebug() << "Invalid base URL:" << cleanServerUrl;
        return QString();
    }

    // S'assurer que l'URL de base se termine par un slash
    QString urlStr = baseUrl.toString();
    if (!urlStr.endsWith('/'))
    {
        urlStr += '/';
    }
    baseUrl = QUrl(urlStr);

    // Construire le chemin relatif
    QString path = convertToServerPath(m_sourceFile);
    if (path.startsWith('/'))
    {
        path = path.mid(1); // Enlever le slash initial car l'URL de base en a déjà un
    }

    // Résoudre l'URL complète
    QUrl fileUrl = baseUrl.resolved(QUrl(path));

    // Ajouter un paramètre de cache-busting
    QUrlQuery query;
    query.addQueryItem("v", QString::number(QDateTime::currentMSecsSinceEpoch()));
    fileUrl.setQuery(query);

    QString result = fileUrl.toString();
    qDebug() << "Generated file URL:" << result;
    return result;
}

void HotWatchClient::connect()
{
    if (m_serverUrl.isEmpty())
    {
        emit error("Server URL is not set");
        return;
    }

    QUrl wsUrl = QUrl(m_serverUrl);
    wsUrl.setScheme("ws");
    wsUrl.setPath("/ws");

    m_webSocket.open(wsUrl);
}

void HotWatchClient::disconnect()
{
    m_webSocket.close();
}

void HotWatchClient::findServer()
{
    discoverServer();
}

void HotWatchClient::clearCache()
{
    qDebug() << "Clearing QML component cache";
    if (!m_engine)
    {
        // Si le moteur n'est toujours pas disponible, essayer de le récupérer
        m_engine = qmlEngine(this);
        if (!m_engine)
        {
            QQmlContext *context = qmlContext(this);
            if (context)
            {
                m_engine = context->engine();
            }
        }
    }

    if (m_engine)
    {
        m_engine->clearComponentCache();
        m_engine->trimComponentCache();
        m_engine->collectGarbage();
        qDebug() << "QML component cache cleared";
    }
    else
    {
        qDebug() << "Warning: No QML engine available for cache clearing";
    }
}

void HotWatchClient::handleConnected()
{
    qDebug() << "WebSocket connected to server";
    m_connected = true;
    emit connectedChanged();

    // Envoyer un message de test
    QString testMsg = "{\"type\":\"hello\",\"client\":\"qt\"}";
    m_webSocket.sendTextMessage(testMsg);
}

void HotWatchClient::handleDisconnected()
{
    qDebug() << "WebSocket disconnected from server";
    m_connected = false;
    emit connectedChanged();

    // Redémarrer la découverte du serveur
    m_serverUrl.clear();
    emit serverUrlChanged();
    m_discoveryAttempts = 0;
    broadcastDiscovery();
}

void HotWatchClient::handleError(QAbstractSocket::SocketError error)
{
    qDebug() << "WebSocket error:" << error << "-" << m_webSocket.errorString();
    emit this->error(m_webSocket.errorString());

    // En cas d'erreur, on redémarre la découverte
    if (m_webSocket.state() != QAbstractSocket::UnconnectedState)
    {
        m_webSocket.close();
    }
    m_serverUrl.clear();
    emit serverUrlChanged();
    m_discoveryAttempts = 0;
    broadcastDiscovery();
}

void HotWatchClient::handleTextMessage(const QString &message)
{
    qDebug() << "Received WebSocket message:" << message;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
    {
        qDebug() << "Invalid JSON message received";
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    qDebug() << "Message type:" << type;

    if (type == "fileChanged")
    {
        QString path = obj["path"].toString();
        qDebug() << "File changed path:" << path;
        QString localPath = convertToLocalPath(path);
        qDebug() << "Local path:" << localPath;

        // S'assurer que le cache est bien nettoyé avant d'émettre le signal
        clearCache();
        QCoreApplication::processEvents();

        emit fileChanged(localPath);
    }
    else if (type == "connected")
    {
        qDebug() << "Received connection confirmation from server";
    }
}

void HotWatchClient::handleDiscoveryTimeout()
{
    m_discoveryAttempts++;
    if (m_discoveryAttempts < MAX_DISCOVERY_ATTEMPTS)
    {
        qDebug() << "Discovery attempt" << m_discoveryAttempts + 1 << "of" << MAX_DISCOVERY_ATTEMPTS;
        broadcastDiscovery();
    }
    else
    {
        qDebug() << "Server discovery failed after" << MAX_DISCOVERY_ATTEMPTS << "attempts";
        emit error("Failed to discover server");
        m_discoveryAttempts = 0;
    }
}

void HotWatchClient::handleDiscoveryResponse()
{
    QUdpSocket *socket = qobject_cast<QUdpSocket *>(sender());
    if (!socket)
    {
        qDebug() << "Invalid socket in discovery response";
        return;
    }

    while (socket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        socket->readDatagram(datagram.data(), datagram.size(),
                             &sender, &senderPort);

        QString response = QString::fromUtf8(datagram);
        qDebug() << "Received discovery response from" << sender.toString()
                 << ":" << senderPort << "-" << response;

        if (response.startsWith("HotWatchServer:"))
        {
            QString url = response.mid(14);
            if (url.startsWith(":"))
            {
                url = url.mid(1);
            }
            qDebug() << "Valid server response, URL:" << url;

            m_discoveryTimer.stop();
            m_discoveryAttempts = 0;
            setServerUrl(url);

            // Construire l'URL WebSocket
            QUrl wsUrl(url);
            if (!wsUrl.isValid())
            {
                qDebug() << "Invalid WebSocket URL:" << url;
                return;
            }

            // S'assurer que le schéma est ws://
            wsUrl.setScheme("ws");

            // Construire le chemin complet /ws
            QString path = wsUrl.path();
            if (path.isEmpty() || path == "/")
            {
                wsUrl.setPath("/ws");
            }
            else
            {
                if (!path.endsWith('/'))
                {
                    path += '/';
                }
                if (!path.endsWith("ws"))
                {
                    path += "ws";
                }
                wsUrl.setPath(path);
            }

            QString wsUrlStr = wsUrl.toString();
            qDebug() << "Attempting WebSocket connection to:" << wsUrlStr;
            m_webSocket.open(QUrl(wsUrlStr));
            return;
        }
        else
        {
            qDebug() << "Invalid server response format";
        }
    }
}

void HotWatchClient::discoverServer()
{
    setupDiscoverySocket();
    m_discoveryAttempts = 0;
    broadcastDiscovery();
}

void HotWatchClient::broadcastDiscovery()
{
    QByteArray datagram = "HotWatchDiscovery";
    bool sent = false;

    qDebug() << "Starting server discovery broadcast...";

    // Broadcast sur chaque interface
    for (QUdpSocket *socket : m_discoverySocketList)
    {
        QHostAddress localAddress = socket->localAddress();

        // Trouver l'interface correspondante
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &interface : interfaces)
        {
            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries)
            {
                if (entry.ip() == localAddress)
                {
                    qDebug() << "Found matching interface:" << interface.name()
                             << "Local address:" << localAddress.toString()
                             << "Broadcast:" << entry.broadcast().toString();

                    // Envoyer sur l'adresse de broadcast spécifique
                    if (socket->writeDatagram(datagram, entry.broadcast(), DISCOVERY_PORT) > 0)
                    {
                        sent = true;
                        qDebug() << "Successfully sent discovery packet to:" << entry.broadcast().toString();
                    }
                    else
                    {
                        qDebug() << "Failed to send discovery packet:" << socket->errorString();
                    }
                }
            }
        }

        // Essayer aussi le broadcast général
        QHostAddress broadcastAddr(QHostAddress::Broadcast);
        if (socket->writeDatagram(datagram, broadcastAddr, DISCOVERY_PORT) > 0)
        {
            sent = true;
            qDebug() << "Successfully sent general broadcast discovery packet";
        }
        else
        {
            qDebug() << "Failed to send general broadcast packet:" << socket->errorString();
        }
    }

    if (sent)
    {
        qDebug() << "Discovery broadcast completed, waiting for responses...";
        m_discoveryTimer.start();
    }
    else
    {
        qDebug() << "Failed to send any discovery broadcasts";
        emit error("Failed to send discovery broadcast");
    }
}

void HotWatchClient::updateConnection()
{
    qDebug() << "Updating connection, server URL:" << m_serverUrl << "default host:" << m_defaultHost;
    if (m_connected)
    {
        disconnect();
    }

    // Si nous avons une URL de serveur existante, l'utiliser
    if (!m_serverUrl.isEmpty())
    {
        connect();
    }
    // Sinon, si nous avons un hôte par défaut, l'utiliser
    else if (!m_defaultHost.isEmpty())
    {
        QString url = m_defaultHost;
        if (!url.startsWith("http://") && !url.startsWith("https://"))
        {
            url = "http://" + url;
        }
        setServerUrl(url);
    }
    // En dernier recours, démarrer la découverte automatique
    else
    {
        m_discoveryAttempts = 0;
        broadcastDiscovery();
    }
}

void HotWatchClient::setDefaultHost(const QString &host)
{
    if (m_defaultHost != host)
    {
        m_defaultHost = host;
        emit defaultHostChanged();
        updateConnection();
    }
}

QString HotWatchClient::convertToServerPath(const QString &localPath) const
{
    QString path = localPath;
    if (path.startsWith("file:///"))
    {
        path = path.mid(7);
    }
    if (path.startsWith(m_watchDir))
    {
        path = path.mid(m_watchDir.length());
    }
    if (!path.startsWith("/"))
    {
        path = "/" + path;
    }
    return path;
}

QString HotWatchClient::convertToLocalPath(const QString &serverPath) const
{
    QString path = serverPath;
    if (!path.startsWith("/"))
    {
        path = "/" + path;
    }
    return m_watchDir + path;
}

void HotWatchClient::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Get the instance
    HotWatchClient *instance = HotWatchClient::getInstance();
    if (!instance)
    {
        return;
    }

    // Format error message
    QString errorMsg;
    switch (type)
    {
    case QtDebugMsg:
        errorMsg = QString("Debug: %1 (%2:%3").arg(msg).arg(context.file).arg(context.line);
        break;
    case QtInfoMsg:
        errorMsg = QString("Info: %1 (%2:%3").arg(msg).arg(context.file).arg(context.line);
        break;
    case QtWarningMsg:
        errorMsg = QString("Warning: %1 (%2:%3)").arg(msg).arg(context.file).arg(context.line);
        break;
    case QtCriticalMsg:
        errorMsg = QString("Critical: %1 (%2:%3)").arg(msg).arg(context.file).arg(context.line);
        break;
    case QtFatalMsg:
        errorMsg = QString("Fatal: %1 (%2:%3)").arg(msg).arg(context.file).arg(context.line);
        break;
    }

    // Send error to server
    instance->sendErrorToServer(errorMsg);

    // Call original handler
    if (instance->originalMessageHandler)
    {
        instance->originalMessageHandler(type, context, msg);
    }
}

void HotWatchClient::sendErrorToServer(const QString &errorMsg)
{
    if (!m_connected)
    {
        return;
    }

    QJsonObject obj;
    obj["type"] = "error";
    obj["message"] = errorMsg;

    QJsonDocument doc(obj);
    QString jsonString = doc.toJson();

    m_webSocket.sendTextMessage(jsonString);
}
