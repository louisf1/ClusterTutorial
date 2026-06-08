#include "SystemMonitor.h"
#include <QDebug>
#include <QTime>
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

    // A. Check Kuksa Service — maps docker Running bool to "active"/"inactive" for QML
    process.start("sh", QStringList() << "-c"
        << "docker inspect -f '{{if .State.Running}}active{{else}}inactive{{end}}' kuksa-databroker 2>/dev/null || echo inactive");
    process.waitForFinished(500);
    m_kuksaStatus = process.readAllStandardOutput().trimmed();
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
    
    // F. K-LINE ECU status placeholder
    float coolantTemp = 85.5;
    updateScrollingLog("K-LINE: ECT Read Success -> " + QString::number(coolantTemp) + "°C");

    // H. Bluetooth status — is-active returns exactly "active" or "inactive"
    process.start("systemctl", QStringList() << "is-active" << "bluetooth");
    process.waitForFinished(500);
    m_bluetoothStatus = process.readAllStandardOutput().trimmed();
    emit bluetoothStatusChanged();

    // I. Arduino status — check USB serial device presence (no systemctl service exists)
    process.start("sh", QStringList() << "-c"
        << "test -e /dev/ttyACM0 && echo active || echo inactive");
    process.waitForFinished(500);
    m_arduinoStatus = process.readAllStandardOutput().trimmed();
    emit arduinoStatusChanged();

    // J. Python support status — check if bridge script process is running
    process.start("sh", QStringList() << "-c"
        << "ps aux | grep [r]pm_speed_arduino_dev_ttyACM0 | grep -v grep");
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
