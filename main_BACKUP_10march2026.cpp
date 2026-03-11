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
   qDebug() << "Start MAIN";
    QGuiApplication app(argc, argv);
    app.setOrganizationName("MyProject");
    app.setApplicationName("cluster-dashboard");
    app.setDesktopFileName("cluster-dashboard"); // <-- THIS BECOMES YOUR WAYLAND app_id

 	struct agl_shell *agl_shell;
	struct wl_output *output;

	QObject *qobj_bg;
	QScreen *screen;

   qDebug() << "DO: QPlatformNativeInterface *native = qApp->platformNativeInterface();";
	QPlatformNativeInterface *native = qApp->platformNativeInterface();
   qDebug() << "TRY: agl_shell = register_agl_shell(native);";
	agl_shell = register_agl_shell(native);
   qDebug() << "COMPLETED: agl_shell = register_agl_shell(native);";
	
	// --- VERBOSE SHELL DEBUGGING ---
        qDebug() << "DEBUG: Checking AGL Shell pointer status...";
        if (agl_shell) {
           qDebug() << "SUCCESS: agl_shell is active at address:" << agl_shell;
        } else {
           qDebug() << "WARNING: agl_shell is NULL. AGL-specific features are disabled.";
        }
	if (!agl_shell){
           qDebug() << "NOTED exit(EXIT_FAILURE);, TRY X11 instead ";	        
           //	exit(EXIT_FAILURE);
	}
	// REPLACE WITH THIS FALLBACK:
       if (!agl_shell) {
          qDebug() << "Desktop Mode: AGL Shell not found. Running with standard window.";
          // On desktop, we just use a regular QQuickView
          view.show(); 
       } else {
          qDebug() << "AGL Mode: Shell found. Proceeding with background mapping.";
        // ... put your agl_shell_set_background logic here ...
       }
	
        qDebug() << "std::shared_ptr<struct agl_shell> shell{agl_shell, agl_shell_destroy};";	        
	std::shared_ptr<struct agl_shell> shell{agl_shell, agl_shell_destroy};

        qDebug() << "TRY: const char *screen_name = getenv(\"DASHBOARD_START_SCREEN\");";	        
	const char *screen_name = getenv("DASHBOARD_START_SCREEN");
	if (screen_name)
		screen = find_screen(screen_name);
	else
		screen = qApp->primaryScreen();
        qDebug() << "TRY: output = getWlOutput(native, screen);";	        
	output = getWlOutput(native, screen);
    
    QQuickView view;
   //This tells Qt to stretch the QML content when the window changes size
   
    qDebug() << "TRY: view.setResizeMode(QQuickView::SizeRootObjectToView);";	        
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    qDebug() << "TRY: view.setGeometry(0, 0, 1280, 720); ";	            
    view.setGeometry(0, 0, 1280, 720); 
    qDebug() << "TRY:     QQmlEngine *engine = view.engine();"; 
    QQmlEngine *engine = view.engine();

    // 3.1 Ensure the physical path is FIRST in the search order
    qDebug() << "TRY: physicalImports;"; 
    
    QString physicalImports = QCoreApplication::applicationDirPath() + "/imports";
    engine->addImportPath(QCoreApplication::applicationDirPath() + "/imports");
    engine->addImportPath(QCoreApplication::applicationDirPath() + "/custom_libs");
    engine->addImportPath(physicalImports);
    qDebug() << "Searching for plugins in:" << physicalImports;
    
     // 1. Root and standard QML paths
     engine->addImportPath("qrc:/qt/qml");
    // 2. Point specifically to the system folder where we installed the Studio plugins
     engine->addImportPath("/usr/lib/qml"); 
    // 3. Point to the URI root for your internal files
     engine->addImportPath("qrc:/qt/qml/ClusterTutorial");
    // 4. The internal imports folder (Satisfies Studio Components)
    // 5. NEW: Tell the engine to look in the physical 'imports' folder next to the app!
    // Add the physical folder where your .so plugins live
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

    //view.setResizeMode(QQuickView::SizeRootObjectToView);
    //qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    //QDirIterator it(":", QDirIterator::Subdirectories);
    //while (it.hasNext()) {
    //    QString path = it.next();
        // Only print QML files to keep the output clean
    //    if (path.endsWith(".qml")) { 
    //        qDebug() << path;
    //    }
    //}
    //qDebug() << "======================================\n";
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
    
    //view.setSource(QUrl("qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/App.qml"));
    //if (view.status() == QQuickView::Error) {
    //    qDebug() << "QML Errors:" << view.errors();
    //    return -1;
    // }
    qDebug() << "DEBUG: Attempting to load App.qml...";
    QQmlComponent bg_comp(engine, QUrl("qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/App.qml"));    
    if (bg_comp.isError()) {
        qDebug() << "CRITICAL: bg_comp has errors:";
        for (const auto &error : bg_comp.errors()) {
            qDebug() << "  ->" << error.toString();
        }
    } else {
        qDebug() << "SUCCESS: bg_comp loaded correctly. Status:" << bg_comp.status();
    }

    // Check if the component loaded successfully
    if (bg_comp.isError()) {
        qCritical() << "--- QML Component Error Report ---";
        // Iterate through all errors (pathing, syntax, missing imports)
        for (const QQmlError &error : bg_comp.errors()) {
            qCritical() << "File:" << error.url().toString();
            qCritical() << "Line:" << error.line() << "Column:" << error.column();
            qCritical() << "Message:" << error.description();
        }
        qCritical() << "---------------------------------";
        return -1; // Stop execution before the Segfault occurs
    } 
       
    struct wl_surface *bg = create_component(native, &bg_comp, screen, &qobj_bg);
    if (!bg) {
        qDebug() << "CRITICAL: wl_surface 'bg' is NULL! This will cause a crash if used.";
    } else {
        qDebug() << "SUCCESS: wl_surface 'bg' created at address:" << bg;
    }
    
    // CRITICAL SAFETY CHECK: Only call AGL functions if the surface and shell exist
// --- THE SAFETY GATE ---
    if (agl_shell && bg && output) {
        qDebug() << "ACTION: All pointers valid. Calling agl_shell_set_background...";
        agl_shell_set_background(agl_shell, bg, output);
        
        QTimer::singleShot(500, [agl_shell]() {
            qDebug() << "ACTION: Sending agl_shell_ready signal...";
            agl_shell_ready(agl_shell);
        });
    } else {
        qDebug() << "ABORT: Safety check failed. agl_shell/bg/output missing. Skipping AGL setup.";
        // On Desktop, we manually show the window so it doesn't just stay invisible
        QWindow *win = qobject_cast<QWindow *>(qobj_bg);
        if (win) {
            qDebug() << "ACTION: Desktop Mode detected. Showing window normally.";
            win->show();
        }
    }

    return app.exec();
}
#include "main.moc"

  QGuiApplication app(argc, argv);
    app.setApplicationName("cluster-dashboard");
    app.setDesktopFileName("cluster-dashboard"); // <-- THIS BECOMES YOUR WAYLAND app_id

    // 1. Handshake: Try to find AGL, but don't exit if it fails
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    struct agl_shell *agl_shell = register_agl_shell(native);

    // 2. Setup Engine and Paths (Crucial: Do this BEFORE showing anything)
    QQuickView view;
    QQmlEngine *engine = view.engine();

    // 1. SYSTEM QT FIRST: This provides the C++ binaries for Effects/Glow/Kuksa
    engine->addImportPath("/home/louis/Qt/6.10.2/gcc_64/qml"); 
    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it1(":", QDirIterator::Subdirectories);
    while (it1.hasNext()) {
        QString path = it1.next();
        // Only print QML files to keep the output clean
        //if (path.endsWith(".qml")) { 
            qDebug() << path;
        //}
    }
    // 2. PROJECT IMPORTS SECOND: This provides your Design Studio components
    QString projectImports = QCoreApplication::applicationDirPath() + "/imports";
    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it2(":", QDirIterator::Subdirectories);
    while (it2.hasNext()) {
        QString path = it2.next();
        // Only print QML files to keep the output clean
        //if (path.endsWith(".qml")) { 
            qDebug() << path;
        //}
    }
    engine->addImportPath(projectImports);
    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it3(":", QDirIterator::Subdirectories);
    while (it3.hasNext()) {
        QString path = it3.next();
        // Only print QML files to keep the output clean
        //if (path.endsWith(".qml")) { 
            qDebug() << path;
        //}
    }    
    // 3. CUSTOM LIBS THIRD: For any manually compiled .so files
    engine->addImportPath(QCoreApplication::applicationDirPath() + "/custom_libs");
    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it4(":", QDirIterator::Subdirectories);
    while (it4.hasNext()) {
        QString path = it4.next();
        // Only print QML files to keep the output clean
        //if (path.endsWith(".qml")) { 
            qDebug() << path;
        //}
    }
    // 4. QRC RESOURCES LAST: For internal QML logic files
    engine->addImportPath("qrc:/qt/qml");
    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it5(":", QDirIterator::Subdirectories);
    while (it5.hasNext()) {
        QString path = it5.next();
        // Only print QML files to keep the output clean
        //if (path.endsWith(".qml")) { 
            qDebug() << path;
        //}
    }
    engine->addImportPath("qrc:/qt/qml/ClusterTutorial");
    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it6(":", QDirIterator::Subdirectories);
    while (it6.hasNext()) {
        QString path = it6.next();
        // Only print QML files to keep the output clean
        //if (path.endsWith(".qml")) { 
            qDebug() << path;
        //}
    }
    qDebug() << "QML Import Stack Initialized. Primary Search Path:" << projectImports;
    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it7(":", QDirIterator::Subdirectories);
    while (it7.hasNext()) {
        QString path = it7.next();
        // Only print QML files to keep the output clean
        //if (path.endsWith(".qml")) { 
            qDebug() << path;
        //}
    }
// CLEAR ALL DEFAULT PATHS FIRST (Start with a clean slate)
engine->setImportPathList(QStringList()); 

// 1. SYSTEM QT FIRST: Essential for C++ plugins like SourceProxy
engine->addImportPath("/home/louis/Qt/6.10.2/gcc_64/qml"); 

// 2. PROJECT RESOURCES: Where your App.qml and UI logic live
engine->addImportPath("qrc:/qt/qml");
engine->addImportPath("qrc:/qt/qml/ClusterTutorial");

// 3. PHYSICAL PROJECT IMPORTS: Last resort for local Design Studio assets
engine->addImportPath(QCoreApplication::applicationDirPath() + "/imports");

// VERIFICATION: Print the list to ensure the system path is INDEX 0
qDebug() << "FINAL IMPORT ORDER:" << engine->importPathList();    
    qDebug() << "\n=== DUMPING ALL INTERNAL QRC PATHS ===";
    QDirIterator it8(":", QDirIterator::Subdirectories);
    while (it8.hasNext()) {
        QString path = it8.next();
        // Only print QML files to keep the output clean
        //if (path.endsWith(".qml")) { 
            qDebug() << path;
        //}
    }
    qDebug() << "======================================\n";     

    // 3. Initialize Kuksa and Data (So the Odometer is ready)
    int valuesTypeId = qmlRegisterSingletonType(
        QUrl("qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/Values.qml"), 
        "Data", 1, 0, "Values"
    );
    QObject *valuesObject = engine->singletonInstance<QObject*>(valuesTypeId);
    if (valuesObject) {
        KuksaHandler *handler = new KuksaHandler(valuesObject, &app);
        handler->loadConfig(QCoreApplication::applicationDirPath() + "/VSS_paths.txt");
    }

    // 4. Load the QML Component
    QQmlComponent bg_comp(engine, QUrl("qrc:/qt/qml/ClusterTutorial/ClusterTutorial/qmldir/App.qml"));
    if (bg_comp.isError()) {
        qDebug() << "QML Error:" << bg_comp.errors();
        return -1; 
    }

    // 5. The "Decision Point": How to display the dashboard?
    if (agl_shell) {
        qDebug() << "AGL Mode: Mapping to Background Layer.";
        QObject *qobj_bg;
        QScreen *screen = qApp->primaryScreen();
        struct wl_surface *bg = create_component(native, &bg_comp, screen, &qobj_bg);
        struct wl_output *output = getWlOutput(native, screen);

        agl_shell_set_background(agl_shell, bg, output);
        QTimer::singleShot(500, [agl_shell]() { agl_shell_ready(agl_shell); });
    } else {
        qDebug() << "Desktop Mode: Showing standard Window.";
        // Use the 'view' we already configured with the source
        view.setSource(bg_comp.url()); 
        view.show(); 
    }

    return app.exec();

