CONFIG += c++11
#CONFIG += debug

TEMPLATE = app

# Compilation

HEADERS += src/localshare.h \
			src/discovery.h \
			src/settings.h \
			src/style.h \
			src/struct_item_model.h \
			src/button_delegate.h \
			src/transfer.h \
			src/transfer_server.h \
			src/transfer_upload.h \
			src/transfer_download.h \
			src/transfer_list.h \
			src/peer_list.h \
			src/window.h
SOURCES += src/main.cpp

QT += core network widgets svg

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

	# These files should be extracted from mDNSResponder sources (not in git)
	# See build/windows/requirement.sh
	INCLUDEPATH += src/
	HEADERS += src/dns_sd.h src/DLLStub.h
	SOURCES += src/DLLStub.cpp
}

# Icons

RESOURCES += resources/resources.qrc

macx: ICON = resources/mac/icon.icns # Mac bundle icon
win32: RC_ICONS += resources/windows/icon.ico # Windows app icon

# Misc information

VERSION = 0.3
DEFINES += LOCALSHARE_VERSION=$${VERSION}

QMAKE_TARGET_COMPANY = François Gindraud
QMAKE_TARGET_PRODUCT = Localshare
QMAKE_TARGET_DESCRIPTION = Small file sharing application for the local network
QMAKE_TARGET_COPYRIGHT = Copyright (C) 2016 François Gindraud

