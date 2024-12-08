/*
 * \brief  Simple UDP test server
 * \author Martin Stein
 * \date   2017-10-18
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>
#include <nic/packet_allocator.h>
#include <util/string.h>

/* libc includes */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace Genode;
using Ipv4_addr_str = Genode::String<16>;

struct Socket_failed     : Genode::Exception { };
struct Send_failed       : Genode::Exception { };
struct Receive_failed    : Genode::Exception { };
struct Bind_failed       : Genode::Exception { };
struct Listen_failed 	 : Genode::Exception { };
struct Accept_failed 	 : Genode::Exception { };
struct Init_failed 		 : Genode::Exception { };
struct Select_failed 	 : Genode::Exception { };


#define WHOAMI "100 IPBENCH V1.0\n"
#define HELLO "HELLO\n"
#define OK_READY "200 OK (Ready to go)\n"
#define LOAD "LOAD cpu_target_lukem\n"
#define OK "200 OK\n"
#define SETUP "SETUP args::\"\"\n"
#define START "START\n"
#define STOP "STOP\n"
#define RESPONSE1 "220 VALID DATA (Data to follow)\n"
#define RESPONSE2 "Content-length: 4\n"
#define RESPONSE3 ",0,0"
#define QUIT "QUIT\n"

static bool initiate_benchmark(int conn)
{
	char    buf[1024];
	ssize_t buflen;

	/* -> "100 IPBENCH V1.0\n" */
	send(conn, WHOAMI, Genode::strlen(WHOAMI), 0);

	/* <- "HELLO\n" */
	buflen = recv(conn, buf, 1024, 0);
	if (Genode::strcmp(buf, HELLO, strlen(HELLO))) {
		return false;
	}

	/* -> "200 OK (Ready to go)\n" */
	send(conn, OK_READY, Genode::strlen(OK_READY), 0);

	/* <- "LOAD cpu_target_lukem\n" */
	buflen = recv(conn, buf, 1024, 0);
	if (Genode::strcmp(buf, LOAD, strlen(LOAD))) {
		return false;
	}

	/* -> "200 OK\n" */
	send(conn, OK, Genode::strlen(OK), 0);

	/* <- "SETUP args::\"\"\n" */
	buflen = recv(conn, buf, 1024, 0);
	if (Genode::strcmp(buf, SETUP, strlen(SETUP))) {
		return false;
	}

	/* -> "200 OK\n" */
	send(conn, OK, Genode::strlen(OK), 0);

	/* <- "START\n" */
	buflen = recv(conn, buf, 1024, 0);
	if (Genode::strcmp(buf, START, strlen(START))) {
		return false;
	}

	return true;
}


static void test(Libc::Env & env)
{
	/*
	 * Set up the control socket
	 */

	/* Set up the control socket */
	int control_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (control_socket < 0) {
		throw Socket_failed();
	}
	unsigned control_port = 1236;

	/* Create server control socket address */
	struct sockaddr_in control_in_addr;
	control_in_addr.sin_family = AF_INET;
	control_in_addr.sin_port = htons(control_port);
	control_in_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(control_socket, (struct sockaddr *)&control_in_addr, sizeof(control_in_addr))) {
		throw Bind_failed();
	}

	/*
	 * Set up the benchmarking socket
	 */

	/* create benchmark socket */
	int s = socket(AF_INET, SOCK_DGRAM, 0 );
	if (s < 0) {
		throw Socket_failed();
	}
	/* read server port */
	unsigned port = 0;
	env.config([&] (Xml_node const &config_node) {
		port = config_node.attribute_value("port", 1235u); });

	/* create server socket address */
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = INADDR_ANY;

	/* bind server socket address to socket */
	if (bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		throw Bind_failed();
	}

	/*
	 * Wait for the control socket to be invoked
	 */

	/* prepare client socket address */
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	socklen_t addr_sz = sizeof(addr);

	/* Initiate the connections sequence */
	if (listen(control_socket, 5)) {
		throw Listen_failed();
	}

	int client = accept(control_socket, (struct sockaddr*) &addr, &addr_sz);
	if (client < 0) {
		throw Accept_failed();
	}

	if (!initiate_benchmark(client)) {
		throw Init_failed();
	}

	log("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA initialization success\n");

	/*
	 * Initiate benchmark
	 */

	fd_set read_sock_set;
	FD_ZERO(&read_sock_set);
	FD_SET(client, &read_sock_set);
	FD_SET(s, &read_sock_set);

	int i = 0;

	char buf[4096];
	while (true) {
		int retval = select(client + 1, &read_sock_set, 0, 0, 0);
		if (retval == -1) {
			log("AAAAAA what went wrong?!");
			throw Select_failed();
		}

		//::memset(buf, 0, sizeof(buf));
		if (FD_ISSET(s, &read_sock_set)) {
			// log("Received data socket");
			/* receive and send back one message without any modifications */
			ssize_t rcv_sz = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addr_sz);
			if (rcv_sz < 0) {
				throw Receive_failed();
			}
			if (sendto(s, buf, rcv_sz, 0, (struct sockaddr*)&addr, addr_sz) != rcv_sz) {
				throw Send_failed();
			}
			// i++;

			// if (i % 1000 == 0) {
				// log("Just got packet ", i);
			// }
		}

		if (FD_ISSET(client, &read_sock_set)) {
			log("Received control socket");

			/* <- STOP */
			ssize_t rcv_sz = recvfrom(client, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addr_sz);
			if (rcv_sz < 0) {
				throw Receive_failed();
			}
			if (Genode::strcmp(buf, STOP, strlen(STOP))) {
				throw Receive_failed();
			}

			/* -> "220 VALID DATA (Data to follow)\nContent-length: 4\n,0,0" */
			send(client, RESPONSE1, Genode::strlen(RESPONSE1), 0);
			send(client, RESPONSE2, Genode::strlen(RESPONSE2), 0);
			send(client, RESPONSE3, Genode::strlen(RESPONSE3), 0);

			/* <- QUIT */
			rcv_sz = recvfrom(client, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addr_sz);
			if (rcv_sz < 0) {
				throw Receive_failed();
			}
			if (Genode::strcmp(buf, QUIT, strlen(QUIT))) {
				throw Receive_failed();
			}

			close(client);

			break;
		}

		FD_SET(client, &read_sock_set);
		FD_SET(s, &read_sock_set);
	}

	close(s);
	close(client);

	log("Benchmarking done!\n");
}

void Libc::Component::construct(Libc::Env &env) { with_libc([&] () { test(env); }); }
