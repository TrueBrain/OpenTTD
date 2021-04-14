/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file core/udp.h Basic functions to receive and send UDP packets.
 */

#ifndef NETWORK_CORE_UDP_H
#define NETWORK_CORE_UDP_H

#include "address.h"
#include "packet.h"

/** Enum with all types of UDP packets. The order MUST not be changed **/
enum PacketUDPType {
	PACKET_UDP_CLIENT_FIND_SERVER,   ///< Queries a game server for game information
	PACKET_UDP_SERVER_RESPONSE,      ///< Reply of the game server with game information
	PACKET_UDP_END,                  ///< Must ALWAYS be on the end of this list!! (period)
};

/** Base socket handler for all UDP sockets */
class NetworkUDPSocketHandler : public NetworkSocketHandler {
protected:
	/** The address to bind to. */
	NetworkAddressList bind;
	/** The opened sockets. */
	SocketList sockets;

	NetworkRecvStatus CloseConnection(bool error = true) override;

	void ReceiveInvalidPacket(PacketUDPType, NetworkAddress *client_addr);

	/**
	 * Queries to the server for information about the game.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr);

	/**
	 * Return of server information to the client.
	 *  Serialized NetworkGameInfo. See game_info.hpp for details.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr);

	void HandleUDPPacket(Packet *p, NetworkAddress *client_addr);

public:
	NetworkUDPSocketHandler(NetworkAddressList *bind = nullptr);

	/** On destructing of this class, the socket needs to be closed */
	virtual ~NetworkUDPSocketHandler() { this->Close(); }

	bool Listen();
	void Close() override;

	void SendPacket(Packet *p, NetworkAddress *recv, bool all = false, bool broadcast = false);
	void ReceivePackets();
};

#endif /* NETWORK_CORE_UDP_H */
