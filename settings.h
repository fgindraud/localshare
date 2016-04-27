#ifndef SETTINGS_H
#define SETTINGS_H

#include <QtCore>

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

class DownloadPath : public Element<QString> {
	// Place to store downloaded files
private:
	const char * key (void) const { return "download/path"; }
	QString default_value (void) const {
		auto path = QStandardPaths::displayName (QStandardPaths::DownloadLocation);
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
}

#endif
