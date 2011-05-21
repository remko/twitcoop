TEMPLATE = app
QT += webkit network
CONFIG -= debug
CONFIG += release
#CONFIG += console

unix {
	TARGET = twitcoop
	target.path = /usr/bin

	desktop.path = /usr/share/applications/
	desktop.files = twitcoop.desktop 

	icon128.path = /usr/share/icons/hicolor/128x128/apps
	icon128.extra = cp -f icon-128.png $$icon128.path/twitcoop.png

	INSTALLS += target desktop icon128
} else {
	TARGET = TwitCoop
}

RC_FILE = TwitCoop.rc
RESOURCES = TwitCoop.qrc
SOURCES += TwitCoop.cpp
