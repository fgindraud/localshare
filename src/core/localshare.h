#pragma once
#ifndef CORE_LOCALSHARE_H
#define CORE_LOCALSHARE_H

#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDebug>
#include <tuple>
#include <type_traits>

#include <QHostAddress>

#include "compatibility.h"

namespace Const {
// Application name for settings, etc...
constexpr auto app_name = "localshare";
constexpr auto app_display_name = "Localshare";

#define XSTR(x) #x
#define STR(x) XSTR (x)
constexpr auto app_version = STR (LOCALSHARE_VERSION);
#undef STR
#undef XSTR

// Network service name
constexpr auto service_type = "_localshare._tcp.";

// Protocol
constexpr quint16 protocol_magic = 0x0CAA;
constexpr auto serializer_version = QDataStream::Qt_5_0; // We are only compatible with Qt5 anyway
constexpr auto hash_algorithm = QCryptographicHash::Md5;
constexpr quint16 protocol_version = 0x2;
constexpr auto chunk_size = qint64 (10000);         // TODO adjust for perf. 10Ko ?
constexpr auto write_buffer_size = qint64 (100000); // TODO same. 100Ko ?

// Setup app object (graphical and console version)
inline void setup (QCoreApplication & app) {
	app.setApplicationVersion (Const::app_version);
	// These two enable the use of QSetting default constructor
	app.setOrganizationName (Const::app_name);
	app.setApplicationName (Const::app_name);
}
inline void setup (QApplication & app) {
	setup (static_cast<QCoreApplication &> (app));
	app.setApplicationDisplayName (Const::app_display_name);
}
}

/* Classes can inherit from tag Streamable.
 * It enables them to be QDataStream compatible through methods to/from_stream.
 * (instead of inline funcs outside class)
 */
struct StreamableOut {};
template <typename T,
          typename = typename std::enable_if<std::is_base_of<StreamableOut, T>::value>::type>
inline QDataStream & operator<< (QDataStream & stream, const T & t) {
	t.to_stream (stream);
	return stream;
}
struct StreamableIn {};
template <typename T,
          typename = typename std::enable_if<std::is_base_of<StreamableIn, T>::value>::type>
inline QDataStream & operator>> (QDataStream & stream, T & t) {
	t.from_stream (stream);
	return stream;
}
struct Streamable : public StreamableOut, public StreamableIn {};

// Same for Debug
struct Debugable {};
template <typename T,
          typename = typename std::enable_if<std::is_base_of<Debugable, T>::value>::type>
inline QDebug operator<< (QDebug debug, const T & t) {
	return t.to_debug (debug);
}

// Utility functions for streams
inline void to_stream (QDataStream &) {}
template <typename Head, typename... Tail>
void to_stream (QDataStream & stream, const Head & h, const Tail &... tail) {
	stream << h;
	to_stream (stream, tail...);
}
inline void from_stream (QDataStream &) {}
template <typename Head, typename... Tail>
void from_stream (QDataStream & stream, Head & h, Tail &... tail) {
	stream >> h;
	from_stream (stream, tail...);
}

// Make std::tuple QDataStream compatible
namespace Impl {
template <typename... Types, std::size_t... I>
void to_stream (QDataStream & stream, const std::tuple<Types...> & tuple, IndexSequence<I...>) {
	to_stream (stream, std::get<I> (tuple)...);
}
template <typename... Types, std::size_t... I>
void from_stream (QDataStream & stream, const std::tuple<Types...> & tuple, IndexSequence<I...>) {
	from_stream (stream, std::get<I> (tuple)...);
}
}
template <typename... Types>
inline QDataStream & operator<< (QDataStream & stream, const std::tuple<Types...> & tuple) {
	Impl::to_stream (stream, tuple, IndexSequenceFor<Types...> ());
	return stream;
}
template <typename... Types>
inline QDataStream & operator>> (QDataStream & stream, const std::tuple<Types...> & tuple) {
	Impl::from_stream (stream, tuple, IndexSequenceFor<Types...> ());
	return stream;
}

/*
 * Peer information
 */
struct Peer {
	QString username;
	QString hostname;
	QHostAddress address;
	quint16 port; // Stored in host byte order
};

/* Print file size with the right suffix.
 */
inline QString size_to_string (qint64 size) {
	double num = size;
	double increment = 1024.0;
	static QString suffixes[] = {QObject::tr ("B"),
	                             QObject::tr ("KiB"),
	                             QObject::tr ("MiB"),
	                             QObject::tr ("GiB"),
	                             QObject::tr ("TiB"),
	                             QObject::tr ("PiB"),
	                             {}};
	int unit_idx = 0;
	while (num >= increment && !suffixes[unit_idx + 1].isEmpty ()) {
		unit_idx++;
		num /= increment;
	}
	return QString ().setNum (num, 'f', 2) + suffixes[unit_idx];
}

#endif
