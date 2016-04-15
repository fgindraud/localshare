CONFIG += c++11

TEMPLATE = app

DEPENDPATH += .
INCLUDEPATH += .

HEADERS += localshare.h discovery.h settings.h style.h transfer.h
SOURCES += main.cpp

QT += core network widgets

# DNS service discovery library
unix:!macx: { # Linux
	# Provided by avahi-compat-libdns_sd
	LIBS += -ldns_sd
}
macx: { # Mac
	# No specific library needed
}
win32: { # Win
	# Provided by mDNSResponder (Bonjour windows service)
	# May require custom LIBPATH/INCLUDEPATH
	LIBS += -ldnssd
}

RESOURCES += icon.qrc

macx: ICON = mac/icon.icns # Mac bundle icon

