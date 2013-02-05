#ifndef H_COMMON
#define H_COMMON

#include <QtCore>
#include <QHostAddress>
#include <QCommonStyle>

/*
 * Handle program settings, and defaults values
 */
class Settings {
	public:
		/* Network settings */
		static QString name (void);
		static QString defaultName (void);
		static void setName (QString & name);

		static quint16 tcpPort (void);
		static quint16 defaultTcpPort (void);
		static void setTcpPort (quint16 port);

		/* Download path/confirmation settings */
		static QString downloadPath (void);
		static QString defaultDownloadPath (void);
		static void setDownloadPath (const QString & path);

		static bool alwaysDownload (void);
		static bool defaultAlwaysDownload (void);
		static void setAlwaysDownload (bool always);

		/* Use system tray */
		static bool useSystemTray (void);
		static bool defaultUseSystemTray (void);
		static void setUseSystemTray (bool use);

	private:
		// Internal instance
		static QSettings settings;

		// Used to make settings accessors
		template< typename T >
		static T getValueCached (
			const QString & key,
			T (*defaultFactory) (void),
			void (*postProcessor) (T &) = 0
		) {
			// Cache value if not already done
			if (not settings.contains (key))
				settings.setValue (key, defaultFactory ());
			// Get value
			T value = settings.value (key).value<T> ();
			// Post process it if needed
			if (postProcessor != 0)
				postProcessor (value);
			// Return it
			return value;
		}
};

/*
 * Error messages
 */
class Message {
	public:
		static void error (const QString & title, const QString & message);
		static void warning (const QString & title, const QString & message);
};

/*
 * Icon settings for the application
 */
class Icon {
	public:
		// Application icon
		static QIcon app (void);

		// Represents a file
		static QIcon file (void);

		// Open file & Settings dialog buttons
		static QIcon openFile (void);
		static QIcon settings (void);

		// Accept and refuse/close icon
		static QIcon accept (void);
		static QIcon closeAbort (void);

		// In/out-bound transfer icons
		static QIcon inbound (void);
		static QIcon outbound (void);

	private:
		static QCommonStyle style;
};

/*
 * File size pretty printer
 */
QString fileSizeToString (quint64 size);

#endif

