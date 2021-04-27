/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_stun.cpp STUN sending/receiving part of the network protocol. */

#include "../stdafx.h"
#include "../debug.h"
#include "network.h"
#include "network_stun.h"

#include "../safeguards.h"

/** Connect to the STUN server. */
class NetworkStunConnecter : TCPConnecter {
private:
	ClientNetworkStunSocketHandler *stun_handler;

public:
	/**
	 * Initiate the connecting.
	 * @param stun_handler The handler for this request.
	 * @param connection_string The address of the server.
	 */
	NetworkStunConnecter(ClientNetworkStunSocketHandler *stun_handler, const std::string &connection_string) : TCPConnecter(connection_string, NETWORK_STUN_SERVER_PORT), stun_handler(stun_handler) {}

	void OnFailure() override
	{
		/* Connection to STUN server failed. For example, the client doesn't
		 * support IPv6, which means it will fail that attempt. */

		this->stun_handler->CloseConnection(true);
	}

	void OnConnect(SOCKET s) override
	{
		assert(this->stun_handler->sock == INVALID_SOCKET);
		this->stun_handler->sock = s;

		/* Store the local address; later connects will reuse it again.
		 * This is what makes STUN work for most NATs. */
		sockaddr_storage address = {};
		socklen_t len = sizeof(address);
		getsockname(s, (sockaddr *)&address, &len);
		this->stun_handler->local_addr = NetworkAddress(address, len);

		/* We leave the connection open till the real connection is setup later. */
	}
};

/**
 * Connect to the STUN server over either IPv4 or IPv6.
 * @param family What IP family to use.
 */
void ClientNetworkStunSocketHandler::Connect(int family)
{
	// TODO -- Support family
	new NetworkStunConnecter(this, NETWORK_STUN_SERVER_HOST);
}

/**
 * Send a STUN packet to the STUN server.
 * @param token The token as received from the Game Coordinator.
 * @param family What IP family this STUN request is for.
 * @return The handler for this STUN request.
 */
std::unique_ptr<ClientNetworkStunSocketHandler> ClientNetworkStunSocketHandler::Stun(const std::string &token, int family)
{
	auto stun_handler = std::make_unique<ClientNetworkStunSocketHandler>();

	stun_handler->Connect(family);

	Packet *p = new Packet(PACKET_STUN_CLIENT_STUN);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_string(token);
	p->Send_uint8(family);

	stun_handler->SendPacket(p);

	return stun_handler;
}

/**
 * Check whether we received/can send some data from/to the STUN server and
 * when that's the case handle it appropriately.
 */
void ClientNetworkStunSocketHandler::SendReceive()
{
	if (this->sock == INVALID_SOCKET) return;

	/* We never attempt to receive anything on a STUN socket. After
	 * connecting a STUN connection, the local address will be reused to
	 * to establish the connection with the real server. If we would be to
	 * read this socket, some OSes get confused and deliver us packets meant
	 * for the real connection. It appears most OSes play best when we simply
	 * never attempt to read it to start with (and the packets will remain
	 * available on the other socket).
	 * Protocol-wise, the STUN server will never send any packet back anyway. */

	this->CanSendReceive();
	this->SendPackets();
}
