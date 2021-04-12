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
#include "game.h"
#include "../../debug.h"

/** Enum with all types of TCP Game Coordinator packets. The order MUST not be changed **/
enum PacketCoordinatorType {
	PACKET_COORDINATOR_CLIENT_REGISTER,
	PACKET_COORDINATOR_SERVER_REGISTER_ACK,
	PACKET_COORDINATOR_CLIENT_UPDATE,
	PACKET_COORDINATOR_CLIENT_LISTING,
	PACKET_COORDINATOR_SERVER_LISTING,
	PACKET_COORDINATOR_CLIENT_JOIN,
	PACKET_COORDINATOR_SERVER_STUN_REQUEST,
	PACKET_COORDINATOR_SERVER_STUN_PEER,
	PACKET_COORDINATOR_CLIENT_STUN_FAILED,
	PACKET_COORDINATOR_END,                 ///< Must ALWAYS be on the end of this list!! (period)
};

/** Base socket handler for all Game Coordinator TCP sockets */
class NetworkCoordinatorSocketHandler : public NetworkTCPSocketHandler {
protected:
	void Close() override;

	bool ReceiveInvalidPacket(PacketCoordinatorType type);

	/**
	 * Client is starting a multiplayer game and wants to let the
	 * Game Coordinator know.
	 *  uint8   Game Coordinator protocol version.
	 *  uint8   Type of game (0 = friends-only, 1 = public).
	 *  string  OpenTTD version the client is running.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_REGISTER(Packet *p);

	/**
	 * Game Coordinator acknowledges the registration.
	 *  string  Join-key that can be used to join this server.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_REGISTER_ACK(Packet *p);

	/**
	 * Send an update of the current state of the server to the Game Coordinator.
	 *  uint8   Game Coordinator protocol version.
	 *  string  Join key of the server
	 *  uint8   Number of GRFs attached (n)
	 *  For each GRF:
	 *    uint32     GRF ID
	 *    bytes[16]  MD5 checksum of the GRF
	 *
	 *  uint32  Current game date in days since 1-1-0 (DMY)
	 *  uint32  Game introduction date in days since 1-1-0 (DMY)
	 *
	 *  uint8   Maximum number of companies allowed on the server
	 *  uint8   Number of companies on the server
	 *  uint8   Maximum number of clients allowed on the server
	 *  uint8   Number of clients on the server
	 *  uint8   Maximum number of spectators allowed on the server
	 *  uint8   Number of spectators on the server
	 *
	 *  string  Name of the server
	 *  string  Revision of the server
	 *  uint8   Whether the server uses a password (0 = no, 1 = yes)
	 *  uint8   Whether the server is dedicated (0 = no, 1 = yes)
	 *
	 *  uint16  Width of the map in tiles
	 *  uint16  Height of the map in tiles
	 *  uint8   Type of map (0 = temperate, 1 = arctic, 2 = desert, 3 = toyland)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_UPDATE(Packet *p);

	/**
	 * Client requests a list of all public servers.
	 *  uint8   Game Coordinator protocol version.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_LISTING(Packet *p);

	/**
	 * Game Coordinator replies with a list of all public servers. Multiple
	 * packets after a single request can be received to complete the full
	 * list of public servers.
	 *  uint16  Amount of public servers in this packet
	 *  For each server:
	 *    same structure as CLIENT_UPDATE
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_LISTING(Packet *p);

	/**
	 * Client wants to join a multiplayer game.
	 *  uint8   Game Coordinator protocol version.
	 *  string  Join-key of the server to join.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_JOIN(Packet *p);

	/**
	 * Game Coordinator requests the client to do a STUN request to the STUN
	 * server. Important is to remember the local port these STUN requests are
	 * send from, as this will be needed for later conenctions too.
	 * The client should do multiple STUN requests for every available
	 * interface that connects to the Internet.
	 *  string  Token to track the current STUN request.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_STUN_REQUEST(Packet *p);

	/**
	 * Game Coordinator informs the client of his STUN peer: the port to
	 * connect to to make a connection. It should start a connect() to
	 * this peer ASAP.
	 *
	 *  string  Token to track the current STUN request.
	 *  uint8   Interface number, as given during STUN request.
	 *  string  Host of the peer.
	 *  uint16  Port of the peer.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_STUN_PEER(Packet *p);

	/**
	 * Client failed to connect to the remote side.
	 *  uint8   Game Coordinator protocol version.
	 *  uint64  Token to track the current STUN request.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_STUN_FAILED(Packet *p);

	bool HandlePacket(Packet *p);
	void ReceiveNetworkGameInfo(Packet *p, NetworkGameInfo *info);
	void SendNetworkGameInfo(Packet *p, const NetworkGameInfo *info);
	void HandleIncomingNetworkGameInfoGRFConfig(GRFConfig *config);
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
