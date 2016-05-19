CONFIG += c++11
CONFIG += debug

TEMPLATE = app

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

RESOURCES += resources/resources.qrc

macx: ICON = resources/mac/icon.icns # Mac bundle icon
win32: RC_FILE = resources/windows/resources.rc # Windows app icon

