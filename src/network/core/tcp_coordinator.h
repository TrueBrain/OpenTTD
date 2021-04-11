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
	 * This packet has several legacy versions, so we list the version and size of each "field":
	 *
	 * Version: Bytes:  Description:
	 *   all      1       the version of this packet's structure
	 *
	 *   5+       var     string with the join key of the server
	 *
	 *   4+       1       number of GRFs attached (n)
	 *   4+       n * 20  unique identifier for GRF files. Consists of:
	 *                     - one 4 byte variable with the GRF ID
	 *                     - 16 bytes (sent sequentially) for the MD5 checksum
	 *                       of the GRF
	 *
	 *   3+       4       current game date in days since 1-1-0 (DMY)
	 *   3+       4       game introduction date in days since 1-1-0 (DMY)
	 *
	 *   2+       1       maximum number of companies allowed on the server
	 *   2+       1       number of companies on the server
	 *   2+       1       maximum number of spectators allowed on the server
	 *
	 *   1+       var     string with the name of the server
	 *   1+       var     string with the revision of the server
	 *   1+       1       the language run on the server
	 *                    (0 = any, 1 = English, 2 = German, 3 = French)
	 *   1+       1       whether the server uses a password (0 = no, 1 = yes)
	 *   1+       1       maximum number of clients allowed on the server
	 *   1+       1       number of clients on the server
	 *   1+       1       number of spectators on the server
	 *   1 & 2    2       current game date in days since 1-1-1920 (DMY)
	 *   1 & 2    2       game introduction date in days since 1-1-1920 (DMY)
	 *   1+       var     string with the name of the map
	 *   1+       2       width of the map in tiles
	 *   1+       2       height of the map in tiles
	 *   1+       1       type of map:
	 *                    (0 = temperate, 1 = arctic, 2 = desert, 3 = toyland)
	 *   1+       1       whether the server is dedicated (0 = no, 1 = yes)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_UPDATE(Packet *p);

	/**
	 * Client wants to join a multiplayer game.
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
	 *  uint64  Token to track the current STUN request.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_STUN_FAILED(Packet *p);

	bool HandlePacket(Packet *p);
	void SendNetworkGameInfo(Packet *p, const NetworkGameInfo *info);
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
