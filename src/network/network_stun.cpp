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

ClientNetworkStunSocketHandler _network_stun_client;

/** Connect to the STUN server. */
class NetworkStunConnecter : TCPConnecter {
public:
	/**
	 * Initiate the connecting.
	 * @param address The address of the server.
	 */
	NetworkStunConnecter(const NetworkAddress &address) : TCPConnecter(address) {}

	void OnFailure() override
	{
		// TODO -- Handle graceful
		_network_stun_client.CloseConnection(true);
	}

	void OnConnect(SOCKET s) override
	{
        assert(_network_stun_client.sock == INVALID_SOCKET);
        _network_stun_client.sock = s;

		/* Store the local address so other sockets can reuse it.
		 * This is needed for STUN to be successful. */
		sockaddr_storage address = {};
		socklen_t len = sizeof(address);
		getsockname(s, (sockaddr *)&address, &len);
		_network_stun_client.local_addr = NetworkAddress(address, len);
	}
};

void ClientNetworkStunSocketHandler::Connect()
{
    new NetworkStunConnecter(NetworkAddress(NETWORK_STUN_SERVER_HOST, NETWORK_STUN_SERVER_PORT, AF_UNSPEC));
}

NetworkRecvStatus ClientNetworkStunSocketHandler::CloseConnection(bool error)
{
	NetworkStunSocketHandler::CloseConnection(error);
	_network_stun_client.sock = INVALID_SOCKET;

	return NETWORK_RECV_STATUS_OKAY;
}

void ClientNetworkStunSocketHandler::Stun(const char *token)
{
	assert(this->sock == INVALID_SOCKET);

	this->Connect();

	Packet *p = new Packet(PACKET_STUN_CLIENT_STUN);
	p->Send_uint8(0); // TODO -- Record which interface this is
	p->Send_string(token);

	this->SendPacket(p);
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

	/* If we have sent all packets, close the connection. */
	if (this->SendPackets() == SPS_ALL_SENT) {
		this->Close();
	}
}
