/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_coordinator.h Basic functions to receive and send TCP packets to/from the Game Coordinator server.
 */

#ifndef NETWORK_CORE_TCP_COORDINATOR_H
#define NETWORK_CORE_TCP_COORDINATOR_H

#include "os_abstraction.h"
#include "tcp.h"
#include "packet.h"
#include "game_info.h"

/** Enum with all types of TCP Game Coordinator packets. The order MUST not be changed **/
enum PacketCoordinatorType {
	PACKET_COORDINATOR_CLIENT_REGISTER,       ///< Server registration.
	PACKET_COORDINATOR_SERVER_REGISTER_ACK,   ///< Game Coordinator accepts the registration.
	PACKET_COORDINATOR_CLIENT_UPDATE,         ///< Server sends an set intervals an update of the server.
	PACKET_COORDINATOR_CLIENT_LISTING,        ///< Client is requesting a listing of all public servers.
	PACKET_COORDINATOR_SERVER_LISTING,        ///< Game Coordinator returns a listing of all public servers.
	PACKET_COORDINATOR_CLIENT_CONNECT,        ///< Client wants to connect to a server based on a join-key.
	PACKET_COORDINATOR_SERVER_CONNECTING,     ///< Game Coordinator informs the client of the token assigned to the connection attempt.
	PACKET_COORDINATOR_CLIENT_CONNECT_FAILED, ///< Client/server tells the Game Coordinator the current connection attempt failed.
	PACKET_COORDINATOR_SERVER_CONNECT_FAILED, ///< Game Coordinator informs client/server it has given up on the connection attempt.
	PACKET_COORDINATOR_SERVER_DIRECT_CONNECT, ///< Game Coordinator tells client to directly connect to the IP:host of the server.
	PACKET_COORDINATOR_SERVER_STUN_REQUEST,   ///< Game Coordinator tells client/server to initiate a STUN request.
	PACKET_COORDINATOR_SERVER_STUN_CONNECT,   ///< Game Coordinator tells client/server to connect() reusing the STUN local address.
	PACKET_COORDINATOR_END,                   ///< Must ALWAYS be on the end of this list!! (period)
};

/**
 * The type of connection the Game Coordinator can detect we have.
 */
enum ConnectionType {
	CONNECTION_TYPE_UNKNOWN,  ///< The Game Coordinator hasn't informed us yet what type of connection we have.
	CONNECTION_TYPE_ISOLATED, ///< The Game Coordinator failed to find a way to connect to your server. Nobody will be able to join.
	CONNECTION_TYPE_DIRECT,   ///< The Game Coordinator can directly connect to your server.
	CONNECTION_TYPE_STUN,     ///< The Game Coordinator can connect to your server via a STUN request.
};

/** Base socket handler for all Game Coordinator TCP sockets */
class NetworkCoordinatorSocketHandler : public NetworkTCPSocketHandler {
protected:
	void Close() override;

	bool ReceiveInvalidPacket(PacketCoordinatorType type);

	/**
	 * Client is starting a multiplayer game and wants to let the
	 * Game Coordinator know.
	 *
	 *  uint8   Game Coordinator protocol version.
	 *  uint8   Type of game (0 = friends-only, 1 = public).
	 *  uint16  Local port of the server.
	 *  Serialized NetworkGameInfo. See game_info.hpp for details.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_REGISTER(Packet *p);

	/**
	 * Game Coordinator acknowledges the registration.
	 *
	 *  string  Join-key that can be used to join this server.
	 *  uint8   Type of connection was detected (see ConnectionType).
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_REGISTER_ACK(Packet *p);

	/**
	 * Send an update of the current state of the server to the Game Coordinator.
	 *
	 *  uint8   Game Coordinator protocol version.
	 *  Serialized NetworkShortGameInfo. See game_info.hpp for details.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_UPDATE(Packet *p);

	/**
	 * Client requests a list of all public servers.
	 *
	 *  uint8   Game Coordinator protocol version.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_LISTING(Packet *p);

	/**
	 * Game Coordinator replies with a list of all public servers. Multiple
	 * of these packets are received after a request till all servers are
	 * send over. Last packet will have server count of 0.
	 *
	 *  uint16  Amount of public servers in this packet
	 *  For each server:
	 *    Serialized NetworkGameInfo. See game_info.hpp for details.
	 *    NewGRFs list doesn't have to be completely; the ones listed are
	 *    unknown by the Game Coordinator (read: not on BaNaNaS) and should
	 *    be locally validated.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_LISTING(Packet *p);

	/**
	 * Client wants to connect to a server.
	 *
	 *  uint8   Game Coordinator protocol version.
	 *  string  Join-key of the server to join.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_CONNECT(Packet *p);

	/**
	 * Game Coordinator informs the client under what token it will start the
	 * attempt to connect the server and client together.
	 *
	 *  string  Token to track the current connect request.
	 *  string  Join-key of the server to join.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_CONNECTING(Packet *p);

	/**
	 * Client failed to connect to the remote side.
	 *
	 *  uint8   Game Coordinator protocol version.
	 *  string  Token to track the current connect request.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_CONNECT_FAILED(Packet *p);

	/**
	 * Game Coordinator informs the client that there hasn't been found any
	 * way to connect the client to the server. Any open connections for this
	 * token should be closed now.
	 *
	 *  string  Token to track the current connect request.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_CONNECT_FAILED(Packet *p);

	/**
	 * Game Coordinator requests that we make a direct connection to the
	 * indicated peer, which is a game server.
	 *
	 *  string  Token to track the current connect request.
	 *  string  Host of the peer.
	 *  uint16  Port of the peer.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_DIRECT_CONNECT(Packet *p);

	/**
	 * Game Coordinator requests the client to do a STUN request to the STUN
	 * server. Important is to remember the local port these STUN requests are
	 * send from, as this will be needed for later conenctions too.
	 * The client should do multiple STUN requests for every available
	 * interface that connects to the Internet.
	 *
	 *  string  Token to track the current connect request.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_STUN_REQUEST(Packet *p);

	/**
	 * Game Coordinator informs the client of his STUN peer: the port to
	 * connect to to make a connection. It should start a connect() to
	 * this peer ASAP with the local address as used with the STUN request.
	 *
	 *  string  Token to track the current connect request.
	 *  uint8   Interface number, as given during STUN request.
	 *  string  Host of the peer.
	 *  uint16  Port of the peer.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_STUN_CONNECT(Packet *p);

	bool HandlePacket(Packet *p);
public:
	/**
	 * Create a new cs socket handler for a given cs
	 * @param s  the socket we are connected with
	 * @param address IP etc. of the client
	 */
	NetworkCoordinatorSocketHandler(SOCKET s = INVALID_SOCKET) :
		NetworkTCPSocketHandler(s)
	{
	}

	/** On destructing of this class, the socket needs to be closed */
	virtual ~NetworkCoordinatorSocketHandler() { this->Close(); }

	bool ReceivePackets();
};

#endif /* NETWORK_CORE_TCP_COORDINATOR_H */
