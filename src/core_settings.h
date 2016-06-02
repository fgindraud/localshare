#pragma once
#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <QByteArray>
#include <QDir>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>

namespace Settings {

template <typename T> class Element {
private:
	QSettings settings;

public:
	virtual ~Element () = default;

	T get (void) {
		if (not settings.contains (key ())) {
			return set (default_value ());
		} else {
			return settings.value (key ()).template value<T> ();
		}
	}

	T set (T new_value) {
		T norm = normalize (new_value);
		settings.setValue (key (), norm);
		return norm;
	}

private:
	virtual const char * key (void) const = 0;
	virtual T default_value (void) const = 0;
	virtual T normalize (T value) { return value; }
};

class Username : public Element<QString> {
	// Username
private:
	const char * key (void) const { return "network/username"; }
	QString default_value (void) const {
		// Try to get a username from classical env variables
		const char * candidates[] = {"USER", "USERNAME", "LOGNAME"};
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment ();
		for (auto key : candidates)
			if (env.contains (key))
				return env.value (key);

		// Or return default if not found
		return "Unknown";
	}
};

class UploadHidden : public Element<bool> {
	// Do we consider hidden files for download when parsing directories ?
private:
	const char * key (void) const { return "download/hidden_files"; }
	bool default_value (void) const { return false; }
};

class DownloadPath : public Element<QString> {
	// Place to store downloaded files
private:
	const char * key (void) const { return "download/path"; }
	QString default_value (void) const {
		auto path = QStandardPaths::writableLocation (QStandardPaths::DownloadLocation);
		if (!path.isEmpty ()) {
			return path;
		} else {
			return QDir::homePath ();
		}
	}
};

class DownloadAuto : public Element<bool> {
	// Automatically start downloading every request
private:
	const char * key (void) const { return "download/auto"; }
	bool default_value (void) const { return false; }
};

class UseTray : public Element<bool> {
	// Allow use of system tray icon if supported
private:
	const char * key (void) const { return "interface/use_tray"; }
	bool default_value (void) const { return true; }
};

class Geometry : public Element<QByteArray> {
	// To save window geometry
private:
	const char * key (void) const { return "interface/geometry"; }
	QByteArray default_value (void) const { return {}; }
};

class WindowState : public Element<QByteArray> {
	// To save window toolbar setup
private:
	const char * key (void) const { return "interface/window_state"; }
	QByteArray default_value (void) const { return {}; }
};
}

#endif
