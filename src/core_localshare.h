#pragma once
#ifndef CORE_LOCALSHARE_H
#define CORE_LOCALSHARE_H

#ifdef LOCALSHARE_HAS_GUI
#include <QApplication>
#endif

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
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

// Performance parameters
constexpr auto chunk_size = qint64 (10000);
constexpr auto write_buffer_size = qint64 (100000);
constexpr auto max_work_msec = qint64 (100); // maximum time spent out of the event loop

// Transfer notifier parameters
constexpr auto rate_update_interval_msec = qint64 (1000 / 3); // should be bigger than progress
constexpr auto progress_history_window_msec = rate_update_interval_msec;
constexpr auto progress_history_window_elem = 10;
constexpr auto progress_update_interval_msec = qint64 (1000 / 10); // 10 fps max

// Setup app object (graphical and console version)
inline void setup (QCoreApplication & app) {
	app.setApplicationVersion (Const::app_version);
	app.setOrganizationName (Const::app_name);
	app.setApplicationName (Const::app_name);
}
#ifdef LOCALSHARE_HAS_GUI
inline void setup (QApplication & app) {
	setup (static_cast<QCoreApplication &> (app));
	app.setApplicationDisplayName (Const::app_display_name);
}
#endif
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

/* Peer information.
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
	qreal num = size;
	qreal increment = 1024.0;
	static const char * suffixes[] = {QT_TR_NOOP ("B"),
	                                  QT_TR_NOOP ("KiB"),
	                                  QT_TR_NOOP ("MiB"),
	                                  QT_TR_NOOP ("GiB"),
	                                  QT_TR_NOOP ("TiB"),
	                                  QT_TR_NOOP ("PiB"),
	                                  nullptr};
	int unit_idx = 0;
	while (num >= increment && suffixes[unit_idx + 1] != nullptr) {
		unit_idx++;
		num /= increment;
	}
	return QString ().setNum (num, 'f', 2) + qApp->translate ("size_to_string", suffixes[unit_idx]);
}

#endif
