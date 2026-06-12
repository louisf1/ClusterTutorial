#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QTimer>
#include <QProcess>
#include <QVariantList>
#include <QString>

class SystemMonitor : public QObject {
    Q_OBJECT

    // 1. Properties: Definitive list for QML access
    Q_PROPERTY(QString kuksaStatus READ kuksaStatus NOTIFY kuksaStatusChanged)
    Q_PROPERTY(QString bluetoothStatus READ bluetoothStatus NOTIFY bluetoothStatusChanged)
    Q_PROPERTY(QString arduinoStatus READ arduinoStatus NOTIFY arduinoStatusChanged)
    Q_PROPERTY(QString pythonSupportStatus READ pythonSupportStatus NOTIFY pythonSupportStatusChanged)
           
    Q_PROPERTY(QString topProcesses READ topProcesses NOTIFY topProcessesChanged)
    Q_PROPERTY(float cpuPercent READ cpuPercent NOTIFY cpuPercentChanged)
    Q_PROPERTY(float ramPercent READ ramPercent NOTIFY ramPercentChanged)
    Q_PROPERTY(float cpuTemp READ cpuTemp NOTIFY cpuTempChanged)
    Q_PROPERTY(QVariantList ecuWireStates    READ ecuWireStates    NOTIFY ecuWireStatesChanged)
    Q_PROPERTY(QVariantList ecuWireVoltages READ ecuWireVoltages NOTIFY ecuWireVoltagesChanged)

public:
    // Constructor (Declared ONLY once)
    explicit SystemMonitor(QObject *parent = nullptr);

    // 2. Getters: These return the private data to QML (Declared ONLY once)
    QString kuksaStatus() const { return m_kuksaStatus; }
    QString bluetoothStatus() const { return m_bluetoothStatus; }
    QString arduinoStatus() const { return m_arduinoStatus; }
    QString pythonSupportStatus() const { return m_pythonSupportStatus; }
    
    QString topProcesses() const { return m_topProcesses; }
    float cpuPercent() const { return m_cpuPercent; }
    float ramPercent() const { return m_ramPercent; }
    float cpuTemp() const { return m_cpuTemp; }
    QVariantList ecuWireStates()    const { return m_ecuWireStates; }
    QVariantList ecuWireVoltages()  const { return m_ecuWireVoltages; }

    // 3. Logic Methods
    void updateEcuWire(int index, bool active);
    void updateEcuVoltage(int index, float voltage);
    void parseKLineData(QByteArray hexResponse);

public slots:
    void updateStats(); // Called by the 1s timer
    void updateFastStats(); // Called by the 100ms timer — page button
    void updateScrollingLog(const QString &message);

signals:
    // 4. Notification Signals: Triggered when data changes
    void kuksaStatusChanged();
    void bluetoothStatusChanged();
    void arduinoStatusChanged();    
    void pythonSupportStatusChanged();    
  
    void topProcessesChanged();
    void cpuPercentChanged();
    void ramPercentChanged();
    void cpuTempChanged();
    void ecuWireStatesChanged();
    void ecuWireVoltagesChanged();
    void pageButtonPressed(); // Fired once per physical D9 button press
    void newLogEntry(QString message); // This "carries" the K-Line string to QML

private:
    // 5. Member Variables: Actual data storage (No duplicates!)
    QString m_kuksaStatus;
    QString m_topProcesses;
    QString m_bluetoothStatus;
    QString m_arduinoStatus;
    QString m_pythonSupportStatus;
    
    float m_cpuPercent = 0.0;
    float m_ramPercent = 0.0;
    float m_cpuTemp = 0.0;

    QTimer *m_timer = nullptr;
    QTimer *m_fastTimer = nullptr;
    QVariantList m_ecuWireStates;    // 20 booleans  — wire active/inactive
    QVariantList m_ecuWireVoltages;  // 20 floats (V) — real voltage after ×3 scale
    bool m_lastPageButton = false;   // edge-detect for the D9 page-switch button
}; // end SystemMonitor class

#endif // end SYSTEMMONITOR_H
