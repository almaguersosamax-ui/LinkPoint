#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class HotspotController : public QObject
{
    Q_OBJECT

public:
    enum class Engine {
        Unknown,
        MobileHotspot,
        HostedNetwork
    };
    Q_ENUM(Engine)

    enum class State {
        Off,
        Starting,
        On,
        Stopping,
        Error
    };
    Q_ENUM(State)

    explicit HotspotController(QObject *parent = nullptr);

    Engine engine() const { return m_engine; }
    State state() const { return m_state; }
    bool isBusy() const { return m_process->state() != QProcess::NotRunning; }
    QString engineName() const;

public slots:
    void detect();
    void start(const QString &ssid, const QString &passphrase);
    void stop();
    void refreshStatus();
    void detectIp();

signals:
    void engineDetected(Engine engine, const QString &detail);
    void stateChanged(State state, const QString &detail);
    void ipDetected(const QString &ip);
    void logMessage(const QString &message);

private slots:
    void onProcessFinished();
    void onProcessError();

private:
    enum class Task {
        None,
        DetectHosted,
        DetectTethering,
        StartHostedSet,
        StartHosted,
        StartTethering,
        StopHosted,
        StopTethering,
        StatusHosted,
        StatusTethering,
        DetectIp
    };

    void setState(State state, const QString &detail = QString());
    bool startPowerShellScript(const QString &scriptName);
    void parseHostedSupport(const QByteArray &output);
    void parseHostedStatus(const QByteArray &output);
    QString tetheringReasonText(const QString &capability) const;
    QString scriptPath(const QString &scriptName) const;
    bool extractScripts();
    static bool containsAny(const QByteArray &text, std::initializer_list<const char *> needles);

    QProcess *m_process = nullptr;
    Task m_task = Task::None;
    Engine m_engine = Engine::Unknown;
    State m_state = State::Off;
    QString m_scriptDir;
    QString m_ssid;
    QString m_passphrase;
    QString m_detail;
    bool m_hostedSupported = false;
    bool m_tetheringAvailable = false;
    QString m_adapterName;
    QString m_hostedText;
    QString m_tetheringReason;
};
