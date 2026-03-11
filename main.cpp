#include <QVariant> // Add this at the top of your file if not already present
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
//#include <QQmlComponent>
#include <QtQml/QQmlComponent>
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
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>
// Keep the C++ wrapper separate or also inside extern "C" if it lacks guards
#include "qwayland-agl-shell.h"

using namespace kuksa;

// Instead of just wrapping the include, explicitly tell 
// the linker about the interface symbols at the top of main.cpp
extern "C" {
    #include "wayland-agl-shell-client-protocol.h"
    
    // Explicitly re-declare these to ensure the C++ linker 
    // knows they are external C symbols from the .c file
    extern const struct wl_interface agl_shell_interface;
    extern const struct wl_interface agl_shell_ext_interface;
}

static void
global_remove(void *data, struct wl_registry *reg, uint32_t id)
{
	(void) data;
	(void) reg;
	(void) id;
}

// Global pointer to the shell interface
static struct agl_shell *agl_shell = nullptr;

// 1. The Listener: This catches the agl_shell interface when Wayland announces it
//static void global_add(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version) {
//    if (strcmp(interface, agl_shell_interface.name) == 0) {
//        agl_shell = static_cast<struct agl_shell *>(wl_registry_bind(reg, name, &agl_shell_interface, 1));
//    }
//}

// Global indicating whether canned animation should run
bool runAnimation = true;

static void
global_add(void *data, struct wl_registry *reg, uint32_t name,
	   const char *interface, uint32_t version)
{
	struct agl_shell **shell = static_cast<struct agl_shell **>(data);
	if (strcmp(interface, agl_shell_interface.name) == 0) {
		*shell = static_cast<struct agl_shell *>(wl_registry_bind(reg,
					name, &agl_shell_interface, 1)
		);
	}
}


static struct wl_surface *
getWlSurface(QPlatformNativeInterface *native, QWindow *window)
{
	void *surf = native->nativeResourceForWindow("surface", window);
	return static_cast<struct ::wl_surface *>(surf);
}

static struct wl_output *
getWlOutput(QPlatformNativeInterface *native, QScreen *screen)
{
	void *output = native->nativeResourceForScreen("output", screen);
	return static_cast<struct ::wl_output*>(output);
}


static QScreen *find_screen(const char *screen_name)
{
	QList<QScreen *> screens = qApp->screens();
	QString name(screen_name);

	for (QScreen *screen : screens) {
		if (name == screen->name())
			return screen;
	}

	return nullptr;
}



static struct wl_surface *
create_component(QPlatformNativeInterface *native, QQmlComponent *comp,
		QScreen *screen, QObject **qobj)
{
    // Use the component to create the object (your App.qml)
   // new QScreen(screen);
    QObject *obj = comp->create();
    if (!obj) {
        qCritical() << "Failed to create QML component:" << comp->errors();
        return nullptr;
    }

    // A visual QML object is usually a QQuickItem or QWindow
    // If App.qml is a Window {}, this cast will work.
    QWindow *win = qobject_cast<QWindow *>(obj);
    if (!win) {
        qWarning() << "Component is not a QWindow; checking for QQuickView wrapper.";
        // Fallback: If it's a QQuickItem, you need to put it in a View/Window
    }

    *qobj = obj;
    return getWlSurface(native, win);
}

// Update 1: Add safety checks to setupAglShell
void setupAglShell() {
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native) return; // Exit if no native interface

    void *display = native->nativeResourceForIntegration("display");
    if (!display) {
        qDebug() << "Wayland display not found. Skipping AGL handshake for Desktop mode.";
        return; 
    }

    struct wl_display *wl = static_cast<struct wl_display *>(display);
    struct wl_registry *registry = wl_display_get_registry(wl);
    // ... rest of the code ...
}

static const struct wl_registry_listener registry_listener = { 
        global_add, 
	global_remove        
  ///      [](void*, wl_registry*, uint32_t){} };
};

static struct agl_shell *
register_agl_shell(QPlatformNativeInterface *native)
{
    struct wl_display *wl;
    struct wl_registry *registry;
    struct agl_shell *shell = nullptr;

qDebug() << "TRY: native->nativeResourceForIntegration;";
// 1. Verify the platform first
    if (qGuiApp->platformName() != "wayland") {
        qDebug() << "PLATFORM CHECK: Currently on" << qGuiApp->platformName() 
                 << "- Skipping Wayland registry calls to prevent Segfault.";
        return nullptr;
    }
    // 1. Safety check for the native resource
    void *display = native->nativeResourceForIntegration("display");
qDebug() << "FINISH: native->nativeResourceForIntegration";
    if (!display) {
        qDebug() << "Linker Error: Wayland display not found. Returning null.";
        return nullptr; // Properly exit if not on Wayland
    }
qDebug() << "TRY: wl = static_cast<struct wl_display";    
    wl = static_cast<struct wl_display *>(display);
qDebug() << "COMPLETED: wl = static_cast<struct wl_display";    
qDebug() << "TRY: registry = wl_display_get_registry(wl)";    
    registry = wl_display_get_registry(wl);  //<<<--- segmentation error!!
qDebug() << "COMPLETED: registry = wl_display_get_registry(wl)";    
    // 2. Bind the listener to the local 'shell' pointer
qDebug() << "TRY: wl_registry_add_listener(registry, &registry_listener, &shell);";    
    wl_registry_add_listener(registry, &registry_listener, &shell);
qDebug() << "COMPLETED: wl_registry_add_listener(registry, &registry_listener, &shell);";    
qDebug() << "TRY:  wl_display_roundtrip(wl);";    
    wl_display_roundtrip(wl);
qDebug() << "COMPLETED:  wl_display_roundtrip(wl);";    
qDebug() << "TRY:  wl_registry_destroy(registry);";    
    wl_registry_destroy(registry);
qDebug() << "COMPLETED: wl_registry_destroy(registry);";    

    // 3. CRITICAL: Return the pointer to the caller
    return shell; 
}

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
                    // Inside your QMetaObject::invokeMethod block in main.cpp:
                    if (hmiProperty == "totalKms") {
                        // Round ODO to a whole number string
                        m_values->setProperty("displayTotalKms", QString::number(static_cast<int>(std::round(newValue.toFloat()))));
                    }
                    if (hmiProperty == "tripKms") {
                        // Force 1 decimal place for Trip
                        m_values->setProperty("displayTripKms", QString::number(newValue.toFloat(), 'f', 1));
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
    app.setDesktopFileName("cluster-dashboard");

    // 1. Handshake: Detect platform (Wayland vs X11) 
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    struct agl_shell *agl_shell = register_agl_shell(native);

    // 2. Setup View and Engine 
    QQuickView view;
    QQmlEngine *engine = view.engine();

    // 3. STRICT IMPORT ORDER: System -> Project -> Resources 
    engine->setImportPathList(QStringList());
    engine->addImportPath("/home/louis/Qt/6.10.2/gcc_64/qml"); 
    engine->addImportPath("/home/louis/Documents/QtDesignStudio/examples/ClusterWorx/imports");
    engine->addImportPath("qrc:/qt/qml");
    engine->addImportPath("qrc:/qt/qml/ClusterTutorial");

    // 4. Initialize Kuksa Singleton 
    int valuesTypeId = qmlRegisterSingletonType(
        QUrl("qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/Values.qml"), 
        "Data", 1, 0, "Values"
    );
    
    QObject *valuesObject = engine->singletonInstance<QObject*>(valuesTypeId);
    if (valuesObject) {
        KuksaHandler *handler = new KuksaHandler(valuesObject, &app);
        // Ensure this path matches where you copied the file 
        handler->loadConfig(QCoreApplication::applicationDirPath() + "/VSS_paths.txt");
    }

    // 5. Load the App Component 
    QUrl appUrl("qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/App.qml");
    QQmlComponent bg_comp(engine, appUrl);
    
    if (bg_comp.isError()) {
        qDebug() << "CRITICAL QML Error:" << bg_comp.errors();
        return -1; 
    }

    // 6. Decision Point: AGL Background vs Desktop Window 
    if (agl_shell) {
        qDebug() << "AGL Mode detected.";
        QObject *qobj_bg;
        QScreen *screen = qApp->primaryScreen();
        struct wl_surface *bg = create_component(native, &bg_comp, screen, &qobj_bg);
        struct wl_output *output = getWlOutput(native, screen);

        agl_shell_set_background(agl_shell, bg, output);
        QTimer::singleShot(500, [agl_shell]() { agl_shell_ready(agl_shell); });
    } else {
        qDebug() << "Desktop Mode: Showing window.";
        view.setSource(appUrl); 
        view.show(); 
    }

    return app.exec();     
}

#include "main.moc"


