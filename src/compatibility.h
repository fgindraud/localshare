#ifndef COMPATIBILITY_H
#define COMPATIBILITY_H

// Contains missing stuff in older Qt versions up to 5.2

#include <QList>
#include <QString>
#include <QtGlobal>
#include <iterator>

#if (QT_VERSION < QT_VERSION_CHECK(5, 4, 0))
// No qUtf8Printable
inline const char * qUtf8Printable (const QString & str) {
	return str.toUtf8 ().constData ();
}
#endif

#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
// No reverse iterators in qt
template <typename T>
using ConstReverseIteratorForQList = std::reverse_iterator<typename QList<T>::const_iterator>;

template <typename T> inline ConstReverseIteratorForQList<T> rbegin (const QList<T> & list) {
	return ConstReverseIteratorForQList<T> (list.end ());
}
template <typename T> inline ConstReverseIteratorForQList<T> rend (const QList<T> & list) {
	return ConstReverseIteratorForQList<T> (list.begin ());
}
#elif (__cplusplus < 201402L)
// No std::rbegin/std::rend
template <typename T>
using ConstReverseIteratorForQList = typename QList<T>::const_reverse_iterator;

template <typename T> inline ConstReverseIteratorForQList<T> rbegin (const QList<T> & list) {
	return list.rbegin ();
}
template <typename T> inline ConstReverseIteratorForQList<T> rend (const QList<T> & list) {
	return list.rend ();
}
#else
using std::rbegin;
using std::rend;
#endif

#endif
