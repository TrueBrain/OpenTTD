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
#include "network_server.h"
#include "network_stun.h"

#include "../safeguards.h"

static const auto NETWORK_COORDINATOR_DELAY_BETWEEN_UPDATES = std::chrono::seconds(30); ///< How many time between updates the server sends to the Game Coordinator.
ClientNetworkCoordinatorSocketHandler _network_coordinator_client; ///< The connection to the Game Coordinator.
ConnectionType _network_server_connection_type = CONNECTION_TYPE_UNKNOWN; ///< What type of connection the Game Coordinator detected we are on.

/** Connect to a game server by IP:port. */
class NetworkDirectConnecter : TCPConnecter {
private:
	std::string token;

public:
	/**
	 * Initiate the connecting.
	 * @param hostname The hostname of the server.
	 * @param port The port of the server.
	 * @param token The connection token.
	 */
	NetworkDirectConnecter(const std::string &hostname, uint16 port, const std::string &token) : TCPConnecter(hostname, port), token(token) {}

	void OnFailure() override
	{
		_network_coordinator_client.ConnectFailure(token);
	}

	void OnConnect(SOCKET s) override
	{
		_network_coordinator_client.ConnectSuccess(token, s, this->address);
	}
};

/** Connecter used after STUN exchange to connect from both sides to each other. */
class NetworkReuseStunConnecter : TCPBindConnecter {
private:
	std::string token;
	int family;

public:
	/**
	 * Initiate the connecting.
	 * @param address The address of the server.
	 */
	NetworkReuseStunConnecter(const std::string &connection_string, uint16 port, const NetworkAddress &bind_address, std::string token, int family) : TCPBindConnecter(connection_string, port, bind_address), token(token), family(family) { }

	void OnFailure() override
	{
		/* Close the STUN connection too, as it is no longer of use. */
		_network_coordinator_client.CloseStunHandler(this->token, this->family);

		_network_coordinator_client.ConnectFailure(this->token);
	}

	void OnConnect(SOCKET s) override
	{
		/* Close all STUN connections as we now have a bidirectional socket
		 * with the other side. Closing the STUN connections is important, as
		 * we now have two sockets on the same local address; better fix that
		 * quickly to avoid OSes getting confused. */
		_network_coordinator_client.CloseStunHandler(this->token);

		_network_coordinator_client.ConnectSuccess(this->token, s, this->address);
	}
};

/** Connect to the Game Coordinator server. */
class NetworkCoordinatorConnecter : TCPConnecter {
public:
	/**
	 * Initiate the connecting.
	 * @param connection_string The address of the Game Coordinator server.
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

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_CONNECTING(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);
	std::string join_key = p->Recv_string(NETWORK_JOIN_KEY_LENGTH);

	/* Find the connecter based on the join-key. */
	auto connecter_it = this->connecter_pre.find(join_key);
	assert(connecter_it != this->connecter_pre.end());

	/* Now store it based on the token. */
	this->connecter_pre.erase(connecter_it);
	this->connecter[token] = connecter_it->second;

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_CONNECT_FAILED(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);

	auto connecter_it = this->connecter.find(token);
	if (connecter_it != this->connecter.end()) {
		connecter_it->second->SetResult(INVALID_SOCKET);
		this->connecter.erase(connecter_it);
	}

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_DIRECT_CONNECT(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);
	std::string host = p->Recv_string(NETWORK_HOSTNAME_PORT_LENGTH);
	uint16 port = p->Recv_uint16();

	new NetworkDirectConnecter(host, port, token);
	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_STUN_REQUEST(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);

	// TODO -- To avoid warnings like "invalid argument" when connections don't have IPv6, we should try to detect if either is supported before using

	this->stun_handlers[token][AF_INET] = ClientNetworkStunSocketHandler::Stun(token, AF_INET);
//	this->stun_handlers[token][AF_INET6] = ClientNetworkStunSocketHandler::Stun(token, AF_INET6);
	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_STUN_CONNECT(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);
	uint8 family = p->Recv_uint8();
	std::string host = p->Recv_string(NETWORK_HOSTNAME_PORT_LENGTH);
	uint16 port = p->Recv_uint16();

	/* Check if we know this token. */
	auto stun_it = this->stun_handlers.find(token);
	if (stun_it == this->stun_handlers.end()) {
		/* Game Coordinator and client are not agreeing on state. */
		this->CloseConnection();
		return false;
	}
	auto family_it = stun_it->second.find(family);
	if (family_it == stun_it->second.end()) {
		/* Game Coordinator and client are not agreeing on state. */
		this->CloseConnection();
		return false;
	}

	/* We now mark the connection as close, but we do not really close the
	 * socket yet. We do this when the NetworkReuseStunConnecter is connected.
	 * This prevents any NAT to already remove the route while we create the
	 * second connection on top of the first. */
	family_it->second->CloseConnection(false);

	/* Connect to our peer from the same local address as we use for the
	 * STUN server. This means that if there is any NAT in the local network,
	 * the public ip:port is still pointing to the local address, and as such
	 * a connection can be established. */
	new NetworkReuseStunConnecter(host, port, family_it->second->local_addr, token, family);
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

	/* Mark any pending connecter as failed. */
	for (auto &[key, it] : this->connecter) {
		it->SetResult(INVALID_SOCKET);
	}
	for (auto &[key, it] : this->connecter_pre) {
		it->SetResult(INVALID_SOCKET);
	}

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
 * Join a server based on a join-key.
 */
void ClientNetworkCoordinatorSocketHandler::ConnectToServer(const std::string &join_key, TCPServerConnecter *connecter)
{
	if (this->connecter_pre.find(join_key) != this->connecter_pre.end()) {
		/* It shouldn't be possible to connect to the same server before a
		 * token is assigned to the connection attempt. In case it does
		 * happen, report the second attempt as failed. */
		connecter->SetResult(INVALID_SOCKET);
		return;
	}

	/* Initially we store based on join-key; on first reply we know the token,
	 * and will start using that key instead. */
	this->connecter_pre[join_key] = connecter;

	this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_CONNECT);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_string(join_key.c_str());

	this->SendPacket(p);
}

/**
 * Callback from a Connecter to let the Game Coordinator know the connection failed.
 */
void ClientNetworkCoordinatorSocketHandler::ConnectFailure(const std::string &token)
{
	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_CONNECT_FAILED);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_string(token);

	this->SendPacket(p);

	/* We do not close the associated connecter here yet, as the
	 * Game Coordinator might have other methods of connecting available. */
}

/**
 * Callback from a Connecter to let the Game Coordinator know the connection
 * to the game server is established.
 */
void ClientNetworkCoordinatorSocketHandler::ConnectSuccess(const std::string &token, SOCKET sock, NetworkAddress &address)
{
	if (_network_server) {
		if (!ServerNetworkGameSocketHandler::ValidateClient(sock, address)) return;
		DEBUG(net, 1, "[%s] Client connected from %s on frame %d", ServerNetworkGameSocketHandler::GetName(), address.GetHostname(), _frame_counter);
		ServerNetworkGameSocketHandler::AcceptConnection(sock, address);
	} else {
		auto connecter_it = this->connecter.find(token);
		assert(connecter_it != this->connecter.end());

		connecter_it->second->SetResult(sock);
		this->connecter.erase(connecter_it);
	}
}

void ClientNetworkCoordinatorSocketHandler::CloseStunHandler(std::string token, int family)
{
	auto stun_it = this->stun_handlers.find(token);
	if (stun_it == this->stun_handlers.end()) return;

	if (family == AF_UNSPEC) {
		for (auto &[family, stun_handler] : stun_it->second) {
			stun_handler->Close();
		}

		this->stun_handlers.erase(stun_it);
	} else {
		auto family_it = stun_it->second.find(family);
		if (family_it == stun_it->second.end()) return;

		family_it->second->Close();

		stun_it->second.erase(family_it);
	}
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

	for (const auto &[token, families] : this->stun_handlers) {
		for (const auto &[family, stun_handler] : families) {
			stun_handler->SendReceive();
		}
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
