/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_connect.cpp Basic functions to create connections without blocking.
 */

#include "../../stdafx.h"
#include "../../thread.h"

#include "tcp.h"
#include "../network_internal.h"

#include "../../safeguards.h"

/** List of connections that are currently being created */
static std::vector<TCPConnecter *> _tcp_connecters;

/**
 * Create an empty connecter.
 */
TCPConnecter::TCPConnecter() :
	connected(false),
	aborted(false),
	killed(false),
	sock(INVALID_SOCKET)
{
}

/**
 * Create a new connecter for the given address.
 * @param connection_string The address to connect to.
 * @param default_port If not indicated in connection_string, what port to use.
 */
TCPConnecter::TCPConnecter(const std::string &connection_string, uint16 default_port) :
	connected(false),
	aborted(false),
	killed(false),
	sock(INVALID_SOCKET)
{
	_tcp_connecters.push_back(this);

	this->BootstrapConnect(ParseConnectionString(connection_string, default_port));
}

/**
 * Create a new connecter for the given address with a local bind address.
 * @param connection_string The address to connect to.
 * @param default_port If not indicated in connection_string, what port to use.
 * @param bind_address The local bind address to use.
 */
TCPBindConnecter::TCPBindConnecter(const std::string &connection_string, uint16 default_port, NetworkAddress bind_address) :
	TCPConnecter(),
	bind_address(bind_address)
{
	_tcp_connecters.push_back(this);

	this->BootstrapConnect(ParseConnectionString(connection_string, default_port));
}

/**
 * Bootstrap connecting, picking either a thread or blocking.
 * @param address The NetworkAddress to connect to.
 */
void TCPConnecter::BootstrapConnect(NetworkAddress address)
{
	this->address = address;

	if (!StartNewThread(nullptr, "ottd:tcp", &TCPConnecter::ThreadEntry, this)) {
		this->Connect();
	}
}

/** The actual connection function. */
void TCPConnecter::Connect()
{
	this->sock = this->address.Connect();
	if (this->sock == INVALID_SOCKET) {
		this->aborted = true;
	} else {
		this->connected = true;
	}
}

/** The actual connection function. */
void TCPBindConnecter::Connect()
{
	this->sock = this->address.Connect(this->bind_address);
	if (this->sock == INVALID_SOCKET) {
		this->aborted = true;
	} else {
		this->connected = true;
	}
}

/**
 * Entry point for the new threads.
 * @param param the TCPConnecter instance to call Connect on.
 */
/* static */ void TCPConnecter::ThreadEntry(TCPConnecter *param)
{
	param->Connect();
}

/**
 * Check whether we need to call the callback, i.e. whether we
 * have connected or aborted and call the appropriate callback
 * for that. It's done this way to ease on the locking that
 * would otherwise be needed everywhere.
 */
/* static */ void TCPConnecter::CheckCallbacks()
{
	for (auto iter = _tcp_connecters.begin(); iter < _tcp_connecters.end(); /* nothing */) {
		TCPConnecter *cur = *iter;
		const bool connected = cur->connected.load();
		const bool aborted = cur->aborted.load();
		if ((connected || aborted) && cur->killed) {
			iter = _tcp_connecters.erase(iter);
			if (cur->sock != INVALID_SOCKET) closesocket(cur->sock);
			delete cur;
			continue;
		}
		if (connected) {
			iter = _tcp_connecters.erase(iter);
			cur->OnConnect(cur->sock);
			delete cur;
			continue;
		}
		if (aborted) {
			iter = _tcp_connecters.erase(iter);
			cur->OnFailure();
			delete cur;
			continue;
		}
		iter++;
	}
}

/** Kill all connection attempts. */
/* static */ void TCPConnecter::KillAll()
{
	for (TCPConnecter *conn : _tcp_connecters) conn->killed = true;
}
