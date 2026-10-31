#include <QVariant>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QtQml/QQmlComponent>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QDir>
#include <QScreen>
#include <QWindow>
#include <QDirIterator>
#include <QStandardPaths>
#include <QQmlEngine>
// [FIX] Removed duplicate #include <QQuickView> (was included twice)
// [FIX] Removed #include <QQuickView> entirely — QQuickView is never used in this file;
//       QQmlApplicationEngine and QQmlComponent are used instead.
#include <fstream>
#include <string>
#include <vector>
#include <kuksaclient/kuksaclient.h>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>
#include "qwayland-agl-shell.h"
#include "SystemMonitor.h"

using namespace kuksa;

extern "C" {
    #include "wayland-agl-shell-client-protocol.h"
    extern const struct wl_interface agl_shell_interface;
    extern const struct wl_interface agl_shell_ext_interface;
}

// =============================================================================
// Wayland helpers
// =============================================================================

static void
global_remove(void *data, struct wl_registry *reg, uint32_t id)
{
    (void) data;
    (void) reg;
    (void) id;
}

static void
global_add(void *data, struct wl_registry *reg, uint32_t name,
           const char *interface, uint32_t version)
{
    struct agl_shell **shell = static_cast<struct agl_shell **>(data);
    if (strcmp(interface, agl_shell_interface.name) == 0) {
        *shell = static_cast<struct agl_shell *>(
            wl_registry_bind(reg, name, &agl_shell_interface, 1)
        );
    }
}

static const struct wl_registry_listener registry_listener = {
    global_add,
    global_remove
};

// Global flag for canned animation
bool runAnimation = true;

static struct wl_surface *
getWlSurface(QPlatformNativeInterface *native, QWindow *window)
{
    void *surf = native->nativeResourceForWindow("surface", window);
    return static_cast<struct wl_surface *>(surf);
}

static struct wl_output *
getWlOutput(QPlatformNativeInterface *native, QScreen *screen)
{
    void *output = native->nativeResourceForScreen("output", screen);
    return static_cast<struct wl_output *>(output);
}

static struct wl_surface *
create_component(QPlatformNativeInterface *native, QQmlComponent *comp,
                 QScreen *screen, QObject **qobj)
{
    QObject *obj = comp->create();
    if (!obj) {
        qCritical() << "Failed to create QML component:" << comp->errors();
        return nullptr;
    }

    QWindow *win = qobject_cast<QWindow *>(obj);
    if (!win) {
        qWarning() << "Component is not a QWindow.";
    }

    *qobj = obj;
    return getWlSurface(native, win);
}

static struct agl_shell *
register_agl_shell(QPlatformNativeInterface *native)
{
    struct agl_shell *shell = nullptr;

    qDebug() << "TRY: native->nativeResourceForIntegration";

    if (qGuiApp->platformName() != "wayland") {
        qDebug() << "PLATFORM CHECK: Currently on" << qGuiApp->platformName()
                 << "- Skipping Wayland registry calls.";
        return nullptr;
    }

    void *display = native->nativeResourceForIntegration("display");
    qDebug() << "FINISH: native->nativeResourceForIntegration";

    if (!display) {
        qDebug() << "Wayland display not found. Returning null.";
        return nullptr;
    }

    qDebug() << "TRY: wl_display_get_registry";
    struct wl_display *wl = static_cast<struct wl_display *>(display);
    struct wl_registry *registry = wl_display_get_registry(wl);
    qDebug() << "COMPLETED: wl_display_get_registry";

    wl_registry_add_listener(registry, &registry_listener, &shell);
    wl_display_roundtrip(wl);
    wl_registry_destroy(registry);

    return shell;
}

// =============================================================================
// KuksaHandler
// =============================================================================

class KuksaHandler : public QObject {
    Q_OBJECT

private:
    struct VssEntry {
        std::string path;
        std::string type;
        std::string hmiProperty;
        std::string initValue;  // 4th pipe field — persisted/default value
    };

    // -------------------------------------------------------------------------
    // Helpers — type-safe conversion between string, QVariant, and kuksa::Value
    // -------------------------------------------------------------------------

    static kuksa::val::v2::Value makeKuksaValue(const std::string &type,
                                                 const std::string &str)
    {
        kuksa::val::v2::Value v;
        try {
            if      (type == "float")                              v.set_float_(std::stof(str));
            else if (type == "double")                             v.set_double_(std::stod(str));
            else if (type == "bool" || type == "boolean")          v.set_bool_(str == "true" || str == "1");
            else if (type == "int8"  || type == "int16"  || type == "int32")
                                                                   v.set_int32(std::stoi(str));
            else if (type == "uint8" || type == "uint16" || type == "uint32")
                                                                   v.set_uint32(static_cast<uint32_t>(std::stoul(str)));
            else if (type == "int64")                              v.set_int64(std::stoll(str));
            else if (type == "uint64")                             v.set_uint64(std::stoull(str));
            else                                                   v.set_string(str);
        } catch (...) { v.set_string(str); }
        return v;
    }

    static QVariant makeQVariant(const std::string &type, const std::string &str) {
        try {
            if      (type == "float")                              return QVariant(std::stof(str));
            if      (type == "double")                             return QVariant(std::stod(str));
            if      (type == "bool" || type == "boolean")          return QVariant(str == "true" || str == "1");
            if      (type == "int8"  || type == "int16"  || type == "int32")
                                                                   return QVariant(std::stoi(str));
            if      (type == "uint8" || type == "uint16" || type == "uint32")
                                                                   return QVariant(static_cast<unsigned int>(std::stoul(str)));
            if      (type == "int64")                              return QVariant(static_cast<qint64>(std::stoll(str)));
            if      (type == "uint64")                             return QVariant(static_cast<quint64>(std::stoull(str)));
        } catch (...) {}
        return QVariant(QString::fromStdString(str));
    }

    static QString formatForFile(const std::string &type, const QVariant &val) {
        if (type == "float" || type == "double")
            return QString::number(val.toDouble(), 'f', 1);
        if (type == "bool" || type == "boolean")
            return val.toBool() ? "true" : "false";
        if (type == "string")
            return val.toString();
        return QString::number(val.toLongLong());
    }

public:
    KuksaHandler(QObject *valuesObj, QObject *parent = nullptr)
        : QObject(parent), m_values(valuesObj)
    {
        m_client = std::make_shared<KuksaClient>();

        bool connectionStatus = m_client->connect_v2("0.0.0.0:55555");
        if (connectionStatus) {
            qDebug() << "Kuksa V2 Connection is Successful!";
        } else {
            qDebug() << "CRITICAL: Kuksa V2 Connection Failed!";
        }
    }

void loadConfig(const QString &filePath) {
    m_filePath = filePath;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "ERROR: Could not open VSS config file:" << filePath;
        return;
    }

    std::vector<VssEntry> entries;
    std::vector<std::string> paths;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#") || line.startsWith("["))
            continue;

        QStringList parts = line.split('|');
        if (parts.size() >= 3) {
            VssEntry e;
            e.path        = parts[0].toStdString();
            e.type        = parts[1].toStdString();
            e.hmiProperty = parts[2].trimmed().toStdString();
            if (parts.size() >= 4)
                e.initValue = parts[3].trimmed().toStdString();

            // When hmiProperty is NO_HMI_Property but an initValue exists,
            // derive the property name from the last VSS path segment (camelCase).
            // E.g. "Vehicle.VehicleIdentification.VehicleModelDate" → "vehicleModelDate"
            if (e.hmiProperty == "NO_HMI_Property" && !e.initValue.empty()) {
                std::string seg = e.path.substr(e.path.rfind('.') + 1);
                if (!seg.empty()) seg[0] = static_cast<char>(std::tolower(seg[0]));
                e.hmiProperty = seg;
            }

            entries.push_back(e);
            paths.push_back(e.path);
            qDebug() << "Queued:" << parts[0] << "|" << parts[1] << "|"
                     << QString::fromStdString(e.hmiProperty)
                     << (e.initValue.empty() ? "" :
                         " | init=" + QString::fromStdString(e.initValue));
        } else {
            qDebug() << "WARNING: Skipping malformed line:" << line;
        }
    }
    file.close();

    if (paths.empty()) {
        qDebug() << "ERROR: No valid VSS paths found in config.";
        return;
    }

    // KUKSA is volatile — publish saved/default values on every startup so the
    // databroker has valid data before a live provider connects.
    // Also pre-fill QML properties so the dashboard is populated immediately.
    for (const VssEntry &e : entries) {
        if (e.initValue.empty()) continue;

        kuksa::val::v2::Value kVal = makeKuksaValue(e.type, e.initValue);
        if (m_client->publishValue(e.path, kVal))
            qDebug() << "Init published:" << QString::fromStdString(e.path)
                     << "=" << QString::fromStdString(e.initValue);
        else
            qDebug() << "Init publish FAILED:" << QString::fromStdString(e.path);

        if (!e.hmiProperty.empty() && e.hmiProperty != "NO_HMI_Property" && m_values) {
            QVariant qv = makeQVariant(e.type, e.initValue);
            if (qv.isValid()) applyToHmi(e.hmiProperty, e.type, qv);
        }

        // Seed the odometer/trip accumulators from the persisted init values
        // so counting resumes from where it left off after a restart.
        if (e.hmiProperty == "totalKms") {
            m_totalKmsAccum = QString::fromStdString(e.initValue).toDouble();
            m_lastTotalKmsStr = QString::number(
                static_cast<long long>(std::round(m_totalKmsAccum)));
        } else if (e.hmiProperty == "tripKms") {
            m_tripKmsAccum = QString::fromStdString(e.initValue).toDouble();
            m_lastTripKmsStr = QString::number(m_tripKmsAccum, 'f', 1);
        }
    }

    // Single batch subscribe for all paths
    qDebug() << "Subscribing to" << paths.size() << "paths in one call...";

    m_client->subscribe(paths,
        [this, entries, filePath]
        (const std::string &path, const kuksa::val::v2::Value &value)
    {
        qDebug() << ">>> KUKSA DATA RECEIVED:" << QString::fromStdString(path);

        if (!m_values) return;

        auto it = std::find_if(entries.begin(), entries.end(),
            [&path](const VssEntry &e) { return e.path == path; });

        if (it == entries.end()) {
            qDebug() << "WARNING: Received update for unknown path:"
                     << QString::fromStdString(path);
            return;
        }

        const std::string &type         = it->type;
        const std::string &hmiProperty  = it->hmiProperty;
        const bool         shouldPersist = !it->initValue.empty();

        QVariant newValue;

        switch (value.typed_value_case()) {
            case kuksa::val::v2::Value::kFloat:
                newValue = value.float_();   break;
            case kuksa::val::v2::Value::kDouble:
                newValue = value.double_();  break;
            case kuksa::val::v2::Value::kBool:
                newValue = value.bool_();    break;
            case kuksa::val::v2::Value::kInt32:
                newValue = value.int32();    break;
            case kuksa::val::v2::Value::kUint32:
                newValue = value.uint32();   break;
            case kuksa::val::v2::Value::kInt64:
                newValue = static_cast<qint64>(value.int64());   break;
            case kuksa::val::v2::Value::kUint64:
                newValue = static_cast<quint64>(value.uint64()); break;
            case kuksa::val::v2::Value::kString:
                newValue = QString::fromStdString(value.string()); break;
            case kuksa::val::v2::Value::TYPED_VALUE_NOT_SET:
                qDebug() << "Signal uninitialized in Databroker. Defaulting to false.";
                newValue = false;
                break;
            default:
                qDebug() << "WARNING: Unsupported VSS data type ID:"
                         << value.typed_value_case();
                return;
        }

        if (!newValue.isValid()) return;

        QMetaObject::invokeMethod(m_values,
            [this, hmiProperty, newValue, type, shouldPersist, filePath]()
        {
            applyToHmi(hmiProperty, type, newValue);

            // Write the updated value back to VSS_paths.txt so it survives
            // KUKSA restarts (totalKms, tripKms, and any other persisted entry).
            if (shouldPersist)
                updateConfigFile(filePath, hmiProperty, formatForFile(type, newValue));
        });
    });

    // Start odometer/trip accumulation, ticking once per second based on
    // the current speedometer reading.
    if (!m_odoTimer) {
        m_odoTimer = new QTimer(this);
        connect(m_odoTimer, &QTimer::timeout, this, [this, filePath]() {
            updateOdometer(filePath);
        });
        m_odoTimer->start(1000);
    }
}

// Accumulates distance travelled (based on the current kph reading) into
// the total odometer and trip counters, updates the dashboard, publishes
// the new totals to KUKSA, and persists them to VSS_paths.txt whenever the
// displayed value changes.
void updateOdometer(const QString &filePath)
{
    if (!m_values) return;

    const double kph = m_values->property("kph").toDouble();
    if (kph <= 0.0) return;

    const double deltaKm = kph / 3600.0; // timer ticks every 1 second
    m_totalKmsAccum += deltaKm;
    m_tripKmsAccum  += deltaKm;

    const QString totalStr = QString::number(
        static_cast<long long>(std::round(m_totalKmsAccum)));
    if (totalStr != m_lastTotalKmsStr) {
        m_lastTotalKmsStr = totalStr;
        m_values->setProperty("totalKms", totalStr);

        kuksa::val::v2::Value kVal;
        kVal.set_float_(static_cast<float>(m_totalKmsAccum));
        m_client->publishValue("Vehicle.TraveledDistanceSinceStart", kVal);

        updateConfigFile(filePath, "totalKms",
            formatForFile("float", QVariant(m_totalKmsAccum)));
    }

    const QString tripStr = QString::number(m_tripKmsAccum, 'f', 1);
    if (tripStr != m_lastTripKmsStr) {
        m_lastTripKmsStr = tripStr;
        m_values->setProperty("tripKms", tripStr);

        kuksa::val::v2::Value kVal;
        kVal.set_float_(static_cast<float>(m_tripKmsAccum));
        m_client->publishValue("Vehicle.TripMeterReading", kVal);

        updateConfigFile(filePath, "tripKms",
            formatForFile("float", QVariant(m_tripKmsAccum)));
    }
}

// Resets the trip meter to zero — triggered by a D7 pulse (Arduino spare
// pin), forwarded via /tmp/ecu_fast_state.json and SystemMonitor.
void resetTripMeter()
{
    m_tripKmsAccum   = 0.0;
    m_lastTripKmsStr = "0.0";
    m_values->setProperty("tripKms", m_lastTripKmsStr);

    kuksa::val::v2::Value kVal;
    kVal.set_float_(0.0f);
    m_client->publishValue("Vehicle.TripMeterReading", kVal);

    updateConfigFile(m_filePath, "tripKms", formatForFile("float", QVariant(0.0)));
    qDebug() << "Trip meter reset to 0 (D7 signal)";
}

// Applies a live or init value to the correct QML property, including
// any derived display-string properties (displayKph, displayRpm, etc.).
void applyToHmi(const std::string &hmiProperty,
                const std::string &type,
                const QVariant    &val)
{
    if (hmiProperty.empty() || hmiProperty == "NO_HMI_Property") return;

    if (hmiProperty == "kph" && type == "float") {
        m_values->setProperty("kph", val);
        m_values->setProperty("displayKph",
            QString::number(static_cast<int>(val.toFloat())));
    } else if (hmiProperty == "rpm" && type == "float") {
        m_values->setProperty("rpm", val);
        m_values->setProperty("displayRpm",
            QString::number(static_cast<int>(val.toFloat())));
    } else if (hmiProperty == "liters" && type == "float") {
        float realLiters = val.toFloat();
        m_values->setProperty("liters", static_cast<int>(realLiters * 10));
        m_values->setProperty("displayLiters",
            QString::number(realLiters, 'f', 2));
    } else if (hmiProperty == "temp" && type == "float") {
        m_values->setProperty("temp", val);
        m_values->setProperty("displayTemp",
            QString::number(static_cast<int>(val.toFloat())));
    } else if (hmiProperty == "totalKms") {
        // Store as formatted integer string — e.g. "160234"
        m_values->setProperty("totalKms",
            QString::number(static_cast<long long>(std::round(val.toDouble()))));
    } else if (hmiProperty == "tripKms") {
        // Store with 1 decimal — e.g. "800.3"
        m_values->setProperty("tripKms",
            QString::number(val.toFloat(), 'f', 1));
    } else {
        QVariant toSet = val;
        // VSS paths typed "string" but bound to QML bool properties need explicit
        // conversion — Qt6's setProperty does not reliably coerce QString→bool.
        if (val.typeId() == QMetaType::QString) {
            int idx = m_values->metaObject()->indexOfProperty(hmiProperty.c_str());
            if (idx >= 0) {
                QMetaProperty mp = m_values->metaObject()->property(idx);
                if (mp.metaType().id() == QMetaType::Bool) {
                    const QString s = val.toString().toLower().trimmed();
                    toSet = QVariant(s == "true" || s == "1");
                }
            }
        }
        m_values->setProperty(hmiProperty.c_str(), toSet);
    }
}

// Rewrites the 4th pipe-delimited field on the matching line in VSS_paths.txt
// so the value survives a KUKSA restart on the next power cycle.
void updateConfigFile(const QString     &filePath,
                      const std::string &hmiProperty,
                      const QString     &newValue)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "updateConfigFile: cannot open for read:" << filePath;
        return;
    }
    QStringList lines;
    QTextStream in(&file);
    bool updated = false;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.startsWith('#') && !line.startsWith('[') &&
            !line.trimmed().isEmpty())
        {
            QStringList parts = line.split('|');
            if (parts.size() >= 3 &&
                parts[2].trimmed().toStdString() == hmiProperty)
            {
                while (parts.size() < 4) parts.append("");
                parts[3] = newValue;
                line = parts.join('|');
                updated = true;
            }
        }
        lines.append(line);
    }
    file.close();
    if (!updated) return;

    // [FIX] QSaveFile writes to a temp file and renames on commit, so a power
    //       cut mid-write (ignition off) can no longer leave a 0-byte config
    //       that makes the next boot subscribe to nothing.
    QSaveFile out_file(filePath);
    if (!out_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "updateConfigFile: cannot open for write:" << filePath;
        return;
    }
    QTextStream out(&out_file);
    for (const QString &l : lines) out << l << '\n';
    out.flush();
    if (!out_file.commit()) {
        qDebug() << "updateConfigFile: commit failed:" << filePath;
        return;
    }
    qDebug() << "Persisted" << QString::fromStdString(hmiProperty)
             << "=" << newValue << "to" << filePath;
}

private:
    void subscribePath(const std::string &vssPath,
                       const std::string &type,
                       const std::string &hmiProperty)
    {
        m_client->subscribe({ vssPath },
            [this, vssPath, type, hmiProperty]
            (const std::string &path, const kuksa::val::v2::Value &value)
        {
            if (!m_values) return;

            QVariant newValue;

            switch (value.typed_value_case()) {
                case kuksa::val::v2::Value::kFloat:
                    newValue = value.float_();
                    break;
                case kuksa::val::v2::Value::kDouble:
                    newValue = value.double_();
                    break;
                case kuksa::val::v2::Value::kBool:
                    newValue = value.bool_();
                    break;
                case kuksa::val::v2::Value::kInt32:
                    newValue = value.int32();
                    break;
                case kuksa::val::v2::Value::kUint32:
                    newValue = value.uint32();
                    break;
                case kuksa::val::v2::Value::kInt64:
                    newValue = static_cast<qint64>(value.int64());
                    break;
                case kuksa::val::v2::Value::kUint64:
                    newValue = static_cast<quint64>(value.uint64());
                    break;
                case kuksa::val::v2::Value::kString:
                    newValue = QString::fromStdString(value.string());
                    break;
                case kuksa::val::v2::Value::TYPED_VALUE_NOT_SET:
                    qDebug() << "Signal uninitialized in Databroker. Defaulting to false.";
                    newValue = false;
                    break;
                default:
                    qDebug() << "WARNING: Unsupported VSS data type ID:"
                             << value.typed_value_case();
                    return;
            }

            if (!newValue.isValid()) return;

            QMetaObject::invokeMethod(m_values,
                [this, hmiProperty, newValue, type]()
            {
                m_values->setProperty(hmiProperty.c_str(), newValue);

                if (hmiProperty == "kph" && type == "float") {
                    m_values->setProperty("displayKph",
                        QString::number(static_cast<int>(newValue.toFloat())));
                }
                if (hmiProperty == "rpm" && type == "float") {
                    m_values->setProperty("displayRpm",
                        QString::number(static_cast<int>(newValue.toFloat())));
                }
                if (hmiProperty == "liters" && type == "float") {
                    float realLiters = newValue.toFloat();
                    m_values->setProperty("liters", static_cast<int>(realLiters * 10));
                    m_values->setProperty("displayLiters",
                        QString::number(realLiters, 'f', 2));
                }
                if (hmiProperty == "temp" && type == "float") {
                    m_values->setProperty("displayTemp",
                        QString::number(static_cast<int>(newValue.toFloat())));
                }
                if (hmiProperty == "totalKms") {
                    m_values->setProperty("displayTotalKms",
                        QString::number(static_cast<int>(
                            std::round(newValue.toFloat()))));
                }
                if (hmiProperty == "tripKms") {
                    m_values->setProperty("displayTripKms",
                        QString::number(newValue.toFloat(), 'f', 1));
                }
            });
        });
    }

    QObject *m_values;
    std::shared_ptr<kuksa::KuksaClient> m_client;

    // Odometer/trip accumulation state
    double m_totalKmsAccum = 0.0;
    double m_tripKmsAccum  = 0.0;
    QString m_lastTotalKmsStr;
    QString m_lastTripKmsStr;
    QTimer *m_odoTimer = nullptr;
    QString m_filePath;
};

// =============================================================================
// main()
// =============================================================================

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setDesktopFileName("cluster-dashboard");

    // 1. Detect Wayland / X11
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    struct agl_shell *agl_shell = register_agl_shell(native);

    // 2. Engine
    QQmlApplicationEngine engine;

    // 3. SystemMonitor — must be set before load
    SystemMonitor sysMon;
    engine.rootContext()->setContextProperty("SysHealth", &sysMon);

    // 4. Import paths
    // [FIX] Replaced hardcoded /home/louis paths with portable equivalents
    // [FIX] Removed qrc:/qt/qml/ClusterTutorial and
    //       qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir as import paths —
    //       these are resource FILE paths, not module roots. The correct root is
    //       qrc:/qt/qml and Qt resolves module URIs from there automatically.
    // [FIX] Removed qmlRegisterSingletonType — Values.qml is a pragma Singleton
    //       declared in qmldir. Manual C++ registration conflicts with qmldir
    //       registration and was the root cause of the singletonInstance() returning
    //       nullptr (double-registration with mismatched URI "Data" vs "ClusterTutorial").
    // Resource dump confirmed the tree is rooted at :/ClusterTutorial/ — NOT :/qt/qml/
    // Structure:
    //   :/ClusterTutorial/                         <- addImportPath points HERE
    //   :/ClusterTutorial/ClusterTutorial/          <- Qt finds "ClusterTutorial" module
    //   :/ClusterTutorial/ClusterTutorial/qmldir/   <- all QML files + qmldir manifest
    engine.addImportPath(QStringLiteral("qrc:/ClusterTutorial"));
    engine.addImportPath(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath("../imports"));
    engine.addImportPath(QStringLiteral("%1/Qt/6.10.3/gcc_64/qml")
                             .arg(QDir::homePath()));

    // Confirmed exact path from resource dump:
    // :/ClusterTutorial/ClusterTutorial/qmldir/App.qml
    const QUrl appUrl(QStringLiteral(
        "qrc:/ClusterTutorial/ClusterTutorial/qmldir/App.qml"));

    // 5. Load — MUST happen before singletonInstance()
    if (agl_shell) {
        qDebug() << "AGL Mode detected.";
        QQmlComponent bg_comp(&engine, appUrl);

        if (bg_comp.status() != QQmlComponent::Ready) {
            qCritical() << "AGL component not ready:" << bg_comp.errorString();
            return -1;
        }

        QObject *qobj_bg = nullptr;
        QScreen *screen = qApp->primaryScreen();
        struct wl_surface *bg = create_component(native, &bg_comp, screen, &qobj_bg);
        struct wl_output *output = getWlOutput(native, screen);
        agl_shell_set_background(agl_shell, bg, output);
        QTimer::singleShot(500, [agl_shell]() { agl_shell_ready(agl_shell); });

    } else {
        qDebug() << "Desktop Mode: loading" << appUrl;
        engine.load(appUrl);

        if (engine.rootObjects().isEmpty()) {
            qCritical() << "Error: Could not load the QML application engine.";
            return -1;
        }

        // Nudge window onto the 7-inch screen (1024x600) at startup.
        // The app runs windowed+frameless so the user can drag it between screens;
        // App.qml's onScreenChanged resizes to fill whichever screen it lands on.
        QTimer::singleShot(100, &app, [&engine]() {
            if (engine.rootObjects().isEmpty()) return;
            QWindow *win = qobject_cast<QWindow *>(engine.rootObjects().first());
            if (!win) return;
            QScreen *target = nullptr;
            for (QScreen *s : QGuiApplication::screens()) {
                if (s->size() == QSize(1024, 600)) { target = s; break; }
            }
            if (target && win->screen() != target) {
                qDebug() << "Moving window to 7-inch screen:" << target->name();
                win->setScreen(target);
            }
            win->showFullScreen();
        });
    }

    // 6. Retrieve Values singleton AFTER engine has loaded.
    //
    // Why the probe approach:
    //   engine.load() with a direct qrc:// URL does not trigger the module
    //   registry the same way loadFromModule() does, so singletonInstance()
    //   by URI string returns nullptr. Instead we instantiate a tiny inline
    //   component that imports the singleton's qrc:// URL directly — this
    //   forces the engine to create the singleton instance and hand us the
    //   pointer via a property.
    //
    // The probe QML is:
    //   import "<values-url>"; QtObject { property var v: Values }
    // setData() needs a base URL so relative imports resolve correctly;
    // we use the qmldir folder as the base.

    // baseUrl must point to an actual FILE (not a directory) so the engine
    // can derive the correct search directory from it.
    // We use App.qml as the anchor — it lives in the same folder as Values.qml
    // and is confirmed loaded, so the engine knows the directory is valid.
    const QUrl baseUrl(QStringLiteral(
        "qrc:/ClusterTutorial/ClusterTutorial/qmldir/App.qml"));

    // Import Values.qml by its relative name — the engine resolves it from
    // the directory of baseUrl, which is the qmldir folder.
    const QByteArray probeQml =
        "import QtQuick 2.15\n"
        "import \".\"\n"
        "QtObject { property var v: Values }";

    QQmlComponent probe(&engine);
    probe.setData(probeQml, baseUrl);

    QObject *valuesObject = nullptr;
    if (probe.status() == QQmlComponent::Ready) {
        QObject *probeObj = probe.create();
        if (probeObj) {
            valuesObject = qvariant_cast<QObject*>(probeObj->property("v"));
            // probeObj is a tiny wrapper — delete it, the singleton itself
            // is owned by the engine and stays alive for the app lifetime.
            probeObj->deleteLater();
        }
    } else {
        qWarning() << "Values probe component failed:" << probe.errorString();
    }

    if (valuesObject) {
        qDebug() << "Values singleton acquired — starting KuksaHandler.";
        KuksaHandler *handler = new KuksaHandler(valuesObject, &app);
        handler->loadConfig(QCoreApplication::applicationDirPath() + "/VSS_paths.txt");

        QObject::connect(&sysMon, &SystemMonitor::tripResetTriggered,
                          handler, &KuksaHandler::resetTripMeter);
    } else {
        qWarning() << "Values singleton not found — Kuksa data will not update the dashboard."
                   << "\n  Check 1: ClusterTutorial/qmldir/qmldir contains:"
                   << "\n           singleton Values 1.0 Values.qml"
                   << "\n  Check 2: Values.qml has 'pragma Singleton' at the top"
                   << "\n  Check 3: No qmlRegisterSingletonType() call for Values anywhere";
    }

    return app.exec();
}

#include "main.moc"
