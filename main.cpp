#include <QVariant> // Add this at the top of your file if not already present
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QDebug>
#include <QTimer>
#include <QQuickView>
#include <QFile>
#include <QTextStream>
#include <fstream>
#include <string>
#include <vector>
#include <QStandardPaths>
#include <QDir>
#include <kuksaclient/kuksaclient.h>
#include <QQuickView>
#include <QQmlEngine>
#include <QScreen>
#include <QDirIterator>

using namespace kuksa;

// --- 2. KUKSA HANDLER CLASS ---
class KuksaHandler : public QObject {
    Q_OBJECT
public:
    KuksaHandler(QObject* valuesObj, QObject* parent = nullptr) 
        : QObject(parent), m_values(valuesObj) {
        
        m_client = std::make_shared<KuksaClient>();
        
        // Use your verified working v2 connection
        bool connectionStatus = m_client->connect_v2("127.0.0.1:55555");
        
        if (connectionStatus) {
            qDebug() << "Kuksa V2 Connection is Successful!";
        } else {
            qDebug() << "CRITICAL: Kuksa V2 Connection Failed!";
        }
    }

void loadConfig(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "ERROR: Could not open VSS config file:" << filePath;
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // Skip empty lines or your tags from the text file
        if (line.isEmpty() || line.startsWith("#") || line.startsWith("[")) {
            continue;
        }

        QStringList parts = line.split('|');
        if (parts.size() >= 3) {
            std::string path = parts[0].toStdString();
            std::string type = parts[1].toStdString();
            std::string hmi  = parts[2].toStdString();

            // --- DEBUG PRINT BEFORE SUBSCRIBE ---
            qDebug() << "Attempting Subscribe -> Path:" << QString::fromStdString(path) 
                     << "| Type:" << QString::fromStdString(type) 
                     << "| HMI Property:" << QString::fromStdString(hmi);

            subscribePath(path, type, hmi);
        } else {
            qDebug() << "WARNING: Skipping malformed line:" << line;
        }
    }
}

private:
    void subscribePath(const std::string& vssPath, const std::string& type, const std::string& hmiProperty) {
        
        // Note the v2::Value parameter
        m_client->subscribe({ vssPath }, [this, vssPath, type, hmiProperty](const std::string& path, const kuksa::val::v2::Value& value) {
            
            if (!m_values) return;

            QVariant newValue;
            
            // Extract data using v2 typed_value_case
switch (value.typed_value_case()) {
    case kuksa::val::v2::Value::kFloat:
        newValue = value.float_();
        break;
        
    case kuksa::val::v2::Value::kDouble: // Catches high-precision speed
        newValue = value.double_();
        break;
        
    case kuksa::val::v2::Value::kBool:
        newValue = value.bool_();
        break;
        
    case kuksa::val::v2::Value::kInt32: // Catches VSS int8, int16, and int32
        newValue = value.int32(); 
        break;
        
    case kuksa::val::v2::Value::kUint32: // Catches VSS uint8, uint16 (like RPM), and uint32
        newValue = value.uint32();
        break;
        
    case kuksa::val::v2::Value::kInt64:
        newValue = static_cast<qint64>(value.int64()); // Safely cast to Qt's 64-bit int
        break;
        
    case kuksa::val::v2::Value::kUint64:
        newValue = static_cast<quint64>(value.uint64()); 
        break;
        
    case kuksa::val::v2::Value::kString:
        newValue = QString::fromStdString(value.string());
        break;
//DATA_TYPE_UNSPECIFIED
    case kuksa::val::v2::Value::TYPED_VALUE_NOT_SET:
        qDebug() << "Signal is uninitialized in Databroker. Defaulting to false.";
        newValue = false; // Or however you want to handle default states!
        break;
    // ----------------------
    default:
        qDebug() << "WARNING: Received unsupported VSS data type! ID:" << value.typed_value_case();
        return; 
}

            // Thread-safe update to the main GUI
            if (newValue.isValid()) {
                QMetaObject::invokeMethod(m_values, [this, hmiProperty, newValue, type]() {
                    m_values->setProperty(hmiProperty.c_str(), newValue);
                    
                    if (hmiProperty == "kph" && type == "float") {
                        m_values->setProperty("displayKph", QString::number(static_cast<int>(newValue.toFloat())));
                    }
                    if (hmiProperty == "rpm" && type == "float") {
                        m_values->setProperty("displayRpm", QString::number(static_cast<int>(newValue.toFloat())));
                    }
                    if (hmiProperty == "liters" && type == "float") {
                        m_values->setProperty("displayLiters", QString::number(static_cast<int>(newValue.toFloat())));
                    }
                    if (hmiProperty == "temp" && type == "float") {
                        m_values->setProperty("displayTemp", QString::number(static_cast<int>(newValue.toFloat())));
                    }
                    
                });
            }
        });
    }

    QObject* m_values;
    std::shared_ptr<kuksa::KuksaClient> m_client;
}; //Qobject

// --- 3. MAIN ---
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("MyProject");
    app.setApplicationName("ClusterTutorial");
    app.setDesktopFileName("cluster-dashboard"); // <-- THIS BECOMES YOUR WAYLAND app_id
     
    QQuickView view;
    QQmlEngine *engine = view.engine();
    
    // 1. Tell the engine to look in the default Qt 6 module folder
    engine->addImportPath("qrc:/");
    // 2. Tell the engine to look in our bundled internal resources for imports
    engine->addImportPath("qrc:/qt/qml/ClusterTutorial/imports");
    // 3. The standard Qt 6 virtual module path
    engine->addImportPath("qrc:/qt/qml");
    
    // 4. NEW: Tell the engine to look in the physical 'imports' folder next to the app!
    engine->addImportPath(QCoreApplication::applicationDirPath() + "/imports");
    
    // Register Singleton using three slashes (absolute QRC root)
    int valuesTypeId = qmlRegisterSingletonType(
        QUrl("qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/Values.qml"), 
        "Data", 1, 0, "Values"
    );
    
    QObject *valuesObject = engine->singletonInstance<QObject*>(valuesTypeId);

    if (valuesObject) {
        KuksaHandler *handler = new KuksaHandler(valuesObject, &app);
        QString configPath = QCoreApplication::applicationDirPath() + "/VSS_paths.txt";
        if (!QFile::exists(configPath)) {
            configPath = "/usr/share/ClusterTutorial/VSS_paths.txt";
        }
        handler->loadConfig(configPath);
    }

    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setGeometry(0, 0, 1280, 720); 

    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        // Only print QML files to keep the output clean
        if (path.endsWith(".qml")) { 
            qDebug() << path;
        }
    }
    qDebug() << "======================================\n";
// --- TEMPORARY PATH FINDER ---
   //QtDirIterator it(":", QDirIterator::Subdirectories);
   // while (it.hasNext()) {
   //     QString qrcPath = it.next();
   //     if (qrcPath.contains("App.qml")) {
   //         qDebug() << "=== FOUND IT! Use this exact string in view.setSource: ===";
   //         qDebug() << "qrc" + qrcPath;
   //     }
   // }
    // Your current setSource line
    // Set the source using three slashes
    // Now load your app
    //const QUrl url(u"qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/App.qml"_qs);
    
    view.setSource(QUrl("qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/App.qml"));
    if (view.status() == QQuickView::Error) {
        qDebug() << "QML Errors:" << view.errors();
        return -1;
    }

    //This tells Qt to stretch the QML content when the window changes size
    //view.setResizeMode(QQuickView::SizeRootObjectToView);
    
    view.showFullScreen();
    return app.exec();
}
#include "main.moc"
