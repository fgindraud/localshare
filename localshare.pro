# Comment this to only compile the command line interface
CONFIG += localshare_gui

### Compilation ###

TEMPLATE = app
CONFIG += c++11

INCLUDEPATH += src/
CONFIG(release, debug|release): DEFINES += QT_NO_DEBUG_OUTPUT

# Core and cli
QT += core network
HEADERS += \
	src/compatibility.h \
	src/portability.h \
	\
	src/core_discovery.h \
	src/core_localshare.h \
	src/core_payload.h \
	src/core_server.h \
	src/core_settings.h \
	src/core_transfer.h \
	\
	src/cli_indicator.h \
	src/cli_main.h \
	src/cli_transfer.h
SOURCES += \
	src/cli_main.cpp \
	src/main.cpp


### DNS service discovery library ###

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
	HEADERS += src/dns_sd.h src/DLLStub.h
	SOURCES += src/DLLStub.cpp
}

# Misc information

VERSION = 0.3
DEFINES += LOCALSHARE_VERSION=$${VERSION}

QMAKE_TARGET_COMPANY = Francois Gindraud
QMAKE_TARGET_PRODUCT = Localshare
QMAKE_TARGET_DESCRIPTION = Small file sharing application for the local network
QMAKE_TARGET_COPYRIGHT = Copyright (C) 2016 Francois Gindraud

### Add gui stuff if enabled ###
localshare_gui {

DEFINES += LOCALSHARE_HAS_GUI

QT += widgets svg
HEADERS += \
	src/gui_button_delegate.h \
	src/gui_discovery_subsystem.h \
	src/gui_main.h \
	src/gui_peer_list.h \
	src/gui_struct_item_model.h \
	src/gui_style.h \
	src/gui_transfer_download.h \
	src/gui_transfer_list.h \
	src/gui_transfer_upload.h \
	src/gui_window.h
SOURCES += src/gui_main.cpp

# Icons
RESOURCES += resources/resources.qrc
macx: ICON = resources/mac/icon.icns # Mac bundle icon
win32: RC_ICONS += resources/windows/icon.ico # Windows app icon

}
