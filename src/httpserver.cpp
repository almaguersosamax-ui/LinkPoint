#include "httpserver.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStringList>
#include <QTcpSocket>
#include <QUrl>

HttpFileServer::HttpFileServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &HttpFileServer::onNewConnection);
}

bool HttpFileServer::start(quint16 port)
{
    stop();

    if (m_root.isEmpty()) {
        emit logMessage(tr("No hay carpeta seleccionada para compartir."));
        return false;
    }
    if (!QDir(m_root).exists()) {
        emit logMessage(tr("La carpeta seleccionada no existe: %1").arg(m_root));
        return false;
    }
    if (!m_server.listen(QHostAddress::Any, port)) {
        emit logMessage(tr("No se pudo abrir el puerto %1: %2").arg(port).arg(m_server.errorString()));
        return false;
    }

    emit logMessage(tr("Servidor HTTP activo en el puerto %1 — carpeta: %2").arg(port).arg(m_root));
    emit stateChanged(true);
    return true;
}

void HttpFileServer::stop()
{
    if (!m_server.isListening())
        return;

    const QList<QTcpSocket *> sockets = m_sockets.values();
    for (QTcpSocket *socket : sockets)
        socket->disconnectFromHost();
    m_sockets.clear();
    m_server.close();
    emit logMessage(tr("Servidor HTTP detenido."));
    emit stateChanged(false);
}

void HttpFileServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        m_sockets.insert(socket);
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &HttpFileServer::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_sockets.remove(socket);
            m_buffers.remove(socket);
        });
    }
}

void HttpFileServer::onSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();

    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        if (buffer.size() > 64 * 1024) {
            socket->disconnectFromHost();
            m_buffers.remove(socket);
        }
        return;
    }

    const QByteArray header = buffer.left(headerEnd);
    m_buffers.remove(socket);
    handleRequest(socket, header);
}

void HttpFileServer::handleRequest(QTcpSocket *socket, const QByteArray &request)
{
    QObject::disconnect(socket, &QTcpSocket::readyRead, this, &HttpFileServer::onSocketReadyRead);

    const QList<QByteArray> lines = request.split('\n');
    if (lines.isEmpty()) {
        sendSimple(socket, "HTTP/1.1 400 Bad Request", "Bad request", "text/plain; charset=utf-8", false);
        return;
    }

    const QList<QByteArray> parts = lines.first().trimmed().split(' ');
    if (parts.size() < 2) {
        sendSimple(socket, "HTTP/1.1 400 Bad Request", "Bad request", "text/plain; charset=utf-8", false);
        return;
    }

    const QByteArray method = parts.at(0);
    const bool head = method == "HEAD";
    if (method != "GET" && method != "HEAD") {
        sendSimple(socket, "HTTP/1.1 405 Method Not Allowed", "Method not allowed",
                   "text/plain; charset=utf-8", head);
        return;
    }

    QByteArray target = parts.at(1);
    const int queryPos = target.indexOf('?');
    if (queryPos >= 0)
        target = target.left(queryPos);

    QString rel = QDir::cleanPath(QUrl::fromPercentEncoding(target));
    if (rel.startsWith(QLatin1Char('/')))
        rel.remove(0, 1);
    if (rel == QStringLiteral("."))
        rel.clear();

    if (rel.startsWith(QStringLiteral("..")) || rel.contains(QStringLiteral(":/"))
        || rel.contains(QLatin1Char('\\'))) {
        sendSimple(socket, "HTTP/1.1 403 Forbidden", "Forbidden", "text/plain; charset=utf-8", head);
        return;
    }

    if (m_root.isEmpty()) {
        sendSimple(socket, "HTTP/1.1 404 Not Found", "Not found", "text/plain; charset=utf-8", head);
        return;
    }

    const QString fullPath = QDir(m_root).filePath(rel);
    const QFileInfo info(fullPath);
    if (!info.exists() || !info.isReadable()) {
        sendSimple(socket, "HTTP/1.1 404 Not Found", "Not found", "text/plain; charset=utf-8", head);
        return;
    }

    if (info.isDir()) {
        const QString urlPath = QStringLiteral("/") + rel;
        sendDirectoryListing(socket, QDir(info.absoluteFilePath()), urlPath, head);
    } else {
        sendFile(socket, info, head);
    }
}

void HttpFileServer::sendDirectoryListing(QTcpSocket *socket, const QDir &dir,
                                          const QString &urlPath, bool head)
{
    const QString relPath = QDir(m_root).relativeFilePath(dir.absolutePath()).replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QString displayPath = relPath == QStringLiteral(".") ? QStringLiteral("/") : QStringLiteral("/") + relPath;
    const QString parentPath = displayPath == QStringLiteral("/")
        ? QString()
        : QDir::cleanPath(displayPath + QStringLiteral("/.."));

    QByteArray html;
    html += "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>LinkPoint - " + htmlEscape(displayPath).toUtf8() + "</title>";
    html += "<style>"
            "body{background:#0B0E14;color:#E8EEF6;font-family:'Segoe UI',sans-serif;margin:0;padding:24px}"
            "h1{font-size:18px;font-weight:600;margin:0 0 16px}"
            "table{width:100%;border-collapse:collapse;font-size:13px}"
            "td,th{padding:8px 10px;border-bottom:1px solid #232C3A;text-align:left}"
            "th{color:#8B9BB0;font-size:11px;text-transform:uppercase;letter-spacing:1px}"
            "a{color:#2DD4BF;text-decoration:none}a:hover{text-decoration:underline}"
            ".size{color:#64748B}.muted{color:#5C6B7E;font-size:12px;margin-top:18px}"
            "</style></head><body>";
    html += "<h1>&#128193; " + htmlEscape(displayPath).toUtf8() + "</h1>";
    html += "<table><thead><tr><th>Nombre</th><th>Tamaño</th><th>Modificado</th></tr></thead><tbody>";

    if (!parentPath.isEmpty()) {
        const QString parentHref = QString::fromUtf8(QUrl::toPercentEncoding(parentPath));
        html += "<tr><td><a href=\"" + parentHref.toUtf8() + "\">&#11014; ..</a></td>"
                "<td class=\"size\">—</td><td class=\"size\">—</td></tr>";
    }

    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::DirsFirst | QDir::Name);

    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        QString href = urlPath;
        if (!href.endsWith(QLatin1Char('/')))
            href += QLatin1Char('/');
        href += QString::fromUtf8(QUrl::toPercentEncoding(name));

        const bool isDir = entry.isDir();
        const QString sizeText = isDir
            ? QStringLiteral("—")
            : QLocale().formattedDataSize(entry.size());
        const QString modified = entry.lastModified().toString(QStringLiteral("dd/MM/yyyy HH:mm"));
        const QString icon = isDir ? QStringLiteral("&#128193;") : QStringLiteral("&#128196;");

        html += "<tr><td><a href=\"" + href.toUtf8() + "\">" + icon.toUtf8() + " "
                + htmlEscape(name).toUtf8() + (isDir ? "/" : "") + "</a></td>";
        html += "<td class=\"size\">" + sizeText.toUtf8() + "</td>";
        html += "<td class=\"size\">" + modified.toUtf8() + "</td></tr>";
    }

    html += "</tbody></table>";
    html += "<div class=\"muted\">LinkPoint &middot; servidor de archivos</div>";
    html += "</body></html>";

    sendSimple(socket, "HTTP/1.1 200 OK", html, "text/html; charset=utf-8", head);
}

void HttpFileServer::sendFile(QTcpSocket *socket, const QFileInfo &info, bool head)
{
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        sendSimple(socket, "HTTP/1.1 404 Not Found", "Not found", "text/plain; charset=utf-8", head);
        return;
    }

    QByteArray response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + contentTypeFor(info.fileName()).toUtf8() + "\r\n";
    response += "Content-Length: " + QByteArray::number(file.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "Accept-Ranges: bytes\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "\r\n";
    socket->write(response);

    if (!head) {
        QByteArray chunk;
        while (!file.atEnd()) {
            chunk = file.read(64 * 1024);
            socket->write(chunk);
            if (socket->bytesToWrite() > 512 * 1024)
                socket->waitForBytesWritten(100);
        }
    }

    socket->flush();
    socket->disconnectFromHost();
    socket->deleteLater();
}

void HttpFileServer::sendSimple(QTcpSocket *socket, const QByteArray &statusLine,
                                const QByteArray &body, const QByteArray &contentType, bool head)
{
    QByteArray response = statusLine + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "\r\n";
    if (!head)
        response += body;
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
    socket->deleteLater();
}

QString HttpFileServer::contentTypeFor(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    static const QHash<QString, QString> types = {
        { "html", "text/html; charset=utf-8" },
        { "htm", "text/html; charset=utf-8" },
        { "txt", "text/plain; charset=utf-8" },
        { "md", "text/markdown; charset=utf-8" },
        { "css", "text/css" },
        { "js", "application/javascript" },
        { "json", "application/json" },
        { "xml", "application/xml" },
        { "csv", "text/csv" },
        { "png", "image/png" },
        { "jpg", "image/jpeg" },
        { "jpeg", "image/jpeg" },
        { "gif", "image/gif" },
        { "bmp", "image/bmp" },
        { "webp", "image/webp" },
        { "ico", "image/x-icon" },
        { "svg", "image/svg+xml" },
        { "pdf", "application/pdf" },
        { "zip", "application/zip" },
        { "rar", "application/vnd.rar" },
        { "7z", "application/x-7z-compressed" },
        { "tar", "application/x-tar" },
        { "gz", "application/gzip" },
        { "mp3", "audio/mpeg" },
        { "wav", "audio/wav" },
        { "ogg", "audio/ogg" },
        { "m4a", "audio/mp4" },
        { "mp4", "video/mp4" },
        { "mkv", "video/x-matroska" },
        { "avi", "video/x-msvideo" },
        { "mov", "video/quicktime" },
        { "webm", "video/webm" },
        { "apk", "application/vnd.android.package-archive" },
        { "exe", "application/octet-stream" },
        { "dll", "application/octet-stream" },
        { "iso", "application/octet-stream" },
        { "doc", "application/msword" },
        { "docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document" },
        { "xls", "application/vnd.ms-excel" },
        { "xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" },
        { "ppt", "application/vnd.ms-powerpoint" },
        { "pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation" }
    };
    return types.value(ext, QStringLiteral("application/octet-stream"));
}

QString HttpFileServer::htmlEscape(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return escaped;
}
