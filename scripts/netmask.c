/*
 * netmask.c
 * Copyright (C) 2000  Paul Davis, pdavis@lpccomp.bc.ca
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * This program takes an IP address and netmask from the command line
 * and shows the netmask, netmask bits, network address, broadcast address,
 * host IP range within the subnet and the number of such hosts.
 *
 * Usage: netmask IP-address netmask-IP
 *     or netmask IP-address netmask-bits
 */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

char *my_ntoa(unsigned long ip);

int main(int argc, char *argv[])
{
	struct in_addr addr,netmask;
	unsigned long mask,network,hostmask,broadcast;
	int maskbits;
	int i;

	if ( argc != 3 ) {
		printf("Usage: %s ipaddr netmask\nThe netmask can be an ip address or the number of bits\n",argv[0]);
		exit(1);
	}

	/* Convert the string ip address to network order address */
	if ( ! inet_aton(argv[1],&addr) ) {
		fprintf(stderr,"%s is not a valid IP address\n",argv[1]);
		exit(1);
	}

	/* If the netmask has a dot, assume it is the netmask as an ip address*/
	if ( strchr(argv[2],'.' ) ) {
		if ( ! inet_aton(argv[2],&netmask) ) {
			fprintf(stderr,"%s is not a valid IP netmask\n",argv[2]);
			exit(1);
		}
		/* Calculate the number of network bits */
		mask = ntohl(netmask.s_addr);
		for ( maskbits=32 ; (mask & (1L<<(32-maskbits))) == 0 ; maskbits-- )
			;
		/* Re-create the netmask and compare to the origianl
		 * to make sure it is a valid netmask.
		 */
		mask = 0;
		for ( i=0 ; i<maskbits ; i++ )
			mask |= 1<<(31-i);
	}
	else {
		maskbits = atoi(argv[2]);
		if ( maskbits < 1 || maskbits > 30 ) {
			fprintf(stderr,"Invalid net mask bits (1-30): %d\n",maskbits);
			exit(1);
		}
		/* Create the netmask from the number of bits */
		mask = 0;
		for ( i=0 ; i<maskbits ; i++ )
			mask |= 1<<(31-i);
		netmask.s_addr = htonl(mask);
	}

	network = ntohl(addr.s_addr) & ntohl(netmask.s_addr);
	hostmask = ~ntohl(netmask.s_addr);
	broadcast = network | hostmask;
	printf("%s\n",my_ntoa(broadcast));
	return 0;
}

/* Convert the given ip address in native byte order to a printable string */
char *
my_ntoa(unsigned long ip) {
	struct in_addr addr;
	addr.s_addr = htonl(ip);
	return inet_ntoa(addr);
}


