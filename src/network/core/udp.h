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
	PACKET_UDP_CLIENT_DETAIL_INFO,   ///< Queries a game server about details of the game, such as companies
	PACKET_UDP_SERVER_DETAIL_INFO,   ///< Reply of the game server about details of the game, such as companies
	PACKET_UDP_CLIENT_GET_NEWGRFS,   ///< Requests the name for a list of GRFs (GRF_ID and MD5)
	PACKET_UDP_SERVER_NEWGRFS,       ///< Sends the list of NewGRF's requested.
	PACKET_UDP_END,                  ///< Must ALWAYS be on the end of this list!! (period)
};

/** The types of server lists we can get */
enum ServerListType {
	SLT_IPv4 = 0,   ///< Get the IPv4 addresses
	SLT_IPv6 = 1,   ///< Get the IPv6 addresses
	SLT_AUTODETECT, ///< Autodetect the type based on the connection

	SLT_END = SLT_AUTODETECT, ///< End of 'arrays' marker
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

	/**
	 * Query for detailed information about companies.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_DETAIL_INFO(Packet *p, NetworkAddress *client_addr);

	/**
	 * Reply with detailed company information.
	 * uint8   Version of the packet.
	 * uint8   Number of companies.
	 * For each company:
	 *   uint8   ID of the company.
	 *   string  Name of the company.
	 *   uint32  Year the company was inaugurated.
	 *   uint64  Value.
	 *   uint64  Money.
	 *   uint64  Income.
	 *   uint16  Performance (last quarter).
	 *   bool    Company is password protected.
	 *   uint16  Number of trains.
	 *   uint16  Number of lorries.
	 *   uint16  Number of busses.
	 *   uint16  Number of planes.
	 *   uint16  Number of ships.
	 *   uint16  Number of train stations.
	 *   uint16  Number of lorry stations.
	 *   uint16  Number of bus stops.
	 *   uint16  Number of airports and heliports.
	 *   uint16  Number of harbours.
	 *   bool    Company is an AI.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_DETAIL_INFO(Packet *p, NetworkAddress *client_addr);

	/**
	 * The client requests information about some NewGRFs.
	 * uint8   The number of NewGRFs information is requested about.
	 * For each NewGRF:
	 *   uint32      The GRFID.
	 *   16 * uint8  MD5 checksum of the GRF.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_GET_NEWGRFS(Packet *p, NetworkAddress *client_addr);

	/**
	 * The server returns information about some NewGRFs.
	 * uint8   The number of NewGRFs information is requested about.
	 * For each NewGRF:
	 *   uint32      The GRFID.
	 *   16 * uint8  MD5 checksum of the GRF.
	 *   string      The name of the NewGRF.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr);

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
