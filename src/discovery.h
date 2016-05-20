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
 * The peer information (hostname, port) can change and has notify signals.
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
	void hostname_changed (void);
	void port_changed (void);

public:
	DnsPeer (QObject * parent = nullptr) : QObject (parent) {}

	QString get_name (void) const { return name; }
	void set_name (const QString & new_name) { name = new_name; }
	QString get_hostname (void) const { return hostname; }
	void set_hostname (const QString & new_hostname) {
		if (hostname != new_hostname) {
			hostname = new_hostname;
			emit hostname_changed ();
		}
	}
	quint16 get_port (void) const { return port; }
	void set_port (quint16 new_port) {
		if (port != new_port) {
			port = new_port;
			emit port_changed ();
		}
	}

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

/* LocalDnsPeer represents the local instance of localshare.
 * Its name is initialized to the proposed service instance name.
 * It CAN change during the registration, so we provide a name_changed signal.
 */
class LocalDnsPeer : public DnsPeer {
	Q_OBJECT

signals:
	void name_changed (void);

public:
	LocalDnsPeer (quint16 server_port, QObject * parent = nullptr) : DnsPeer (parent) {
		QString suffix{QHostInfo::localHostName ()};
		if (suffix.isEmpty ()) {
			// Random suffix
			qsrand (QTime::currentTime ().msec ());
			suffix.setNum (qrand ());
		}
		set_username (Settings::Username ().get (), suffix);
		set_port (server_port);
	}

	void set_name (const QString & new_name) {
		if (get_name () != new_name) {
			DnsPeer::set_name (new_name);
			emit name_changed ();
		}
	}
};

/* Helper class to manage a DNSServiceRef.
 * This class must be dynamically allocated.
 */
class DnsSocket : public QObject {
	Q_OBJECT

private:
	DNSServiceRef ref{nullptr};
	QString error_msg;

signals:
	/* On delete this class will send this signal.
	 * When sent, it is guaranteed that the service has been closed.
	 * The string "error" may be empty if it is a normal termination.
	 */
	void being_destroyed (QString error);

public:
	DnsSocket (QObject * parent = nullptr) : QObject (parent) {}
	~DnsSocket () {
		DNSServiceRefDeallocate (ref);
		emit being_destroyed (error_msg);
	}

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
		Q_ASSERT (error_msg.isEmpty ()); // Should only be called once
		error_msg = make_error_string (e);
		deleteLater ();
	}

	virtual QString make_error_string (Error e) const {
		switch (e) {
		case kDNSServiceErr_NoError:
			return tr ("No error");
		case kDNSServiceErr_Unknown:
			return tr ("Unknown error");
		case kDNSServiceErr_NoSuchName:
			return tr ("Internal error: No such name");
		case kDNSServiceErr_NoMemory:
			return tr ("Out of memory");
		case kDNSServiceErr_BadParam:
			return tr ("API error: Bad parameter");
		case kDNSServiceErr_BadReference:
			return tr ("API error: Bad DNSServiceRef");
		case kDNSServiceErr_BadState:
			return tr ("Internal error: Bad state");
		case kDNSServiceErr_BadFlags:
			return tr ("API error: Bad flags");
		case kDNSServiceErr_Unsupported:
			return tr ("Unsupported operation");
		case kDNSServiceErr_NotInitialized:
			return tr ("API error: DNSServiceRef is not initialized");
		case kDNSServiceErr_AlreadyRegistered:
			return tr ("Service is already registered");
		case kDNSServiceErr_NameConflict:
			return tr ("Service name is already taken");
		case kDNSServiceErr_Invalid:
			return tr ("API error: Invalid data");
		case kDNSServiceErr_Firewall:
			return tr ("Firewall");
		case kDNSServiceErr_Incompatible:
			return tr ("Localshare incompatible with local Bonjour service");
		case kDNSServiceErr_BadInterfaceIndex:
			return tr ("API error: Bad interface index");
		case kDNSServiceErr_Refused:
			return tr ("kDNSServiceErr_Refused");
		case kDNSServiceErr_NoSuchRecord:
			return tr ("kDNSServiceErr_NoSuchRecord");
		case kDNSServiceErr_NoAuth:
			return tr ("kDNSServiceErr_NoAuth");
		case kDNSServiceErr_NoSuchKey:
			return tr ("The key does not exist in the TXT record");
		case kDNSServiceErr_NATTraversal:
			return tr ("kDNSServiceErr_NATTraversal");
		case kDNSServiceErr_DoubleNAT:
			return tr ("kDNSServiceErr_DoubleNAT");
		case kDNSServiceErr_BadTime:
			return tr ("kDNSServiceErr_BadTime");
		case -65563: // kDNSServiceErr_ServiceNotRunning, only in recent versions
			return tr ("Bonjour service in not running");
		default:
			return tr ("Unknown error code: %1").arg (e);
		}
	}

private slots:
	void has_pending_data (void) {
		auto err = DNSServiceProcessResult (ref);
		if (has_error (err))
			failure (err);
	}
};

/* Service registration class.
 *
 * Registers at construction, and unregister at destruction.
 * Name of service is limited to kDNSServiceMaxServiceName utf8 bytes.
 * Longer names will be truncated by the library, so do nothing about that.
 * The actual registered name will be provided by the callback.
 * It will be set in the local_peer, which will notify others if needed.
 */
class Service : public DnsSocket {
	Q_OBJECT

private:
	bool has_registered{false};

signals:
	void registered (void);

public:
	Service (LocalDnsPeer * local_peer) : DnsSocket (local_peer) {
		init_with (DNSServiceRegister, 0 /* flags */, 0 /* any interface */,
		           qUtf8Printable (local_peer->get_name ()), qUtf8Printable (Const::service_type),
		           nullptr /* default domain */, nullptr /* default hostname */,
		           qToBigEndian (local_peer->get_port ()) /* port in NBO */, 0,
		           nullptr /* text len and text */, register_callback, this /* context */);
	}

	bool is_registered (void) const { return has_registered; }

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
		c->get_local_peer ()->set_name (name);
		c->has_registered = true;
		emit c->registered ();
	}

	QString make_error_string (Error e) const Q_DECL_OVERRIDE {
		return tr ("Service registration failed: %1").arg (DnsSocket::make_error_string (e));
	}

	LocalDnsPeer * get_local_peer (void) {
		auto p = qobject_cast<LocalDnsPeer *> (parent ());
		Q_ASSERT (p);
		return p;
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

	QString make_error_string (Error e) const Q_DECL_OVERRIDE {
		return tr ("Service resolver failed: %1").arg (DnsSocket::make_error_string (e));
	}
};

/* Browser object.
 * Starts browsing at creation, stops at destruction.
 * Emits added/removed signals to indicate changes to peer list.
 *
 * It owns DnsPeer objects representing discovered peers.
 * DnsPeer objects will be destroyed when the peer disappears.
 * They are all destroyed when the Browser dies.
 *
 * The local_peer username might be changed ; currently nothing reacts to it.
 * The filtering test will use the new name without care.
 */
class Browser : public DnsSocket {
	Q_OBJECT

signals:
	void added (DnsPeer *);

public:
	Browser (LocalDnsPeer * local_peer) : DnsSocket (local_peer) {
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
			connect (r, &Resolver::being_destroyed, [](const QString & msg) {
				if (!msg.isEmpty ())
					qWarning () << "Browser: resolve failed:" << msg;
			});
		} else {
			// Peer is removed
			auto p = c->find_peer_by_name (name);
			if (p)
				p->deleteLater ();
		}
	}

private slots:
	void peer_resolved (DnsPeer * peer) {
		auto p = find_peer_by_name (peer->get_name ());
		if (p) {
			// Update, and let peer be discarded
			p->set_hostname (peer->get_hostname ());
			p->set_port (peer->get_port ());
		} else {
			// Add and take ownership
			if (get_local_peer ()->get_name () != peer->get_name ()) {
				peer->setParent (this);
				emit added (peer);
			}
		}
	}

private:
	DnsPeer * find_peer_by_name (const QString & name) {
		for (auto dns_peer : findChildren<DnsPeer *> (QString (), Qt::FindDirectChildrenOnly))
			if (dns_peer->get_name () == name)
				return dns_peer;
		return nullptr;
	}

	QString make_error_string (Error e) const Q_DECL_OVERRIDE {
		return tr ("Service browser failed: %1").arg (DnsSocket::make_error_string (e));
	}

	LocalDnsPeer * get_local_peer (void) {
		auto p = qobject_cast<LocalDnsPeer *> (parent ());
		Q_ASSERT (p);
		return p;
	}
};
}

#endif
