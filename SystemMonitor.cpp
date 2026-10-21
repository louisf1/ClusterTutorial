#include "SystemMonitor.h"
#include <QDebug>
#include <QTime>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

void SystemMonitor::updateScrollingLog(const QString &message) {
    // We append a timestamp here in C++ so the log looks professional
    QString timestamp = QTime::currentTime().toString("hh:mm:ss.zzz");
    QString formattedMessage = QString("[%1] %2").arg(timestamp, message);
    
    emit newLogEntry(formattedMessage); 
} // end updateScrollingLog


void SystemMonitor::parseKLineData(QByteArray hexResponse) {
    // Example Response for RPM (PID 0C): 83 F1 10 41 0C 1A F8 [Checksum]
    // The data is 1A F8 (hex). Formula: ((A * 256) + B) / 4
    
    if (hexResponse.size() > 5 && hexResponse[3] == 0x41) {
        uint8_t pid = hexResponse[4];
        if (pid == 0x0C) { // RPM
            int rpm = ((unsigned char)hexResponse[5] * 256 + (unsigned char)hexResponse[6]) / 4;
            updateScrollingLog("ECU RPM: " + QString::number(rpm));
        }
    }
}

// Constructor: Initializes the timer AND the ECU wire list
SystemMonitor::SystemMonitor(QObject *parent) : QObject(parent) {
    // Setup the timer for periodic Linux stats updates
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SystemMonitor::updateStats);
    m_timer->start(1000);

    // Fast timer — page-switch button, polled at 100ms for low-latency response
    m_fastTimer = new QTimer(this);
    connect(m_fastTimer, &QTimer::timeout, this, &SystemMonitor::updateFastStats);
    m_fastTimer->start(100);

    // Initialize all 20 ECU wires to inactive / 0 V
    m_ecuWireStates.clear();
    m_ecuWireVoltages.clear();
    for (int i = 0; i < 20; ++i) {
        m_ecuWireStates.append(false);
        m_ecuWireVoltages.append(0.0f);
    }
} // end SystemMonitor constructor

// Method to update specific wire states (e.g., from Serial data)
void SystemMonitor::updateEcuWire(int index, bool active) {
    if (index >= 0 && index < m_ecuWireStates.size()) {
        if (m_ecuWireStates[index].toBool() != active) {
            m_ecuWireStates[index] = active;
            emit ecuWireStatesChanged(); // Redraws the ECU list in QML
        } // end state-check if
    } // end index-check if
} // end updateEcuWire method

void SystemMonitor::updateEcuVoltage(int index, float voltage) {
    if (index >= 0 && index < m_ecuWireVoltages.size()) {
        if (m_ecuWireVoltages[index].toFloat() != voltage) {
            m_ecuWireVoltages[index] = voltage;
            emit ecuWireVoltagesChanged();
        }
    }
} // end updateEcuVoltage method

void SystemMonitor::updateStats() {
    QProcess process;

    // A. Check Kuksa Service — maps docker Running bool to "active"/"inactive" for QML.
    //    "unknown" when docker itself fails (daemon down, no access to
    //    docker.sock, timeout) so a broken check doesn't look like a stopped broker.
    process.start("docker", QStringList() << "inspect" << "-f" << "{{.State.Running}}" << "kuksa-databroker");
    QString kuksaStatus;
    QString kuksaError;
    if (!process.waitForFinished(500)) {
        kuksaError = (process.error() == QProcess::FailedToStart) ? "docker not found" : "docker inspect timed out";
        process.kill();
        process.waitForFinished(100);
    } else if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        kuksaStatus = (process.readAllStandardOutput().trimmed() == "true") ? "active" : "inactive";
    } else {
        kuksaError = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        // Container doesn't exist — docker answered, the broker is definitely not running
        if (kuksaError.contains("No such object") || kuksaError.contains("No such container"))
            kuksaStatus = "inactive";
    } // end docker inspect if
    if (kuksaStatus.isEmpty()) {
        if (m_kuksaStatus != "unknown") qDebug() << "[KUKSA] Status check failed:" << kuksaError;
        kuksaStatus = "unknown";
    } // end unknown if
    m_kuksaStatus = kuksaStatus;
    emit kuksaStatusChanged();

    // B. Get Top CPU Processes
    process.start("sh", QStringList() << "-c" << "ps -eo pcpu,comm --sort=-pcpu | head -n 15");
    process.waitForFinished(500);
    m_topProcesses = process.readAllStandardOutput();
    emit topProcessesChanged();

    // C. Get CPU Temperature
    process.start("cat", QStringList() << "/sys/class/thermal/thermal_zone0/temp");
    if (process.waitForFinished(500)) {
        QString tempRaw = process.readAllStandardOutput().trimmed();
        m_cpuTemp = tempRaw.toFloat() / 1000.0;
        emit cpuTempChanged();
    } // end temp if

    // D. Get Overall CPU Usage %
    process.start("sh", QStringList() << "-c" << "top -bn1 | grep \"Cpu(s)\" | sed \"s/.*, *\\([0-9.]*\\)%* id.*/\\1/\" | awk '{print 100 - $1}'");
    if (process.waitForFinished(500)) {
        m_cpuPercent = process.readAllStandardOutput().trimmed().toFloat();
        emit cpuPercentChanged();
    } // end cpu if

    // E. Get RAM Usage %
    process.start("sh", QStringList() << "-c" << "free | grep Mem | awk '{print $3/$2 * 100.0}'");
    if (process.waitForFinished(500)) {
        m_ramPercent = process.readAllStandardOutput().trimmed().toFloat();
        emit ramPercentChanged();
    } // end ram if
    
    // F. Arduino telemetry → K-Line scrolling log (noise-filtered)
    // Reads /tmp/arduino_telemetry.json written by the Python bridge on every serial cycle.
    // Rule: emit immediately if a value changed; otherwise repeat every 30 seconds.
    QFile tf("/tmp/arduino_telemetry.json");
    if (tf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument tdoc = QJsonDocument::fromJson(tf.readAll());
        tf.close();
        if (!tdoc.isNull() && tdoc.isObject()) {
            QJsonObject cur = tdoc.object();
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 REPEAT_MS = 30000;

            auto maybeLog = [&](const QString &key, const QString &msg) {
                bool changed = (m_lastTelemetry.value(key) != cur.value(key));
                if (changed || (now - m_lastLogTime.value(key, 0)) >= REPEAT_MS) {
                    updateScrollingLog(msg);
                    m_lastLogTime[key]    = now;
                    m_lastTelemetry[key]  = cur.value(key);
                }
            };

            maybeLog("speed",        QString("SPD: %1 km/h")           .arg(cur["speed"].toDouble(),  0, 'f', 0));
            maybeLog("rpm",          QString("RPM: %1")                 .arg(cur["rpm"].toDouble(),    0, 'f', 0));
            maybeLog("temp",         QString("TEMP: %1 °C")             .arg(cur["temp"].toDouble(),   0, 'f', 1));
            maybeLog("liters",       QString("FUEL: %1 L")              .arg(cur["liters"].toDouble(), 0, 'f', 1));
            maybeLog("range",        QString("RANGE: %1 km")            .arg(cur["range"].toDouble(),  0, 'f', 0));
            maybeLog("glowPlug",     QString("GLOW PLUG: %1")           .arg(cur["glowPlug"].toBool()     ? "ON"      : "OFF"));
            maybeLog("ac",           QString("A/C: %1")                 .arg(cur["ac"].toBool()            ? "ON"      : "OFF"));
            maybeLog("lightsDim",    QString("LIGHTS DIM: %1")          .arg(cur["lightsDim"].toBool()     ? "ON"      : "OFF"));
            maybeLog("lightsBright", QString("LIGHTS BRIGHT: %1")       .arg(cur["lightsBright"].toBool()  ? "ON"      : "OFF"));
            maybeLog("daytime",      QString("DAYTIME RUNNING: %1")     .arg(cur["daytime"].toBool()       ? "ON"      : "OFF"));
            maybeLog("parking",      QString("PARKING LIGHTS: %1")      .arg(cur["parking"].toBool()       ? "ON"      : "OFF"));
            maybeLog("parkingBrake", QString("PARK BRAKE: %1")          .arg(cur["parkingBrake"].toBool()  ? "ON"      : "OFF"));
            maybeLog("seatbelt",     QString("SEATBELT: %1")            .arg(cur["seatbelt"].toBool()      ? "BUCKLED" : "OPEN"));
            maybeLog("oilNormal",    QString("OIL LEVEL: %1")           .arg(cur["oilNormal"].toBool()     ? "NORMAL"  : "LOW"));
            maybeLog("doorOpen",     QString("DOOR: %1")                .arg(cur["doorOpen"].toBool()      ? "OPEN"    : "CLOSED"));
            maybeLog("ecuError",     QString("ECU ERROR: %1")           .arg(cur["ecuError"].toBool()      ? "YES"     : "NONE"));
        }
    }

    // H. Bluetooth status — is-active returns exactly "active" or "inactive"
    process.start("systemctl", QStringList() << "is-active" << "bluetooth");
    process.waitForFinished(500);
    m_bluetoothStatus = process.readAllStandardOutput().trimmed();
    emit bluetoothStatusChanged();

    // I. Arduino status — check ttyACM0 first, fall back to ttyACM1
    process.start("sh", QStringList() << "-c" << "test -e /dev/ttyACM0 && echo ttyACM0 || (test -e /dev/ttyACM1 && echo ttyACM1 || echo inactive)");
    process.waitForFinished(500);
    {
        QString arduinoPort = process.readAllStandardOutput().trimmed();
        if (arduinoPort == "inactive") {
            if (m_arduinoStatus != "inactive") qDebug() << "[Arduino] No device on ttyACM0 or ttyACM1";
            m_arduinoStatus = "inactive";
        } else {
            if (m_arduinoStatus != "active") qDebug() << "[Arduino] Connected on" << arduinoPort;
            m_arduinoStatus = "active";
        }
    }
    emit arduinoStatusChanged();

    // J. Python support status — check if bridge script process is running
    process.start("sh", QStringList() << "-c"
        << "ps aux | grep [r]pm_speed_arduino_dev_ttyACM | grep -v grep");
    process.waitForFinished(500);
    m_pythonSupportStatus = process.readAllStandardOutput().trimmed().isEmpty() ? "inactive" : "active";
    emit pythonSupportStatusChanged();

    // J. ECU wire states + voltages — written by Python bridge from Arduino AV1-AV13
    QFile jf("/tmp/ecu_wire_states.json");
    if (jf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(jf.readAll());
        jf.close();
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj      = doc.object();
            QJsonArray  states   = obj["states"].toArray();
            QJsonArray  voltages = obj["voltages"].toArray();

            for (int i = 0; i < 20 && i < states.size(); ++i)
                updateEcuWire(i, states[i].toBool());

            bool vChanged = false;
            for (int i = 0; i < 20 && i < voltages.size(); ++i) {
                float v = static_cast<float>(voltages[i].toDouble());
                if (m_ecuWireVoltages[i].toFloat() != v) {
                    m_ecuWireVoltages[i] = v;
                    vChanged = true;
                }
            }
            if (vChanged) emit ecuWireVoltagesChanged();
        }
    }
} // end updateStats slot

// Dashboard page-switch button (D9) — polled at 100ms from the FAST report,
// fires once on rising edge
void SystemMonitor::updateFastStats() {
    QFile jf("/tmp/ecu_fast_state.json");
    if (jf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(jf.readAll());
        jf.close();
        if (!doc.isNull() && doc.isObject()) {
            bool pageButton = doc.object()["pageButton"].toBool();
            if (pageButton && !m_lastPageButton) {
                emit pageButtonPressed();
            }
            m_lastPageButton = pageButton;

            bool tripReset = doc.object()["tripReset"].toBool();
            if (tripReset && !m_lastTripReset) {
                emit tripResetTriggered();
            }
            m_lastTripReset = tripReset;
        }
    }
} // end updateFastStats slot
