# Needs c++11
CONFIG += c++11

TEMPLATE = app

DEPENDPATH += .
INCLUDEPATH += .

QT += core network widgets
RESOURCES += icon.qrc

# DNS service discovery library
unix {
	# Provided by avahi-compat-libdns_sd
	LIBS += -ldns_sd
}
mac {
	# Seems to be part of stdlib there
}
win32 {
	# Provided by mDNSResponder (Bonjour windows service)
	# May require custom LIBPATH/INCLUDEPATH
	LIBS += -ldnssd
}

# Input
HEADERS += localshare.h discovery.h settings.h style.h
SOURCES += main.cpp
