#pragma once

#include <QByteArray>
#include <QDir>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTcpServer>

class QTcpSocket;

class HttpFileServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpFileServer(QObject *parent = nullptr);

    bool isRunning() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }
    QString root() const { return m_root; }
    void setRoot(const QString &root) { m_root = root; }

public slots:
    bool start(quint16 port);
    void stop();

signals:
    void logMessage(const QString &message);
    void stateChanged(bool running);

private slots:
    void onNewConnection();
    void onSocketReadyRead();

private:
    void handleRequest(QTcpSocket *socket, const QByteArray &request);
    void sendDirectoryListing(QTcpSocket *socket, const QDir &dir, const QString &urlPath, bool head);
    void sendFile(QTcpSocket *socket, const QFileInfo &info, bool head);
    void sendSimple(QTcpSocket *socket, const QByteArray &statusLine, const QByteArray &body,
                   const QByteArray &contentType, bool head);
    static QString contentTypeFor(const QString &fileName);
    static QString htmlEscape(const QString &text);

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QSet<QTcpSocket *> m_sockets;
    QString m_root;
};
