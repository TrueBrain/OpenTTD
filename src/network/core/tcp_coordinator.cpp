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
bool NetworkCoordinatorSocketHandler::Receive_CLIENT_JOIN(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_JOIN); }
bool NetworkCoordinatorSocketHandler::Receive_SERVER_STUN_REQUEST(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERVER_STUN_REQUEST); }
bool NetworkCoordinatorSocketHandler::Receive_SERVER_STUN_PEER(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERVER_STUN_PEER); }
bool NetworkCoordinatorSocketHandler::Receive_CLIENT_STUN_FAILED(Packet *p) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_STUN_FAILED); }
