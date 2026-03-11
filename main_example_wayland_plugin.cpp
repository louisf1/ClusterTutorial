/*
 * Copyright (C) 2016 The Qt Company Ltd.
 * Copyright (C) 2018,2019,2020,2022,2025 Konsulko Group
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <QtCore/QDebug>
#include <QGuiApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QUrlQuery>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlComponent>
#include <QtQml/qqml.h>
#include <QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <qpa/qplatformnativeinterface.h>
#include <QTimer>
#include <glib.h>
#include <QDebug>
#include <QScreen>

#include <wayland-client.h>
#include "agl-shell-client-protocol.h"

#include <vehiclesignals.h>

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

static void
global_remove(void *data, struct wl_registry *reg, uint32_t id)
{
	(void) data;
	(void) reg;
	(void) id;
}

static const struct wl_registry_listener registry_listener = {
	global_add,
	global_remove,
};

static struct agl_shell *
register_agl_shell(QPlatformNativeInterface *native)
{
	struct wl_display *wl;
	struct wl_registry *registry;
	struct agl_shell *shell = nullptr;

	wl = static_cast<struct wl_display *>(native->nativeResourceForIntegration("display"));
	registry = wl_display_get_registry(wl);

	wl_registry_add_listener(registry, &registry_listener, &shell);
	wl_display_roundtrip(wl);
	wl_registry_destroy(registry);

	return shell;
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

void read_config(void)
{
	GKeyFile* conf_file;
	gboolean value;

	// Load settings from configuration file if it exists
	conf_file = g_key_file_new();
	if(conf_file &&
	   g_key_file_load_from_dirs(conf_file,
				     "AGL.conf",
				     (const gchar**) g_get_system_config_dirs(),
				     NULL,
				     G_KEY_FILE_KEEP_COMMENTS,
				     NULL) == TRUE) {
		GError *err = NULL;
		value = g_key_file_get_boolean(conf_file,
					       "dashboard",
					       "animation",
					       &err);
		if(value) {
			runAnimation = true;
		} else {
			if(err == NULL) {
				runAnimation = false;
			} else {
				qWarning("Invalid value for \"animation\" key!");
			}
		}
	}

}

static struct wl_surface *
create_component(QPlatformNativeInterface *native, QQmlComponent *comp,
		QScreen *screen, QObject **qobj)
{
	QObject *obj = comp->create();
	//QObject *screen_obj = new QScreen(screen);
	obj->setParent(screen);

	QWindow *win = qobject_cast<QWindow *>(obj);
	*qobj = obj;

	return getWlSurface(native, win);
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

int main(int argc, char *argv[])
{
	QString myname = QString("cluster-dashboard");
	struct agl_shell *agl_shell;
	struct wl_output *output;

	QObject *qobj_bg;
	QScreen *screen;

	QGuiApplication app(argc, argv);
	app.setDesktopFileName(myname);
	QPlatformNativeInterface *native = qApp->platformNativeInterface();

	agl_shell = register_agl_shell(native);
	if (!agl_shell) {
		exit(EXIT_FAILURE);
	}

	std::shared_ptr<struct agl_shell> shell{agl_shell, agl_shell_destroy};

	const char *screen_name = getenv("DASHBOARD_START_SCREEN");
	if (screen_name)
		screen = find_screen(screen_name);
	else
		screen = qApp->primaryScreen();
	output = getWlOutput(native, screen);

	read_config();

	QQmlApplicationEngine engine;
	QQmlContext *context = engine.rootContext();
	context->setContextProperty("runAnimation", runAnimation);

	VehicleSignalsConfig vsConfig(myname);
	context->setContextProperty("VehicleSignals", new VehicleSignals(vsConfig));

	QQmlComponent bg_comp(&engine, QUrl("qrc:/cluster-dashboard.qml"));
	qDebug() << bg_comp.errors();
	struct wl_surface *bg = create_component(native, &bg_comp, screen, &qobj_bg);

	// set the surface as the background
	agl_shell_set_background(agl_shell, bg, output);

	// instruct the compositor it can display after Qt has a chance
	// to load everything
	QTimer::singleShot(500, [agl_shell](){
		qDebug() << "agl_shell ready!";
		agl_shell_ready(agl_shell);
	});

	return app.exec();
}
