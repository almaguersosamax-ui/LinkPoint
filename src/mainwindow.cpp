#include "mainwindow.h"

#include "elevation.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <QProcess>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("LinkPoint — Hotspot personal"));
    setWindowIcon(makeAppIcon());

    buildUi();
    applyStyle();
    loadSettings();
    updateUrl();
    updateServerUi();

    connect(&m_controller, &HotspotController::stateChanged,
            this, &MainWindow::onHotspotStateChanged);
    connect(&m_controller, &HotspotController::engineDetected,
            this, [this](HotspotController::Engine, const QString &detail) {
        m_engineLabel->setText(m_controller.engineName());
        m_engineLabel->setToolTip(detail);
    });
    connect(&m_controller, &HotspotController::ipDetected,
            this, &MainWindow::onIpDetected);
    connect(&m_controller, &HotspotController::logMessage,
            this, &MainWindow::log);
    connect(&m_server, &HttpFileServer::logMessage,
            this, &MainWindow::log);

    m_statusTimer.setInterval(3000);
    connect(&m_statusTimer, &QTimer::timeout, this, &MainWindow::onStatusRefresh);

    log(tr("LinkPoint iniciado. Detectando capacidades del adaptador…"));
    m_controller.detect();
}

QIcon MainWindow::makeAppIcon() const
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath background;
    background.addRoundedRect(QRectF(2, 2, 60, 60), 15, 15);
    painter.fillPath(background, QColor(QStringLiteral("#14B8A6")));

    QPen pen(QColor(QStringLiteral("#FFFFFF")));
    pen.setWidthF(4.0);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    const QPointF center(32, 38);
    const double radii[] = { 16.0, 10.5, 5.0 };
    for (double radius : radii) {
        painter.drawArc(QRectF(center.x() - radius, center.y() - radius,
                               radius * 2, radius * 2),
                        210 * 16, 120 * 16);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
    painter.drawEllipse(center, 2.5, 2.5);
    painter.end();

    return QIcon(pixmap);
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralWidget"));
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(16, 14, 16, 12);
    root->setSpacing(10);

    // Header
    auto *header = new QHBoxLayout();
    header->setSpacing(10);
    auto *iconLabel = new QLabel();
    iconLabel->setPixmap(makeAppIcon().pixmap(30, 30));
    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(0);
    auto *title = new QLabel(tr("LinkPoint"));
    title->setObjectName(QStringLiteral("appTitle"));
    auto *subtitle = new QLabel(tr("Hotspot personal + compartición de archivos"));
    subtitle->setObjectName(QStringLiteral("appSubtitle"));
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    header->addWidget(iconLabel);
    header->addLayout(titleBox);
    header->addStretch();
    m_engineLabel = new QLabel(tr("Detectando…"));
    m_engineLabel->setObjectName(QStringLiteral("engineBadge"));
    m_engineLabel->setAlignment(Qt::AlignCenter);
    header->addWidget(m_engineLabel);
    root->addLayout(header);

    if (!isElevated()) {
        auto *banner = new QFrame();
        banner->setObjectName(QStringLiteral("warningBanner"));
        auto *bannerLayout = new QHBoxLayout(banner);
        bannerLayout->setContentsMargins(10, 8, 10, 8);
        bannerLayout->setSpacing(8);
        auto *bannerText = new QLabel(
            tr("Sin permisos de administrador: el hotspot no podrá iniciar."));
        bannerText->setObjectName(QStringLiteral("bannerText"));
        bannerText->setWordWrap(true);
        auto *elevateButton = new QPushButton(tr("Reiniciar como administrador"));
        elevateButton->setObjectName(QStringLiteral("ghostBtn"));
        elevateButton->setCursor(Qt::PointingHandCursor);
        connect(elevateButton, &QPushButton::clicked, this, []() {
            if (relaunchAsAdmin())
                qApp->quit();
        });
        bannerLayout->addWidget(bannerText, 1);
        bannerLayout->addWidget(elevateButton);
        root->insertWidget(1, banner);
    }

    // Card 1 — Punto de acceso
    auto *card1 = new QFrame();
    card1->setObjectName(QStringLiteral("card"));
    auto *c1 = new QVBoxLayout(card1);
    c1->setContentsMargins(14, 12, 14, 14);
    c1->setSpacing(8);

    auto *section1 = new QLabel(tr("PUNTO DE ACCESO"));
    section1->setObjectName(QStringLiteral("sectionTitle"));
    c1->addWidget(section1);

    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(8);
    m_statusDot = new QLabel();
    m_statusDot->setObjectName(QStringLiteral("statusDot"));
    m_statusDot->setFixedSize(12, 12);
    m_statusDot->setProperty("state", QStringLiteral("off"));
    m_statusText = new QLabel(tr("Apagado"));
    m_statusText->setObjectName(QStringLiteral("statusText"));
    statusRow->addWidget(m_statusDot);
    statusRow->addWidget(m_statusText);
    statusRow->addStretch();
    c1->addLayout(statusRow);

    auto *ssidRow = new QHBoxLayout();
    ssidRow->setSpacing(8);
    auto *ssidLabel = new QLabel(tr("Nombre (SSID)"));
    ssidLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_ssidEdit = new QLineEdit();
    m_ssidEdit->setPlaceholderText(tr("Ej.: PC-WIFI"));
    m_ssidEdit->setMaxLength(32);
    ssidRow->addWidget(ssidLabel);
    ssidRow->addWidget(m_ssidEdit, 1);
    c1->addLayout(ssidRow);

    auto *passRow = new QHBoxLayout();
    passRow->setSpacing(8);
    auto *passLabel = new QLabel(tr("Contraseña"));
    passLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_passEdit = new QLineEdit();
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setMaxLength(63);
    m_showPassBtn = new QPushButton(tr("Mostrar"));
    m_showPassBtn->setObjectName(QStringLiteral("ghostBtn"));
    m_showPassBtn->setCursor(Qt::PointingHandCursor);
    connect(m_showPassBtn, &QPushButton::clicked, this, [this]() {
        const bool show = m_passEdit->echoMode() == QLineEdit::Password;
        m_passEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
        m_showPassBtn->setText(show ? tr("Ocultar") : tr("Mostrar"));
    });
    passRow->addWidget(passLabel);
    passRow->addWidget(m_passEdit, 1);
    passRow->addWidget(m_showPassBtn);
    c1->addLayout(passRow);

    m_toggleHotspotBtn = new QPushButton(tr("Iniciar Hotspot"));
    m_toggleHotspotBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_toggleHotspotBtn->setCursor(Qt::PointingHandCursor);
    m_toggleHotspotBtn->setMinimumHeight(42);
    connect(m_toggleHotspotBtn, &QPushButton::clicked, this, &MainWindow::onToggleHotspot);
    c1->addWidget(m_toggleHotspotBtn);
    root->addWidget(card1);

    // Card 2 — Acceso desde el teléfono
    auto *card2 = new QFrame();
    card2->setObjectName(QStringLiteral("card"));
    auto *c2 = new QVBoxLayout(card2);
    c2->setContentsMargins(14, 12, 14, 14);
    c2->setSpacing(8);

    auto *section2 = new QLabel(tr("ACCESO DESDE EL TELÉFONO"));
    section2->setObjectName(QStringLiteral("sectionTitle"));
    c2->addWidget(section2);

    auto *ipRow = new QHBoxLayout();
    ipRow->setSpacing(8);
    auto *ipLabel = new QLabel(tr("IP del equipo"));
    ipLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_ipValue = new QLabel(m_ip);
    m_ipValue->setObjectName(QStringLiteral("valueLabel"));
    m_ipValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_copyIpBtn = new QPushButton(tr("Copiar"));
    m_copyIpBtn->setObjectName(QStringLiteral("copyBtn"));
    m_copyIpBtn->setCursor(Qt::PointingHandCursor);
    connect(m_copyIpBtn, &QPushButton::clicked, this, &MainWindow::onCopyIp);
    ipRow->addWidget(ipLabel);
    ipRow->addStretch();
    ipRow->addWidget(m_ipValue);
    ipRow->addWidget(m_copyIpBtn);
    c2->addLayout(ipRow);

    auto *urlRow = new QHBoxLayout();
    urlRow->setSpacing(8);
    auto *urlLabel = new QLabel(tr("Dirección"));
    urlLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_urlValue = new QLabel();
    m_urlValue->setObjectName(QStringLiteral("valueLabel"));
    m_urlValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_copyUrlBtn = new QPushButton(tr("Copiar"));
    m_copyUrlBtn->setObjectName(QStringLiteral("copyBtn"));
    m_copyUrlBtn->setCursor(Qt::PointingHandCursor);
    connect(m_copyUrlBtn, &QPushButton::clicked, this, &MainWindow::onCopyUrl);
    m_openUrlBtn = new QPushButton(tr("Abrir"));
    m_openUrlBtn->setObjectName(QStringLiteral("copyBtn"));
    m_openUrlBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openUrlBtn, &QPushButton::clicked, this, &MainWindow::onOpenUrl);
    urlRow->addWidget(urlLabel);
    urlRow->addStretch();
    urlRow->addWidget(m_urlValue);
    urlRow->addWidget(m_copyUrlBtn);
    urlRow->addWidget(m_openUrlBtn);
    c2->addLayout(urlRow);

    auto *portRow = new QHBoxLayout();
    portRow->setSpacing(8);
    auto *portLabel = new QLabel(tr("Puerto"));
    portLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1025, 65535);
    m_portSpin->setValue(8080);
    m_portSpin->setFixedWidth(90);
    portRow->addWidget(portLabel);
    portRow->addWidget(m_portSpin);
    portRow->addStretch();
    c2->addLayout(portRow);

    auto *folderRow = new QHBoxLayout();
    folderRow->setSpacing(8);
    auto *folderLabel = new QLabel(tr("Carpeta"));
    folderLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_folderLabel = new QLabel();
    m_folderLabel->setObjectName(QStringLiteral("hintLabel"));
    m_folderLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_pickFolderBtn = new QPushButton(tr("Examinar…"));
    m_pickFolderBtn->setObjectName(QStringLiteral("copyBtn"));
    m_pickFolderBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pickFolderBtn, &QPushButton::clicked, this, &MainWindow::onPickFolder);
    folderRow->addWidget(folderLabel);
    folderRow->addWidget(m_folderLabel, 1);
    folderRow->addWidget(m_pickFolderBtn);
    c2->addLayout(folderRow);

    auto *serverRow = new QHBoxLayout();
    serverRow->setSpacing(8);
    m_serverDot = new QLabel();
    m_serverDot->setObjectName(QStringLiteral("statusDot"));
    m_serverDot->setFixedSize(12, 12);
    m_serverDot->setProperty("state", QStringLiteral("off"));
    m_serverStatusLabel = new QLabel(tr("Servidor de archivos: apagado"));
    m_serverStatusLabel->setObjectName(QStringLiteral("statusText"));
    m_toggleServerBtn = new QPushButton(tr("Activar"));
    m_toggleServerBtn->setObjectName(QStringLiteral("serverBtn"));
    m_toggleServerBtn->setCursor(Qt::PointingHandCursor);
    m_toggleServerBtn->setProperty("active", false);
    connect(m_toggleServerBtn, &QPushButton::clicked, this, &MainWindow::onToggleServer);
    serverRow->addWidget(m_serverDot);
    serverRow->addWidget(m_serverStatusLabel);
    serverRow->addStretch();
    serverRow->addWidget(m_toggleServerBtn);
    c2->addLayout(serverRow);

    auto *hint = new QLabel(tr("En el teléfono: ES File Explorer → HTTP → entra en la dirección de arriba."));
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setWordWrap(true);
    c2->addWidget(hint);
    root->addWidget(card2);

    // Card 3 — Registro
    auto *card3 = new QFrame();
    card3->setObjectName(QStringLiteral("card"));
    auto *c3 = new QVBoxLayout(card3);
    c3->setContentsMargins(14, 12, 14, 12);
    c3->setSpacing(6);
    auto *section3 = new QLabel(tr("REGISTRO"));
    section3->setObjectName(QStringLiteral("sectionTitle"));
    c3->addWidget(section3);
    m_logView = new QPlainTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(500);
    m_logView->setFixedHeight(88);
    c3->addWidget(m_logView);
    root->addWidget(card3);

    // Footer
    auto *footer = new QHBoxLayout();
    auto *rdpButton = new QPushButton(tr("Escritorio remoto (RDP)"));
    rdpButton->setObjectName(QStringLiteral("ghostBtn"));
    rdpButton->setCursor(Qt::PointingHandCursor);
    connect(rdpButton, &QPushButton::clicked, this, &MainWindow::onLaunchRdp);
    auto *version = new QLabel(tr("v1.0.0"));
    version->setObjectName(QStringLiteral("hintLabel"));
    footer->addWidget(rdpButton);
    footer->addStretch();
    footer->addWidget(version);
    root->addLayout(footer);
}

void MainWindow::applyStyle()
{
    QFile styleFile(QStringLiteral(":/styles/styles.qss"));
    if (styleFile.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
}

void MainWindow::loadSettings()
{
    QSettings settings;
    m_ssidEdit->setText(settings.value(QStringLiteral("hotspot/ssid")).toString());
    m_passEdit->setText(settings.value(QStringLiteral("hotspot/pass")).toString());
    m_portSpin->setValue(settings.value(QStringLiteral("server/port"), 8080).toInt());
    m_folder = settings.value(QStringLiteral("server/folder"),
                              QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                   .toString();
    if (m_folder.isEmpty())
        m_folder = QDir::homePath();

    const QString elided = fontMetrics().elidedText(m_folder, Qt::ElideMiddle, 240);
    m_folderLabel->setText(elided);
    m_folderLabel->setToolTip(m_folder);
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("hotspot/ssid"), m_ssidEdit->text().trimmed());
    settings.setValue(QStringLiteral("hotspot/pass"), m_passEdit->text());
    settings.setValue(QStringLiteral("server/port"), m_portSpin->value());
    settings.setValue(QStringLiteral("server/folder"), m_folder);
}

QString MainWindow::serverUrl() const
{
    return QStringLiteral("http://%1:%2").arg(m_ip).arg(m_portSpin->value());
}

void MainWindow::updateUrl()
{
    const QString url = serverUrl();
    m_urlValue->setText(url);
    m_urlValue->setToolTip(url);
}

void MainWindow::setDot(QLabel *dot, const QString &state)
{
    dot->setProperty("state", state);
    repolish(dot);
}

void MainWindow::repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void MainWindow::updateHotspotUi()
{
    using S = HotspotController::State;
    const S state = m_controller.state();

    QString dotState;
    QString statusText;
    switch (state) {
    case S::Off:
        dotState = QStringLiteral("off");
        statusText = tr("Apagado");
        break;
    case S::Starting:
        dotState = QStringLiteral("starting");
        statusText = tr("Iniciando…");
        break;
    case S::On:
        dotState = QStringLiteral("on");
        statusText = tr("Activo");
        break;
    case S::Stopping:
        dotState = QStringLiteral("stopping");
        statusText = tr("Deteniendo…");
        break;
    case S::Error:
        dotState = QStringLiteral("error");
        statusText = tr("Error");
        break;
    }

    setDot(m_statusDot, dotState);
    m_statusText->setText(statusText);

    const bool active = state == S::On || state == S::Starting || state == S::Stopping;
    m_toggleHotspotBtn->setText(active ? tr("Detener Hotspot") : tr("Iniciar Hotspot"));
    m_toggleHotspotBtn->setObjectName(active ? QStringLiteral("dangerBtn") : QStringLiteral("primaryBtn"));
    repolish(m_toggleHotspotBtn);

    m_ssidEdit->setEnabled(!active);
    m_passEdit->setEnabled(!active);
    m_showPassBtn->setEnabled(!active);
}

void MainWindow::updateServerUi()
{
    const bool running = m_server.isRunning();
    setDot(m_serverDot, running ? QStringLiteral("on") : QStringLiteral("off"));
    m_serverStatusLabel->setText(running
        ? tr("Servidor de archivos: activo") + QStringLiteral(" · ") + serverUrl()
        : tr("Servidor de archivos: apagado"));
    m_toggleServerBtn->setText(running ? tr("Desactivar") : tr("Activar"));
    m_toggleServerBtn->setProperty("active", running);
    repolish(m_toggleServerBtn);
}

void MainWindow::onToggleHotspot()
{
    if (!isElevated()) {
        log(tr("Permisos insuficientes: reinicia como administrador desde el aviso superior."));
        return;
    }

    using S = HotspotController::State;
    const S state = m_controller.state();
    if (state == S::On || state == S::Starting || state == S::Stopping) {
        m_controller.stop();
        return;
    }

    const QString ssid = m_ssidEdit->text().trimmed();
    const QString pass = m_passEdit->text();

    if (ssid.isEmpty()) {
        log(tr("El nombre de la red no puede estar vacío."));
        m_ssidEdit->setFocus();
        return;
    }
    if (ssid.size() > 32) {
        log(tr("El nombre de la red debe tener 32 caracteres o menos."));
        return;
    }
    if (pass.size() < 8 || pass.size() > 63) {
        log(tr("La contraseña debe tener entre 8 y 63 caracteres."));
        m_passEdit->setFocus();
        return;
    }

    saveSettings();
    m_controller.start(ssid, pass);
}

void MainWindow::onHotspotStateChanged(HotspotController::State state, const QString &detail)
{
    updateHotspotUi();
    if (!detail.isEmpty())
        log(detail);

    if (state == HotspotController::State::On) {
        m_pollTick = 0;
        m_statusTimer.start();
        refreshIp();
        if (!m_server.isRunning())
            startServer();
    } else if (state == HotspotController::State::Off
               || state == HotspotController::State::Error) {
        m_statusTimer.stop();
        if (m_server.isRunning()) {
            m_server.stop();
            updateServerUi();
        }
    }
}

void MainWindow::onStatusRefresh()
{
    if (m_controller.state() != HotspotController::State::On)
        return;
    if (m_controller.isBusy())
        return;
    if (m_pollTick++ % 2 == 0)
        m_controller.refreshStatus();
    else
        m_controller.detectIp();
}

void MainWindow::refreshIp()
{
    m_controller.detectIp();
}

void MainWindow::onIpDetected(const QString &ip)
{
    if (ip.isEmpty() || ip == m_ip)
        return;
    m_ip = ip;
    m_ipValue->setText(ip);
    updateUrl();
    updateServerUi();
    log(tr("IP del equipo: %1").arg(ip));
}

bool MainWindow::startServer()
{
    m_server.setRoot(m_folder);
    const bool ok = m_server.start(static_cast<quint16>(m_portSpin->value()));
    updateServerUi();
    return ok;
}

void MainWindow::onToggleServer()
{
    if (m_server.isRunning()) {
        m_server.stop();
        updateServerUi();
        return;
    }
    startServer();
}

void MainWindow::onPickFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Carpeta a compartir"), m_folder);
    if (folder.isEmpty())
        return;
    m_folder = folder;
    const QString elided = fontMetrics().elidedText(m_folder, Qt::ElideMiddle, 240);
    m_folderLabel->setText(elided);
    m_folderLabel->setToolTip(m_folder);
    saveSettings();
    if (m_server.isRunning()) {
        m_server.stop();
        startServer();
    }
}

void MainWindow::onCopyIp()
{
    QGuiApplication::clipboard()->setText(m_ip);
    log(tr("IP copiada: %1").arg(m_ip));
}

void MainWindow::onCopyUrl()
{
    const QString url = serverUrl();
    QGuiApplication::clipboard()->setText(url);
    log(tr("Dirección copiada: %1").arg(url));
}

void MainWindow::onOpenUrl()
{
    QDesktopServices::openUrl(QUrl(serverUrl()));
}

void MainWindow::onLaunchRdp()
{
#ifdef Q_OS_WIN
    if (!QProcess::startDetached(QStringLiteral("mstsc"), { QStringLiteral("/v:") + m_ip }))
        log(tr("No se pudo abrir el Escritorio remoto."));
    else
        log(tr("Abriendo Escritorio remoto hacia %1…").arg(m_ip));
#else
    log(tr("Escritorio remoto solo está disponible en Windows."));
#endif
}

void MainWindow::log(const QString &message)
{
    m_logView->appendPlainText(QStringLiteral("[%1] %2")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), message));
}
