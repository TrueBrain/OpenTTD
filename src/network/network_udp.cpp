/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file network_udp.cpp This file handles the UDP related communication.
 *
 * This is the GameServer <-> GameClient
 * communication before the game is being joined.
 */

#include "../stdafx.h"
#include "../date_func.h"
#include "../map_func.h"
#include "../debug.h"
#include "network_gamelist.h"
#include "network_internal.h"
#include "network_udp.h"
#include "network.h"
#include "../core/endian_func.hpp"
#include "../company_base.h"
#include "../thread.h"
#include "../rev.h"
#include "../strings_func.h"
#include "table/strings.h"
#include <mutex>

#include "core/udp.h"

#include "../safeguards.h"

static bool _network_udp_server;         ///< Is the UDP server started?
static uint16 _network_udp_broadcast;    ///< Timeout for the UDP broadcasts.

/** Some information about a socket, which exists before the actual socket has been created to provide locking and the likes. */
struct UDPSocket {
	const std::string name;                     ///< The name of the socket.
	std::mutex mutex;                           ///< Mutex for everything that (indirectly) touches the sockets within the handler.
	NetworkUDPSocketHandler *socket;            ///< The actual socket, which may be nullptr when not initialized yet.
	std::atomic<int> receive_iterations_locked; ///< The number of receive iterations the mutex was locked.

	UDPSocket(const std::string &name_) : name(name_), socket(nullptr) {}

	void Close()
	{
		std::lock_guard<std::mutex> lock(mutex);
		socket->Close();
		delete socket;
		socket = nullptr;
	}

	void ReceivePackets()
	{
		std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
		if (!lock.try_lock()) {
			if (++receive_iterations_locked % 32 == 0) {
				DEBUG(net, 0, "[udp] %s background UDP loop processing appears to be blocked. Your OS may be low on UDP send buffers.", name.c_str());
			}
			return;
		}

		receive_iterations_locked.store(0);
		socket->ReceivePackets();
	}
};

static UDPSocket _udp_client("Client"); ///< udp client socket
static UDPSocket _udp_server("Server"); ///< udp server socket

///*** Communication with clients (we are server) ***/

/** Helper class for handling all server side communication. */
class ServerNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	void Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr) override;
public:
	/**
	 * Create the socket.
	 * @param addresses The addresses to bind on.
	 */
	ServerNetworkUDPSocketHandler(NetworkAddressList *addresses) : NetworkUDPSocketHandler(addresses) {}
	virtual ~ServerNetworkUDPSocketHandler() {}
};

void ServerNetworkUDPSocketHandler::Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr)
{
	/* Just a fail-safe.. should never happen */
	if (!_network_udp_server) {
		return;
	}

	NetworkGameInfo ngi;
	FillNetworkGameInfo(ngi);

	Packet packet(PACKET_UDP_SERVER_RESPONSE);
	SendNetworkGameInfo(&packet, &ngi);

	/* Let the client know that we are here */
	this->SendPacket(&packet, client_addr);

	DEBUG(net, 2, "[udp] queried from %s", client_addr->GetHostname());
}

///*** Communication with servers (we are client) ***/

/** Helper class for handling all client side communication. */
class ClientNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	void Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr) override;
public:
	virtual ~ClientNetworkUDPSocketHandler() {}
};

void ClientNetworkUDPSocketHandler::Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr)
{
	NetworkGameList *item;

	/* Just a fail-safe.. should never happen */
	if (_network_udp_server) return;

	DEBUG(net, 4, "[udp] server response from %s", client_addr->GetAddressAsString().c_str());

	/* Find next item */
	item = NetworkGameListAddItem(ServerAddress(*client_addr));

	ClearGRFConfigList(&item->info.grfconfig);
	ReceiveNetworkGameInfo(p, &item->info);

	CheckGameCompatability(item);

	if (client_addr->GetAddress()->ss_family == AF_INET6) {
		strecat(item->info.server_name, " (IPv6)", lastof(item->info.server_name));
	}

	UpdateNetworkGameWindow();
}

/** Broadcast to all ips */
static void NetworkUDPBroadCast(NetworkUDPSocketHandler *socket)
{
	for (NetworkAddress &addr : _broadcast_list) {
		Packet p(PACKET_UDP_CLIENT_FIND_SERVER);

		DEBUG(net, 4, "[udp] broadcasting to %s", addr.GetHostname());

		socket->SendPacket(&p, &addr, true, true);
	}
}

/** Find all servers */
void NetworkUDPSearchGame()
{
	/* We are still searching.. */
	if (_network_udp_broadcast > 0) return;

	DEBUG(net, 0, "[udp] searching server");

	NetworkUDPBroadCast(_udp_client.socket);
	_network_udp_broadcast = 300; // Stay searching for 300 ticks
}

/** Initialize the whole UDP bit. */
void NetworkUDPInitialize()
{
	/* If not closed, then do it. */
	if (_udp_server.socket != nullptr) NetworkUDPClose();

	DEBUG(net, 1, "[udp] initializing listeners");
	assert(_udp_client.socket == nullptr && _udp_server.socket == nullptr);

	std::scoped_lock lock(_udp_client.mutex, _udp_server.mutex);

	_udp_client.socket = new ClientNetworkUDPSocketHandler();

	NetworkAddressList server;
	GetBindAddresses(&server, _settings_client.network.server_port);
	_udp_server.socket = new ServerNetworkUDPSocketHandler(&server);

	server.clear();
	GetBindAddresses(&server, 0);

	_network_udp_server = false;
	_network_udp_broadcast = 0;
}

/** Start the listening of the UDP server component. */
void NetworkUDPServerListen()
{
	std::lock_guard<std::mutex> lock(_udp_server.mutex);
	_network_udp_server = _udp_server.socket->Listen();
}

/** Close all UDP related stuff. */
void NetworkUDPClose()
{
	_udp_client.Close();
	_udp_server.Close();

	_network_udp_server = false;
	_network_udp_broadcast = 0;
	DEBUG(net, 1, "[udp] closed listeners");
}

/** Receive the UDP packets. */
void NetworkBackgroundUDPLoop()
{
	if (_network_udp_server) {
		_udp_server.ReceivePackets();
	} else {
		_udp_client.ReceivePackets();
		if (_network_udp_broadcast > 0) _network_udp_broadcast--;
	}
}
