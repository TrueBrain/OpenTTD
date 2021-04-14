/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_stun.cpp STUN sending/receiving part of the network protocol. */

#include "../stdafx.h"
#include "../debug.h"
#include "../rev.h"
#include "../ai/ai.hpp"
#include "../game/game.hpp"
#include "../thread.h"
#include "../window_func.h"
#include "../error.h"
#include "../base_media_base.h"
#include "../settings_type.h"
#include "network.h"
#include "network_client.h"
#include "network_server.h"
#include "network_stun.h"

#include "table/strings.h"

/** Connect to the STUN server. */
class NetworkStunConnecter : TCPConnecter {
private:
	ClientNetworkStunSocketHandler *stun_handler;

public:
	/**
	 * Initiate the connecting.
	 * @param address The address of the server.
	 */
	NetworkStunConnecter(ClientNetworkStunSocketHandler *stun_handler, const NetworkAddress &address) : TCPConnecter(address), stun_handler(stun_handler) {}

	void OnFailure() override
	{
		/* Connection to STUN server failed; that is a bit weird, as the connection to the GC did work. */
		// TODO -- Figure out what to do in these cases. Record it as a failure to GC? Retry?
		this->stun_handler->CloseConnection(true);
	}

	void OnConnect(SOCKET s) override
	{
		assert(this->stun_handler->sock == INVALID_SOCKET);
		this->stun_handler->sock = s;

		/* Store the local address so other sockets can reuse it.
		 * This is needed for STUN to be successful. */
		sockaddr_storage address = {};
		socklen_t len = sizeof(address);
		getsockname(s, (sockaddr *)&address, &len);
		this->stun_handler->local_addr = NetworkAddress(address, len);
	}
};

void ClientNetworkStunSocketHandler::Connect(int family)
{
	new NetworkStunConnecter(this, NetworkAddress(NETWORK_STUN_SERVER_HOST, NETWORK_STUN_SERVER_PORT, family));
}

ClientNetworkStunSocketHandler *ClientNetworkStunSocketHandler::Stun(const char *token, int family)
{
	ClientNetworkStunSocketHandler *stun_handler = new ClientNetworkStunSocketHandler();

	assert(stun_handler->sock == INVALID_SOCKET);

	stun_handler->Connect(family);

	Packet *p = new Packet(PACKET_STUN_CLIENT_STUN);
	p->Send_uint8(family);
	p->Send_string(token);

	stun_handler->SendPacket(p);

	return stun_handler;
}

/**
 * Check whether we received/can send some data from/to the STUN server and
 * when that's the case handle it appropriately
 */
void ClientNetworkStunSocketHandler::SendReceive()
{
	if (this->sock == INVALID_SOCKET) return;

	/* There should never be any packets send to us; this merely exists out of
	 * code consistency, and to capture any packet that is arrived on this
	 * socket to ease up debugging. */
	if (this->CanSendReceive()) {
		this->ReceivePackets();
	}

	this->SendPackets();
}
