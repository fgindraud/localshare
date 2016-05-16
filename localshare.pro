CONFIG += c++11
CONFIG += debug

TEMPLATE = app

HEADERS += src/localshare.h \
			src/discovery.h \
			src/settings.h \
			src/style.h \
			src/struct_item_model.h \
			src/transfer.h \
			src/transfer_server.h \
			src/transfer_upload.h \
			src/transfer_download.h \
			src/transfer_model.h \
			src/transfer_delegate.h \
			src/peer_list.h \
			src/window.h
SOURCES += src/main.cpp

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

RESOURCES += resources/resources.qrc

macx: ICON = resources/mac/icon.icns # Mac bundle icon

