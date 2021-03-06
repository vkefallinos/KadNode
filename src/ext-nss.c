
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>

#include "main.h"
#include "conf.h"
#include "log.h"
#include "utils.h"
#include "kad.h"
#include "net.h"
#include "ext-nss.h"


void nss_lookup( int sock, IP *clientaddr, const char *hostname ) {
	char addrbuf[FULL_ADDSTRLEN+1];
	socklen_t addrlen;
	IP addrs[8];
	size_t num;

	/* Return at most 8 addresses */
	num = 8;

	/* Lookup id. Starts search when not already started. */
	if( kad_lookup_value( hostname, addrs, &num ) >= 0 && num > 0 ) {
		/* Found addresses */
		log_debug( "NSS: Send %lu addresses to %s. Packet has %d bytes.",
		   num, str_addr( clientaddr, addrbuf ), sizeof(IP)
		);

		addrlen = addr_len( clientaddr );
		sendto( sock, (UCHAR *) addrs, num * sizeof(IP), 0, (const struct sockaddr *) clientaddr, addrlen );
	}
}

/*
* Handle a local connection
*/
void nss_handler( int rc, int sock ) {
	IP clientaddr;
	socklen_t addrlen_ret;
	char hostname[512];

	if( rc == 0 ) {
		return;
	}

	addrlen_ret = sizeof(IP);
	rc = recvfrom( sock, hostname, sizeof(hostname), 0, (struct sockaddr *) &clientaddr, &addrlen_ret );

	if( rc <= 0 || rc >= sizeof(hostname) ) {
		return;
	}

	/* Add missing null terminator */
	hostname[rc] = '\0';

	if( !is_suffix( hostname, QUERY_OMIT_SUFFIX ) ) {
		return;
	}

	/* Validate hostname */
	if( !str_isValidHostname( hostname ) ) {
		log_warn( "NSS: Invalid hostname for lookup: '%s'", hostname );
		return;
	}

	nss_lookup( sock, &clientaddr, hostname );
}

void nss_setup( void ) {
	int sock;

	if( str_isZero( gconf->nss_port ) ) {
		return;
	}

	sock = net_bind( "NSS", "::1", gconf->nss_port, NULL, IPPROTO_UDP, AF_INET6 );
	net_add_handler( sock, &nss_handler );
}
