/*
 * Program wide general declarations
 */
#ifndef H_DECL
#define H_DECL

/*
 * Application name for printing
 */
#define APP_NAME "LocalShare"

/*
 * Network name size limit
 */
#define NAME_SIZE_LIMIT (300)

/*
 * Default tcp port for communication
 * (using avahi to discover it, you can change this)
 */
#define DEFAULT_TCP_PORT 41563

/*
 * Avahi Service name
 * DO NOT change this, this will break interoperability
 */
#define AVAHI_SERVICE_NAME ("_" APP_NAME "._tcp")

#endif

