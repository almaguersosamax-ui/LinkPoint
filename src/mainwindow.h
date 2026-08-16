#pragma once

#include <QIcon>
#include <QMainWindow>
#include <QString>
#include <QTimer>

#include "hotspotcontroller.h"
#include "httpserver.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onToggleHotspot();
    void onToggleServer();
    void onPickFolder();
    void onCopyIp();
    void onCopyUrl();
    void onOpenUrl();
    void onLaunchRdp();
    void onStatusRefresh();
    void onHotspotStateChanged(HotspotController::State state, const QString &detail);
    void onIpDetected(const QString &ip);
    void log(const QString &message);

private:
    void buildUi();
    void applyStyle();
    void loadSettings();
    void saveSettings();
    void updateHotspotUi();
    void updateServerUi();
    void updateUrl();
    void refreshIp();
    bool startServer();
    static void setDot(QLabel *dot, const QString &state);
    static void repolish(QWidget *widget);
    QIcon makeAppIcon() const;
    QString serverUrl() const;

    HotspotController m_controller;
    HttpFileServer m_server;
    QTimer m_statusTimer;
    int m_pollTick = 0;

    QLabel *m_engineLabel = nullptr;
    QLabel *m_statusDot = nullptr;
    QLabel *m_statusText = nullptr;
    QLineEdit *m_ssidEdit = nullptr;
    QLineEdit *m_passEdit = nullptr;
    QPushButton *m_showPassBtn = nullptr;
    QPushButton *m_toggleHotspotBtn = nullptr;
    QLabel *m_ipValue = nullptr;
    QLabel *m_urlValue = nullptr;
    QPushButton *m_copyIpBtn = nullptr;
    QPushButton *m_copyUrlBtn = nullptr;
    QPushButton *m_openUrlBtn = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLabel *m_folderLabel = nullptr;
    QPushButton *m_pickFolderBtn = nullptr;
    QLabel *m_serverDot = nullptr;
    QLabel *m_serverStatusLabel = nullptr;
    QPushButton *m_toggleServerBtn = nullptr;
    QPlainTextEdit *m_logView = nullptr;

    QString m_ip = QStringLiteral("192.168.137.1");
    QString m_folder;
};
