/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_coordinator.cpp Game Coordinator sending/receiving part of the network protocol. */

#include "../stdafx.h"
#include "../debug.h"
#include "../rev.h"
#include "../ai/ai.hpp"
#include "../game/game.hpp"
#include "../thread.h"
#include "../console_func.h"
#include "../window_func.h"
#include "../error.h"
#include "../base_media_base.h"
#include "../settings_type.h"
#include "network.h"
#include "network_client.h"
#include "network_server.h"
#include "network_coordinator.h"
#include "network_stun.h"

#include "table/strings.h"

ClientNetworkCoordinatorSocketHandler _network_coordinator_client;

/** Connecter used after STUN exchange to connect from both sides to each other. */
class TCPDirectConnecter : TCPConnecter {
private:
	std::string token;

public:
	TCPDirectConnecter(const NetworkAddress &address, std::string token) : TCPConnecter(address), token(token) {}

	void OnFailure() override
	{
		_network_coordinator_client.StunFailed(token.c_str());
	}

	void OnConnect(SOCKET s) override
	{
		if (_network_server) {
			if (!ServerNetworkGameSocketHandler::ValidateClient(s, address)) return;
			DEBUG(net, 1, "[%s] Client connected from %s via STUN on frame %d", ServerNetworkGameSocketHandler::GetName(), address.GetHostname(), _frame_counter);
			ServerNetworkGameSocketHandler::AcceptConnection(s, address);
		} else {
			_networking = true;
			new ClientNetworkGameSocketHandler(s);
			IConsoleCmdExec("exec scripts/on_client.scr 0");
			NetworkClient_Connected();
		}
	}
};

/** Connect to the Game Coordinator server. */
class NetworkCoordinatorConnecter : TCPConnecter {
public:
	/**
	 * Initiate the connecting.
	 * @param address The address of the server.
	 */
	NetworkCoordinatorConnecter(const NetworkAddress &address) : TCPConnecter(address) {}

	void OnFailure() override
	{
		_network_coordinator_client.connecting = false;
		_network_coordinator_client.CloseConnection(true);
	}

	void OnConnect(SOCKET s) override
	{
        assert(_network_coordinator_client.sock == INVALID_SOCKET);

        _network_coordinator_client.sock = s;
		_network_coordinator_client.connecting = false;
	}
};

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_REGISTER_ACK(Packet *p)
{
	char join_key[64];

	p->Recv_string(join_key, lengthof(join_key));

	DEBUG(misc, 0, "Your join-key: %s", join_key);

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_STUN_REQUEST(Packet *p)
{
	char token[64];

	p->Recv_string(token, lengthof(token));

	_network_stun_client.Stun(token);
	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_STUN_PEER(Packet *p)
{
	char token[64];
	char host[64];

	p->Recv_string(token, lengthof(token));
	uint8 interface_number = p->Recv_uint8(); // TODO -- Do something with this
	p->Recv_string(host, lengthof(host));
	uint16 port = p->Recv_uint16();

	NetworkAddress address = NetworkAddress(host, port);
	address.SetConnectBindAddress(_network_stun_client.local_addr);
	new TCPDirectConnecter(address, std::string(token));

	return true;
}

void ClientNetworkCoordinatorSocketHandler::Connect()
{
	this->connecting = true;
    new NetworkCoordinatorConnecter(NetworkAddress(NETWORK_COORDINATOR_SERVER_HOST, NETWORK_COORDINATOR_SERVER_PORT, AF_UNSPEC));
}

NetworkRecvStatus ClientNetworkCoordinatorSocketHandler::CloseConnection(bool error)
{
	NetworkCoordinatorSocketHandler::CloseConnection(error);
	_network_coordinator_client.sock = INVALID_SOCKET;
	return NETWORK_RECV_STATUS_OKAY;
}

void ClientNetworkCoordinatorSocketHandler::Register()
{
	if (this->sock == INVALID_SOCKET) this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_REGISTER);
	p->Send_uint8(0); // TODO -- Make this into a type
	p->Send_string(_openttd_revision);

	this->SendPacket(p);
}

void ClientNetworkCoordinatorSocketHandler::Join(const char *join_key)
{
	if (this->sock == INVALID_SOCKET) this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_JOIN);
	p->Send_string(join_key);

	this->SendPacket(p);
}

void ClientNetworkCoordinatorSocketHandler::StunFailed(const char *token)
{
	if (this->sock == INVALID_SOCKET) this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_STUN_FAILED);
	p->Send_string(token);

	this->SendPacket(p);
}

/**
 * Check whether we received/can send some data from/to the Game Coordinator server and
 * when that's the case handle it appropriately
 */
void ClientNetworkCoordinatorSocketHandler::SendReceive()
{
	if (this->sock == INVALID_SOCKET) {
		if (!this->connecting && _network_server) {
			static std::chrono::steady_clock::time_point last_attempt = {};

			if (std::chrono::steady_clock::now() > last_attempt + std::chrono::seconds(1)) {
				last_attempt = std::chrono::steady_clock::now();

				DEBUG(net, 0, "[tcp/coordinator] Connection with Game Coordinator lost; reconnecting ...");
				this->Reopen();
				this->Register();
			}
		}
		return;
	}

	if (this->CanSendReceive()) {
		this->ReceivePackets();
	}

	this->SendPackets();
}
