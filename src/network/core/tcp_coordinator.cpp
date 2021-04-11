/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_coordinator.cpp Basic functions to receive and send Game Coordinator packets.
 */

#include "../../stdafx.h"
#include "tcp_coordinator.h"

#include "../../safeguards.h"

void NetworkCoordinatorSocketHandler::Close()
{
	CloseConnection();
	if (this->sock == INVALID_SOCKET) return;

	closesocket(this->sock);
	this->sock = INVALID_SOCKET;
}

/**
 * Handle the given packet, i.e. pass it to the right
 * parser receive command.
 * @param p the packet to handle
 * @return true if we should immediately handle further packets, false otherwise
 */
bool NetworkCoordinatorSocketHandler::HandlePacket(Packet *p)
{
	PacketCoordinatorType type = (PacketCoordinatorType)p->Recv_uint8();

	switch (type) {
		case PACKET_COORDINATOR_CLIENT_REGISTER:     return this->Receive_CLIENT_REGISTER(p);
		case PACKET_COORDINATOR_SERVER_REGISTER_ACK: return this->Receive_SERVER_REGISTER_ACK(p);
		case PACKET_COORDINATOR_CLIENT_UPDATE:       return this->Receive_CLIENT_UPDATE(p);
		case PACKET_COORDINATOR_CLIENT_JOIN:         return this->Receive_CLIENT_JOIN(p);
		case PACKET_COORDINATOR_SERVER_STUN_REQUEST: return this->Receive_SERVER_STUN_REQUEST(p);
		case PACKET_COORDINATOR_SERVER_STUN_PEER:    return this->Receive_SERVER_STUN_PEER(p);
		case PACKET_COORDINATOR_CLIENT_STUN_FAILED:  return this->Receive_CLIENT_STUN_FAILED(p);

		default:
			DEBUG(net, 0, "[tcp/coordinator] received invalid packet type %d", type);
			return false;
	}
}

/**
 * Receive a packet at TCP level
 * @return Whether at least one packet was received.
 */
bool NetworkCoordinatorSocketHandler::ReceivePackets()
{
	Packet *p;
	static const int MAX_PACKETS_TO_RECEIVE = 4; // We exchange only very few packets, so only doing a few per loop is sufficient.
	int i = MAX_PACKETS_TO_RECEIVE;
	while (--i != 0 && (p = this->ReceivePacket()) != nullptr) {
		bool cont = this->HandlePacket(p);
		delete p;
		if (!cont) return true;
	}

	return i != MAX_PACKETS_TO_RECEIVE - 1;
}

/**
 * Serializes the NetworkGameInfo struct to the packet
 * @param p    the packet to write the data to
 * @param info the NetworkGameInfo struct to serialize
 */
void NetworkCoordinatorSocketHandler::SendNetworkGameInfo(Packet *p, const NetworkGameInfo *info)
{
	p->Send_uint8 (NETWORK_GAME_INFO_VERSION);

	/*
	 *              Please observe the order.
	 * The parts must be read in the same order as they are sent!
	 */

	/* Update the documentation in tcp_coordinator.h on changes
	 * to the NetworkGameInfo wire-protocol in Receive_CLIENT_UPDATE(). */

	/* NETWORK_GAME_INFO_VERSION = 5 */
	p->Send_string(info->join_key);

	/* NETWORK_GAME_INFO_VERSION = 4 */
	{
		/* Only send the GRF Identification (GRF_ID and MD5 checksum) of
		 * the GRFs that are needed, i.e. the ones that the server has
		 * selected in the NewGRF GUI and not the ones that are used due
		 * to the fact that they are in [newgrf-static] in openttd.cfg */
		const GRFConfig *c;
		uint count = 0;

		/* Count number of GRFs to send information about */
		for (c = info->grfconfig; c != nullptr; c = c->next) {
			if (!HasBit(c->flags, GCF_STATIC)) count++;
		}
		p->Send_uint8 (count); // Send number of GRFs

		/* Send actual GRF Identifications */
		for (c = info->grfconfig; c != nullptr; c = c->next) {
			if (!HasBit(c->flags, GCF_STATIC)) this->SendGRFIdentifier(p, &c->ident);
		}
	}

	/* NETWORK_GAME_INFO_VERSION = 3 */
	p->Send_uint32(info->game_date);
	p->Send_uint32(info->start_date);

	/* NETWORK_GAME_INFO_VERSION = 2 */
	p->Send_uint8 (info->companies_max);
	p->Send_uint8 (info->companies_on);
	p->Send_uint8 (info->spectators_max);

	/* NETWORK_GAME_INFO_VERSION = 1 */
	p->Send_string(info->server_name);
	p->Send_string(info->server_revision);
	p->Send_uint8 (info->server_lang);
	p->Send_bool  (info->use_password);
	p->Send_uint8 (info->clients_max);
	p->Send_uint8 (info->clients_on);
	p->Send_uint8 (info->spectators_on);
	p->Send_string(info->map_name);
	p->Send_uint16(info->map_width);
	p->Send_uint16(info->map_height);
	p->Send_uint8 (info->map_set);
	p->Send_bool  (info->dedicated);
}

/**
 * Helper for logging receiving invalid packets.
 * @param type The received packet type.
 * @return Always false, as it's an error.
 */
bool NetworkCoordinatorSocketHandler::ReceiveInvalidPacket(PacketCoordinatorType type)
{
	DEBUG(net, 0, "[tcp/coordinator] received illegal packet type %d", type);
	return false;
}

bool NetworkCoordinatorSocketHandler::Receive_CLIENT_REGISTER(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_REGISTER); }
bool NetworkCoordinatorSocketHandler::Receive_SERVER_REGISTER_ACK(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERVER_REGISTER_ACK); }
bool NetworkCoordinatorSocketHandler::Receive_CLIENT_UPDATE(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_UPDATE); }
bool NetworkCoordinatorSocketHandler::Receive_CLIENT_JOIN(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_JOIN); }
bool NetworkCoordinatorSocketHandler::Receive_SERVER_STUN_REQUEST(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERVER_STUN_REQUEST); }
bool NetworkCoordinatorSocketHandler::Receive_SERVER_STUN_PEER(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERVER_STUN_PEER); }
bool NetworkCoordinatorSocketHandler::Receive_CLIENT_STUN_FAILED(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_STUN_FAILED); }
