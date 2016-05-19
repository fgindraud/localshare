#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <QHostInfo>
#include <QSocketNotifier>
#include <QString>
#include <QTime>
#include <QtEndian>

#include <dns_sd.h>
#include <utility> // std::forward

#include "compatibility.h"
#include "localshare.h"
#include "settings.h"

namespace Discovery {

/* QObject representing a discovered peer.
 * As Bonjour does not provide Ip resolution itself, ip is not contained.
 * These objects will be owned by the browser.
 * On Browser failure, they will be destroyed.
 *
 * name VS username:
 * - name is the service raw name.
 *   It includes the machine name to allow same usernames on the network.
 *   It must be unique.
 * - username is the user name on the network.
 *   It is used by the gui, but the discovery doesn't care.
 *
 * The local_peer is used to represent the local machine.
 * It allows to prevent showing our own instance of localshare.
 */
class DnsPeer : public QObject {
	Q_OBJECT

private:
	QString name;
	QString hostname;
	quint16 port; // Host byte order

signals:
	void name_changed (void);

public:
	DnsPeer (QObject * parent = nullptr) : QObject (parent) {}

	QString get_name (void) const { return name; }
	void set_name (const QString & new_name) {
		name = new_name;
		emit name_changed ();
	}
	QString get_hostname (void) const { return hostname; }
	void set_hostname (const QString & new_hostname) { hostname = new_hostname; }
	quint16 get_port (void) const { return port; }
	void set_port (quint16 new_port) { port = new_port; }

	QString get_username (void) const {
		auto s = get_name ().section ('@', 0, -2);
		if (!s.isEmpty ())
			return s;
		return get_name ();
	}

protected:
	void set_username (const QString & username, const QString & suffix) {
		set_name (QString ("%1@%2").arg (username, suffix));
	}
};

class LocalDnsPeer : public DnsPeer {
private:
	QString suffix;

public:
	LocalDnsPeer (quint16 server_port, QObject * parent = nullptr)
	    : DnsPeer (parent), suffix (QHostInfo::localHostName ()) {
		if (suffix.isEmpty ()) {
			// Random suffix
			qsrand (QTime::currentTime ().msec ());
			suffix.setNum (qrand ());
		}
		set_username (Settings::Username ().get (), suffix);
		set_port (server_port);
	}
};

/* Helper class to manage a DNSServiceRef.
 * On error, the class emits the error and calls deleteLater as it is supposedly broken.
 * This class must only be created with new DnsSocket ()...
 */
class DnsSocket : public QObject {
	Q_OBJECT

private:
	DNSServiceRef ref{nullptr};

signals:
	void error (QString);

public:
	DnsSocket (QObject * parent = nullptr) : QObject (parent) {}
	~DnsSocket () { DNSServiceRefDeallocate (ref); }

protected:
	template <typename QueryFunc, typename... Args>
	void init_with (QueryFunc && query_func, Args &&... args) {
		auto err = std::forward<QueryFunc> (query_func) (&ref, std::forward<Args> (args)...);
		if (has_error (err)) {
			failure (err);
			return;
		}
		auto fd = DNSServiceRefSockFD (ref);
		if (fd != -1) {
			auto notifier = new QSocketNotifier (fd, QSocketNotifier::Read, this);
			connect (notifier, &QSocketNotifier::activated, this, &DnsSocket::has_pending_data);
		} else {
			// Should never happen, the function is just an accessor
			qFatal ("DNSServiceRefSockFD failed");
		}
	}

	// Error handling
	using Error = DNSServiceErrorType;
	static bool has_error (Error e) { return e != kDNSServiceErr_NoError; }

	void failure (Error e) {
		emit error (error_string (e));
		deleteLater ();
	}

	virtual QString error_string (Error e) {
		switch (e) {
		case kDNSServiceErr_NoError:
			return tr ("No error");
		// TODO more, and add in subclasses
		default:
			return tr ("Unknown error");
		}
	}

private slots:
	void has_pending_data (void) {
		auto err = DNSServiceProcessResult (ref);
		if (has_error (err))
			failure (err);
	}
};

/* Service announce class
 *
 * Registers at construction, and unregister at destruction.
 * Name of service is limited to kDNSServiceMaxServiceName utf8 bytes (64B including '\0'
 * currently).
 * Longer names will be truncated by the library, so do nothing about that.
 * The actual registered name will be provided by the callback and given to the app through the
 * registered signal.
 */
class Service : public DnsSocket {
	Q_OBJECT

signals:
	void registered (QString name);

public:
	Service (const DnsPeer * local_peer, QObject * parent = nullptr) : DnsSocket (parent) {
		init_with (DNSServiceRegister, 0 /* flags */, 0 /* any interface */,
		           qUtf8Printable (local_peer->get_name ()), qUtf8Printable (Const::service_type),
		           nullptr /* default domain */, nullptr /* default hostname */,
		           qToBigEndian (local_peer->get_port ()) /* port in NBO */, 0,
		           nullptr /* text len and text */, register_callback, this /* context */);
	}

private:
	static void DNSSD_API register_callback (DNSServiceRef, DNSServiceFlags,
	                                         DNSServiceErrorType error_code, const char * name,
	                                         const char * /* regtype */, const char * /* domain */,
	                                         void * context) {
		auto c = static_cast<Service *> (context);
		if (has_error (error_code)) {
			c->failure (error_code);
			return;
		}
		emit c->registered (name);
	}
};

/* Temporary structure use to represent a resolve query.
 * Resolve query : from DNS_SD service id to hostname, port.
 *
 * A DnsPeer object is created empty, and owned by the Query.
 * When finally filled, it is published.
 * The browser should receive the reference and get ownership of the DnsPeer.
 * If the query fails or succeeds, it then deletes itself.
 * If the DnsPeer was not taken by the Browser, it will be deleted too.
 */
class Resolver : public DnsSocket {
	Q_OBJECT

private:
	DnsPeer * peer;

signals:
	void peer_resolved (DnsPeer *);

public:
	Resolver (uint32_t interface_index, const char * name, const char * regtype, const char * domain,
	          QObject * parent = nullptr)
	    : DnsSocket (parent) {
		peer = new DnsPeer (this);
		peer->set_name (name);
		init_with (DNSServiceResolve, 0 /* flags */, interface_index, name, regtype, domain,
		           resolver_callback, this /* context */);
	}

private:
	static void DNSSD_API resolver_callback (DNSServiceRef, DNSServiceFlags, uint32_t /* interface */,
	                                         DNSServiceErrorType error_code,
	                                         const char * /* fullname */, const char * hostname,
	                                         uint16_t port, uint16_t /* txt len */,
	                                         const unsigned char * /* txt record */, void * context) {
		auto c = static_cast<Resolver *> (context);
		if (has_error (error_code)) {
			c->failure (error_code);
			return;
		}
		c->peer->set_port (qFromBigEndian (port)); // To host byte order
		c->peer->set_hostname (hostname);
		emit c->peer_resolved (c->peer);
		c->deleteLater ();
	}
};

/* Browser object.
 * Starts browsing at creation, stops at destruction.
 * Emits added/removed signals to indicate changes to peer list.
 *
 * It owns DnsPeer objects representing discovered peers.
 * DnsPeer objects will be destroyed when the peer disappears.
 * They are all destroyed when the Browser dies.
 */
class Browser : public DnsSocket {
	Q_OBJECT

private:
	const LocalDnsPeer * local_peer;

signals:
	void added (DnsPeer *);

public:
	Browser (const LocalDnsPeer * local_peer, QObject * parent = nullptr)
	    : DnsSocket (parent), local_peer (local_peer) {
		init_with (DNSServiceBrowse, 0 /* flags */, 0 /* interface */,
		           qUtf8Printable (Const::service_type), nullptr /* default domain */, browser_callback,
		           this /* context */);
	}

private:
	static void DNSSD_API browser_callback (DNSServiceRef, DNSServiceFlags flags,
	                                        uint32_t interface_index, DNSServiceErrorType error_code,
	                                        const char * name, const char * regtype,
	                                        const char * domain, void * context) {
		auto c = static_cast<Browser *> (context);
		if (has_error (error_code)) {
			c->failure (error_code);
			return;
		}
		if (flags & kDNSServiceFlagsAdd) {
			// Peer is added, find its contact info
			auto r = new Resolver (interface_index, name, regtype, domain, c);
			connect (r, &Resolver::peer_resolved, c, &Browser::peer_resolved);
			connect (r, &Resolver::error,
			         [](const QString & msg) { qWarning () << "Browse: resolve aborted:" << msg; });
		} else {
			// Peer is removed
			c->delete_peer_by_name (name);
		}
	}

private slots:
	void peer_resolved (DnsPeer * peer) {
		if (local_peer->get_name () != peer->get_name ()) {
			peer->setParent (this); // Take ownership
			emit added (peer);
			// TODO find if already exist and update ?
		}
	}

private:
	void delete_peer_by_name (const QString & name) {
		for (auto dns_peer : findChildren<DnsPeer *> (QString (), Qt::FindDirectChildrenOnly)) {
			if (dns_peer->get_name () == name)
				dns_peer->deleteLater ();
		}
	}
};
}

#endif
