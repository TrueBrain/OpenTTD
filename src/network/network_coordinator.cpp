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
#include "../date_func.h"
#include "../map_func.h"
#include "../thread.h"
#include "../console_func.h"
#include "../company_base.h"
#include "../window_func.h"
#include "../error.h"
#include "../base_media_base.h"
#include "../settings_type.h"
#include "network.h"
#include "network_client.h"
#include "network_server.h"
#include "network_coordinator.h"
#include "network_gamelist.h"
#include "network_stun.h"

#include "table/strings.h"

ClientNetworkCoordinatorSocketHandler _network_coordinator_client;

/** Connecter used after STUN exchange to connect from both sides to each other. */
class TCPStunConnecter : TCPConnecter {
private:
	std::string token;
	TCPServerConnecter *connecter;

public:
	TCPStunConnecter(const NetworkAddress &address, std::string token, TCPServerConnecter *connecter) : TCPConnecter(address), token(token), connecter(connecter) { }

	void OnFailure() override
	{
		/* Close the STUN connection as we now have another TCP stream on the same local address. */
		_network_stun_client.Close();

		_network_coordinator_client.StunFailed(token.c_str());

		if (!_network_server) {
			this->connecter->SetResult(INVALID_SOCKET);
		}
	}

	void OnConnect(SOCKET s) override
	{
		/* Close the STUN connection as we now have another TCP stream on the same local address. */
		_network_stun_client.Close();

		if (_network_server) {
			if (!ServerNetworkGameSocketHandler::ValidateClient(s, address)) return;
			DEBUG(net, 1, "[%s] Client connected from %s via STUN on frame %d", ServerNetworkGameSocketHandler::GetName(), address.GetHostname(), _frame_counter);
			ServerNetworkGameSocketHandler::AcceptConnection(s, address);
		} else {
			this->connecter->SetResult(s);
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
	p->Recv_string(_network_game_info.join_key, lengthof(_network_game_info.join_key));

	DEBUG(net, 5, "Game Coordinator registered our server with join-key '%s'", _network_game_info.join_key);

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_LISTING(Packet *p)
{
	uint8 servers = p->Recv_uint16();

	for (; servers > 0; servers--) {
		char join_key[NETWORK_JOIN_KEY_LENGTH];
		p->Recv_string(join_key, lengthof(join_key));

		NetworkGameList *item = NetworkGameListAddItem(ServerAddress(join_key));

		ClearGRFConfigList(&item->info.grfconfig);
		strecpy(item->info.join_key, join_key, lastof(item->info.join_key));
		this->ReceiveNetworkGameInfo(p, &item->info);

		CheckGameCompatability(item);
	}

	UpdateNetworkGameWindow();

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

	/* We already mark the connection as close, but we do not really close the
	 * socket yet. We do this when the TCPStunConnector is connected. This
	 * prevents any NAT to already remove the route while we create the second
	 * connection. */
	_network_stun_client.CloseConnection(false);

	/* Connect to our peer from the same local address as we use for the
	 * STUN server. This means that if there is any NAT in the local network,
	 * the public ip:port is still pointing to the local address, and as such
	 * a connection can be established. */
	assert(_network_server || this->connecter != nullptr);
	new TCPStunConnecter(address, std::string(token), this->connecter);

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

	if (this->sock != INVALID_SOCKET) closesocket(this->sock);
	this->sock = INVALID_SOCKET;

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Register our server to receive our join-key.
 */
void ClientNetworkCoordinatorSocketHandler::Register()
{
	*_network_game_info.join_key = '\0';

	if (this->sock == INVALID_SOCKET) this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_REGISTER);
	p->Send_uint8(NETWORK_GAME_COORDINATOR_VERSION);
	p->Send_uint8(0); // TODO -- Make this into a type
	p->Send_string(_openttd_revision);

	this->SendPacket(p);
}

/**
 * Join a server based on a join-key.
 */
void ClientNetworkCoordinatorSocketHandler::ConnectToPeer(const char *join_key, TCPServerConnecter *connecter)
{
	// TODO -- Open a window to show we are connecting
	assert(connecter != nullptr);
	this->connecter = connecter;

	if (this->sock == INVALID_SOCKET) this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_CONNECT);
	p->Send_uint8(NETWORK_GAME_COORDINATOR_VERSION);
	p->Send_string(join_key);

	this->SendPacket(p);
}

/**
 * Request a listing of all public servers.
 */
void ClientNetworkCoordinatorSocketHandler::GetListing()
{
	if (this->sock == INVALID_SOCKET) this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_LISTING);
	p->Send_uint8(NETWORK_GAME_COORDINATOR_VERSION);

	this->SendPacket(p);
}

/**
 * Tell the Game Coordinator the STUN failed, and he needs to find us another
 * way to connect.
 */
void ClientNetworkCoordinatorSocketHandler::StunFailed(const char *token)
{
	if (this->sock == INVALID_SOCKET) this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_STUN_FAILED);
	p->Send_uint8(NETWORK_GAME_COORDINATOR_VERSION);
	p->Send_string(token);

	this->SendPacket(p);
}

/**
 * Send an update of our server status to the Game Coordinator.
 */
void ClientNetworkCoordinatorSocketHandler::SendServerUpdate()
{
	NetworkGameInfo ngi;

	/* Update some game_info */
	ngi.clients_on     = _network_game_info.clients_on;
	ngi.start_date     = ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1);

	ngi.server_lang    = _settings_client.network.server_lang;
	ngi.use_password   = !StrEmpty(_settings_client.network.server_password);
	ngi.clients_max    = _settings_client.network.max_clients;
	ngi.companies_on   = (byte)Company::GetNumItems();
	ngi.companies_max  = _settings_client.network.max_companies;
	ngi.spectators_on  = NetworkSpectatorCount();
	ngi.spectators_max = _settings_client.network.max_spectators;
	ngi.game_date      = _date;
	ngi.map_width      = MapSizeX();
	ngi.map_height     = MapSizeY();
	ngi.map_set        = _settings_game.game_creation.landscape;
	ngi.dedicated      = _network_dedicated;
	ngi.grfconfig      = _grfconfig;

	strecpy(ngi.map_name, _network_game_info.map_name, lastof(ngi.map_name));
	strecpy(ngi.server_name, _settings_client.network.server_name, lastof(ngi.server_name));
	strecpy(ngi.join_key, _network_game_info.join_key, lastof(ngi.join_key));
	strecpy(ngi.server_revision, GetNetworkRevisionString(), lastof(ngi.server_revision));

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_UPDATE);
	this->SendNetworkGameInfo(p, &ngi);

	this->SendPacket(p);
}

/**
 * Check whether we received/can send some data from/to the Game Coordinator server and
 * when that's the case handle it appropriately
 */
void ClientNetworkCoordinatorSocketHandler::SendReceive()
{
	static std::chrono::steady_clock::time_point last_update = {};

	if (this->sock == INVALID_SOCKET) {
		if (!this->connecting && _network_server) {
			static std::chrono::steady_clock::time_point last_attempt = {};

			if (std::chrono::steady_clock::now() > last_attempt + std::chrono::seconds(1)) {
				last_attempt = std::chrono::steady_clock::now();

				DEBUG(net, 0, "[tcp/coordinator] Connection with Game Coordinator lost; reconnecting ...");
				this->Reopen();
				this->Register();
				last_update = std::chrono::steady_clock::now() - std::chrono::seconds(30);
			}
		}
		return;
	}

	if (!StrEmpty(_network_game_info.join_key) && std::chrono::steady_clock::now() > last_update + std::chrono::seconds(30)) {
		last_update = std::chrono::steady_clock::now();

		DEBUG(net, 5, "[tcp/coordinator] Sending server update");
		this->SendServerUpdate();
	}

	if (this->CanSendReceive()) {
		this->ReceivePackets();
	}

	this->SendPackets();
}
