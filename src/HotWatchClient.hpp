#ifndef HOTWATCHCLIENT_H
#define HOTWATCHCLIENT_H

#include <QtCore>
#include <QtQml>
#include <QtNetwork>
#include <QWebSocket>

class HotWatchClient : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString watchDir READ watchDir NOTIFY watchDirChanged)
    Q_PROPERTY(QString sourceFile READ sourceFile WRITE setSourceFile NOTIFY sourceFileChanged)
    Q_PROPERTY(QString defaultHost READ defaultHost WRITE setDefaultHost NOTIFY defaultHostChanged)

public:
    explicit HotWatchClient(QQmlEngine *engine = nullptr, QObject *parent = nullptr);
    ~HotWatchClient();

    static void registerQml();
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    QString serverUrl() const { return m_serverUrl; }
    void setServerUrl(const QString &url);
    bool isConnected() const { return m_connected; }
    QString watchDir() const { return m_watchDir; }
    QString sourceFile() const { return m_sourceFile; }
    void setSourceFile(const QString &file);
    QString defaultHost() const { return m_defaultHost; }
    void setDefaultHost(const QString &host);

    Q_INVOKABLE void connect();
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void findServer();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE QString getFileUrl() const;

    static HotWatchClient *getInstance() { return instance; }

signals:
    void serverUrlChanged();
    void connectedChanged();
    void watchDirChanged();
    void sourceFileChanged();
    void defaultHostChanged();
    void fileChanged(const QString &path);
    void error(const QString &message);

private slots:
    void handleConnected();
    void handleDisconnected();
    void handleTextMessage(const QString &message);
    void handleError(QAbstractSocket::SocketError error);
    void handleDiscoveryTimeout();
    void handleDiscoveryResponse();

private:
    void discoverServer();
    void updateConnection();
    QString convertToServerPath(const QString &localPath) const;
    QString convertToLocalPath(const QString &serverPath) const;
    void broadcastDiscovery();
    void setupDiscoverySocket();
    void sendErrorToServer(const QString &errorMsg);

    QQmlEngine *m_engine;
    QWebSocket m_webSocket;
    QString m_serverUrl;
    bool m_connected;
    QString m_watchDir;
    QString m_sourceFile;
    QString m_defaultHost;
    QList<QUdpSocket *> m_discoverySocketList;
    QTimer m_discoveryTimer;
    int m_discoveryAttempts;
    static const int MAX_DISCOVERY_ATTEMPTS = 3;
    static const int DISCOVERY_TIMEOUT = 1000; // ms
    static const quint16 DISCOVERY_PORT = 45454;
    QtMessageHandler originalMessageHandler;
    static HotWatchClient *instance;
};

#endif // HOTWATCHCLIENT_H
