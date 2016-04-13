# Needs c++11
CONFIG += c++11

TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += .
QT += network widgets
RESOURCES += icon.qrc

CONFIG += debug_and_release
CONFIG(debug, debug|release) {
	TARGET = debug_localshare
} else {
	TARGET = localshare
}

# Input
HEADERS += common.h localshare.h mainWindow.h network.h peerWidgets.h miscWidgets.h transfer.h
SOURCES += common.cpp main.cpp mainWindow.cpp network.cpp peerWidgets.cpp miscWidgets.cpp transfer.cpp
