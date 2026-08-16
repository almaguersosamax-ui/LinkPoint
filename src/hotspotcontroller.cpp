#include "hotspotcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

#include <initializer_list>

HotspotController::HotspotController(QObject *parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &HotspotController::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &HotspotController::onProcessError);
}

QString HotspotController::engineName() const
{
    switch (m_engine) {
    case Engine::MobileHotspot:
        return tr("Mobile Hotspot");
    case Engine::HostedNetwork:
        return tr("Hosted Network");
    case Engine::Unknown:
        return tr("Detectando…");
    }
    return {};
}

bool HotspotController::containsAny(const QByteArray &text, std::initializer_list<const char *> needles)
{
    for (const char *needle : needles) {
        if (text.contains(needle))
            return true;
    }
    return false;
}

bool HotspotController::extractScripts()
{
    if (!m_scriptDir.isEmpty() && QFileInfo::exists(m_scriptDir))
        return true;

    const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_scriptDir = base + QStringLiteral("/LinkPoint/scripts");

    QDir dir(m_scriptDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;

    const QStringList scripts = {
        QStringLiteral("check_tethering.ps1"),
        QStringLiteral("start_tethering.ps1"),
        QStringLiteral("stop_tethering.ps1"),
        QStringLiteral("tethering_state.ps1"),
        QStringLiteral("hotspot_ip.ps1")
    };

    bool ok = true;
    QStringList missing;
    for (const QString &name : scripts) {
        QFile res(QStringLiteral(":/scripts/") + name);
        const QString dest = m_scriptDir + QLatin1Char('/') + name;
        if (QFileInfo::exists(dest))
            QFile::remove(dest);
        if (!res.open(QIODevice::ReadOnly)) {
            ok = false;
            missing << QStringLiteral(":/scripts/") + name;
            continue;
        }
        QFile out(dest);
        if (!out.open(QIODevice::WriteOnly)) {
            ok = false;
            missing << dest;
            res.close();
            continue;
        }
        out.write(res.readAll());
        out.close();
        res.close();
    }
    if (!missing.isEmpty())
        emit logMessage(tr("Recursos no disponibles: %1").arg(missing.join(QStringLiteral(", "))));
    return ok;
}

QString HotspotController::scriptPath(const QString &scriptName) const
{
    return m_scriptDir + QLatin1Char('/') + scriptName;
}

bool HotspotController::startPowerShellScript(const QString &scriptName)
{
    if (!extractScripts())
        return false;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("HOTSPOT_SSID"), m_ssid);
    env.insert(QStringLiteral("HOTSPOT_PASS"), m_passphrase);
    m_process->setProcessEnvironment(env);

    m_process->start(QStringLiteral("powershell"), QStringList({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-File"),
        scriptPath(scriptName)
    }));
    return true;
}

void HotspotController::setState(State state, const QString &detail)
{
    const bool changed = m_state != state;
    m_state = state;
    if (!detail.isEmpty())
        m_detail = detail;
    if (changed || !detail.isEmpty())
        emit stateChanged(m_state, m_detail);
}

void HotspotController::detect()
{
    if (isBusy())
        return;

    m_hostedSupported = false;
    m_tetheringAvailable = false;
    m_engine = Engine::Unknown;
    emit engineDetected(m_engine, tr("Detectando capacidades…"));

    m_task = Task::DetectHosted;
    m_process->start(QStringLiteral("netsh"), QStringList({
        QStringLiteral("wlan"), QStringLiteral("show"), QStringLiteral("drivers")
    }));
}

void HotspotController::start(const QString &ssid, const QString &passphrase)
{
    m_ssid = ssid;
    m_passphrase = passphrase;

    if (isBusy()) {
        setState(State::Error, tr("Operación anterior aún en curso."));
        return;
    }

    setState(State::Starting, tr("Iniciando hotspot…"));

    if (m_engine == Engine::HostedNetwork) {
        m_task = Task::StartHostedSet;
        m_process->start(QStringLiteral("netsh"), QStringList({
            QStringLiteral("wlan"),
            QStringLiteral("set"),
            QStringLiteral("hostednetwork"),
            QStringLiteral("mode=allow"),
            QStringLiteral("ssid=") + m_ssid,
            QStringLiteral("key=") + m_passphrase
        }));
    } else if (m_engine == Engine::MobileHotspot) {
        m_task = Task::StartTethering;
        if (!startPowerShellScript(QStringLiteral("start_tethering.ps1")))
            setState(State::Error, tr("No se pudo ejecutar el script de Mobile Hotspot."));
    } else {
        setState(State::Error, tr("Motor no disponible. Reintenta la detección."));
    }
}

void HotspotController::stop()
{
    if (m_state == State::Off)
        return;

    if (isBusy()) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }

    setState(State::Stopping, tr("Deteniendo hotspot…"));

    if (m_engine == Engine::HostedNetwork) {
        m_task = Task::StopHosted;
        m_process->start(QStringLiteral("netsh"), QStringList({
            QStringLiteral("wlan"), QStringLiteral("stop"), QStringLiteral("hostednetwork")
        }));
    } else if (m_engine == Engine::MobileHotspot) {
        m_task = Task::StopTethering;
        if (!startPowerShellScript(QStringLiteral("stop_tethering.ps1")))
            setState(State::Off);
    } else {
        setState(State::Off);
    }
}

void HotspotController::refreshStatus()
{
    if (isBusy())
        return;

    if (m_engine == Engine::HostedNetwork) {
        m_task = Task::StatusHosted;
        m_process->start(QStringLiteral("netsh"), QStringList({
            QStringLiteral("wlan"), QStringLiteral("show"), QStringLiteral("hostednetwork")
        }));
    } else if (m_engine == Engine::MobileHotspot) {
        m_task = Task::StatusTethering;
        startPowerShellScript(QStringLiteral("tethering_state.ps1"));
    }
}

void HotspotController::detectIp()
{
    if (isBusy())
        return;
    m_task = Task::DetectIp;
    startPowerShellScript(QStringLiteral("hotspot_ip.ps1"));
}

void HotspotController::parseHostedSupport(const QByteArray &output)
{
    m_hostedSupported = false;
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &line : lines) {
        if (containsAny(line, { "Hosted network support", "Soporte de red hospedada", "Red hospedada" })) {
            const int idx = line.indexOf(':');
            if (idx < 0)
                continue;
            const QByteArray value = line.mid(idx + 1).trimmed().toLower();
            m_hostedSupported = value.startsWith("yes") || value.startsWith("si") || value.startsWith("sí");
            break;
        }
    }
}

void HotspotController::parseHostedStatus(const QByteArray &output)
{
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &line : lines) {
        if (containsAny(line, { "Status", "Estado" })) {
            const int idx = line.indexOf(':');
            if (idx < 0)
                continue;
            const QByteArray value = line.mid(idx + 1).trimmed().toLower();
            const bool on = value.startsWith("started")
                || value.startsWith("iniciado")
                || value.startsWith("activo")
                || value.startsWith("activado");
            if (on)
                setState(State::On, tr("Hotspot activo."));
            else
                setState(State::Off, tr("Hotspot apagado."));
            break;
        }
    }
}

void HotspotController::onProcessFinished()
{
    const QByteArray out = m_process->readAllStandardOutput();
    const QByteArray err = m_process->readAllStandardError();
    const QByteArray all = out + err;

    switch (m_task) {
    case Task::DetectHosted:
        parseHostedSupport(all);
        if (extractScripts()) {
            m_task = Task::DetectTethering;
            startPowerShellScript(QStringLiteral("check_tethering.ps1"));
        } else {
            emit logMessage(tr("No se pudieron extraer los scripts de PowerShell."));
            m_engine = m_hostedSupported ? Engine::HostedNetwork : Engine::Unknown;
            emit engineDetected(m_engine, m_engine == Engine::HostedNetwork
                ? tr("Hosted Network disponible")
                : tr("Sin motor disponible"));
            if (m_engine == Engine::Unknown)
                setState(State::Error, tr("Sin motor disponible."));
        }
        break;

    case Task::DetectTethering:
        m_tetheringAvailable = all.contains("CAPABILITY=Enabled");
        if (m_tetheringAvailable && m_hostedSupported)
            m_engine = Engine::HostedNetwork;
        else if (m_tetheringAvailable)
            m_engine = Engine::MobileHotspot;
        else if (m_hostedSupported)
            m_engine = Engine::HostedNetwork;
        else
            m_engine = Engine::Unknown;

        {
            QString detail;
            if (m_engine == Engine::HostedNetwork)
                detail = tr("Hosted Network disponible (no requiere internet)");
            else if (m_engine == Engine::MobileHotspot)
                detail = tr("Mobile Hotspot disponible");
            else
                detail = tr("Ningún motor disponible: el adaptador no soporta punto de acceso.");
            emit engineDetected(m_engine, detail);
            if (m_engine == Engine::Unknown)
                setState(State::Error, detail);
        }
        break;

    case Task::StartHostedSet:
        if (m_process->exitCode() == 0) {
            m_task = Task::StartHosted;
            m_process->start(QStringLiteral("netsh"), QStringList({
                QStringLiteral("wlan"), QStringLiteral("start"), QStringLiteral("hostednetwork")
            }));
        } else {
            setState(State::Error, tr("netsh no pudo configurar la red hospedada.") + QLatin1Char(' ')
                + QString::fromUtf8(all).trimmed());
        }
        break;

    case Task::StartHosted:
        if (m_process->exitCode() == 0) {
            setState(State::On, tr("Hotspot iniciado (Hosted Network)."));
            detectIp();
        } else {
            setState(State::Error, tr("netsh no pudo iniciar la red hospedada.") + QLatin1Char(' ')
                + QString::fromUtf8(all).trimmed());
        }
        break;

    case Task::StartTethering:
        if (all.contains("STATUS=Success")) {
            setState(State::On, tr("Hotspot iniciado (Mobile Hotspot)."));
            detectIp();
        } else {
            setState(State::Error, tr("Mobile Hotspot rechazó el arranque: ")
                + QString::fromUtf8(all).trimmed());
        }
        break;

    case Task::StopHosted:
    case Task::StopTethering:
        setState(State::Off, tr("Hotspot detenido."));
        break;

    case Task::StatusHosted:
        parseHostedStatus(all);
        break;

    case Task::StatusTethering:
        if (all.contains("STATE=On"))
            setState(State::On, tr("Hotspot activo."));
        else
            setState(State::Off, tr("Hotspot apagado."));
        break;

    case Task::DetectIp: {
        static const QRegularExpression re(QStringLiteral("IP=([0-9]+\\.){3}[0-9]+"));
        const QRegularExpressionMatch match = re.match(QString::fromUtf8(all));
        if (match.hasMatch())
            emit ipDetected(match.captured(1).mid(3));
        break;
    }

    case Task::None:
        break;
    }
}

void HotspotController::onProcessError()
{
    switch (m_task) {
    case Task::StartHostedSet:
    case Task::StartHosted:
    case Task::StartTethering:
    case Task::StopHosted:
    case Task::StopTethering:
        setState(State::Error, tr("Error al ejecutar el comando: ") + m_process->errorString());
        break;
    default:
        emit logMessage(tr("Aviso: ") + m_process->errorString());
        break;
    }
    m_task = Task::None;
}
