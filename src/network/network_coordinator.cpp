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
#include "../settings_type.h"
#include "../window_func.h"
#include "../window_type.h"
#include "network.h"
#include "network_coordinator.h"
#include "network_gamelist.h"
#include "network_internal.h"

#include "../safeguards.h"

static const auto NETWORK_COORDINATOR_DELAY_BETWEEN_UPDATES = std::chrono::seconds(30); ///< How many time between updates the server sends to the Game Coordinator.
ClientNetworkCoordinatorSocketHandler _network_coordinator_client; ///< The connection to the Game Coordinator.
ConnectionType _network_server_connection_type = CONNECTION_TYPE_UNKNOWN; ///< What type of connection the Game Coordinator detected we are on.

/** Connect to the Game Coordinator server. */
class NetworkCoordinatorConnecter : TCPConnecter {
public:
	/**
	 * Initiate the connecting.
	 * @param address The address of the Game Coordinator server.
	 */
	NetworkCoordinatorConnecter(const std::string &connection_string) : TCPConnecter(connection_string, NETWORK_COORDINATOR_SERVER_PORT) {}

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
	/* Schedule sending an update. */
	this->next_update = std::chrono::steady_clock::now();

	p->Recv_string(_network_game_info.join_key, lengthof(_network_game_info.join_key));
	_network_server_connection_type = (ConnectionType)p->Recv_uint8();

	if (_network_server_connection_type == CONNECTION_TYPE_ISOLATED) {
		// TODO -- Warn user nobody will be able to connect
	}

	SetWindowDirty(WC_CLIENT_LIST, 0);

	DEBUG(net, 2, "Game Coordinator registered our server with join-key '%s'", _network_game_info.join_key);

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_LISTING(Packet *p)
{
	uint8 servers = p->Recv_uint16();

	/* End of list; we can now remove all expired items from the list. */
	if (servers == 0) {
		NetworkGameListRemoveExpired();
		return true;
	}

	for (; servers > 0; servers--) {
		/* Read the NetworkGameInfo from the packet. */
		NetworkGameInfo ngi;
		DeserializeNetworkGameInfo(p, &ngi);

		/* Now we know the join-key, we can add it to our list. */
		NetworkGameList *item = NetworkGameListAddItem("+" + std::string(ngi.join_key));

		/* Clear any existing GRFConfig chain. */
		ClearGRFConfigList(&item->info.grfconfig);
		/* Copy the new NetworkGameInfo info. */
		item->info = ngi;
		/* Check for compatability with the client. */
		CheckGameCompatibility(item->info);
		/* Mark server as online. */
		item->online = true;
		/* Mark the item as up-to-date. */
		item->version = _network_game_list_version;
	}

	UpdateNetworkGameWindow();
	return true;
}

void ClientNetworkCoordinatorSocketHandler::Connect()
{
	/* We are either already connected or are trying to connect. */
	if (this->sock != INVALID_SOCKET || this->connecting) return;

	this->Reopen();

	this->connecting = true;
	new NetworkCoordinatorConnecter(NETWORK_COORDINATOR_SERVER_HOST);
}

NetworkRecvStatus ClientNetworkCoordinatorSocketHandler::CloseConnection(bool error)
{
	NetworkCoordinatorSocketHandler::CloseConnection(error);

	DEBUG(net, 1, "[tcp/coordinator] closed connection");

	if (this->sock != INVALID_SOCKET) closesocket(this->sock);
	this->sock = INVALID_SOCKET;

	*_network_game_info.join_key = '\0';
	_network_server_connection_type = CONNECTION_TYPE_UNKNOWN;

	SetWindowDirty(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Register our server to receive our join-key.
 */
void ClientNetworkCoordinatorSocketHandler::Register()
{
	*_network_game_info.join_key = '\0';
	_network_server_connection_type = CONNECTION_TYPE_UNKNOWN;

	SetWindowDirty(WC_CLIENT_LIST, 0);

	this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_REGISTER);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_uint8(SERVER_GAME_TYPE_PUBLIC);
	p->Send_uint16(_settings_client.network.server_port);
	p->Send_string(_openttd_revision);

	this->SendPacket(p);
}

/**
 * Send an update of our server status to the Game Coordinator.
 */
void ClientNetworkCoordinatorSocketHandler::SendServerUpdate()
{
	DEBUG(net, 5, "[tcp/coordinator] Sending server update");
	this->next_update = std::chrono::steady_clock::now() + NETWORK_COORDINATOR_DELAY_BETWEEN_UPDATES;

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_UPDATE);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	SerializeNetworkGameInfo(p, GetCurrentNetworkServerGameInfo());

	this->SendPacket(p);
}

/**
 * Request a listing of all public servers.
 */
void ClientNetworkCoordinatorSocketHandler::GetListing()
{
	this->Connect();

	_network_game_list_version++;

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_LISTING);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);

	this->SendPacket(p);
}

/**
 * Check whether we received/can send some data from/to the Game Coordinator server and
 * when that's the case handle it appropriately
 */
void ClientNetworkCoordinatorSocketHandler::SendReceive()
{
	/* Private games are not listed via the Game Coordinator. */
	if (!_settings_client.network.server_advertise) {
		if (this->sock != INVALID_SOCKET) {
			this->Close();
		}
		return;
	}

	static int last_attempt_backoff = 1;

	if (this->sock == INVALID_SOCKET) {
		if (!this->connecting && _network_server) {
			static std::chrono::steady_clock::time_point last_attempt = {};

			if (std::chrono::steady_clock::now() > last_attempt + std::chrono::seconds(1) * last_attempt_backoff) {
				last_attempt = std::chrono::steady_clock::now();
				/* Delay reconnecting with up to 32 seconds. */
				if (last_attempt_backoff < 30) {
					last_attempt_backoff *= 2;
				}

				DEBUG(net, 0, "[tcp/coordinator] Connection with Game Coordinator lost; reconnecting ...");
				this->Register();
			}
		}
		return;
	} else if (last_attempt_backoff != 1) {
		last_attempt_backoff = 1;
	}

	if (!StrEmpty(_network_game_info.join_key) && std::chrono::steady_clock::now() > this->next_update) {
		this->SendServerUpdate();
	}

	if (this->CanSendReceive()) {
		this->ReceivePackets();
	}

	this->SendPackets();
}
