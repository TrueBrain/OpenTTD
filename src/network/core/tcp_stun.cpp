/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_stun.cpp Basic functions to receive and send STUN packets.
 */

#include "../../stdafx.h"
#include "tcp_stun.h"

#include "../../safeguards.h"

void NetworkStunSocketHandler::Close()
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
bool NetworkStunSocketHandler::HandlePacket(Packet *p)
{
	PacketStunType type = (PacketStunType)p->Recv_uint8();

	switch (type) {
		case PACKET_STUN_CLIENT_STUN: return this->Receive_CLIENT_STUN(p);

		default:
			DEBUG(net, 0, "[tcp/stun] received invalid packet type %d", type);
			return false;
	}
}

/**
 * Receive a packet at TCP level
 * @return Whether at least one packet was received.
 */
bool NetworkStunSocketHandler::ReceivePackets()
{
	Packet *p;
	static const int MAX_PACKETS_TO_RECEIVE = 2; // We should never receive any packets, so only check the first few.
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
bool NetworkStunSocketHandler::ReceiveInvalidPacket(PacketStunType type)
{
	DEBUG(net, 0, "[tcp/stun] received illegal packet type %d", type);
	return false;
}

bool NetworkStunSocketHandler::Receive_CLIENT_STUN(Packet *p) { return this->ReceiveInvalidPacket(PACKET_STUN_CLIENT_STUN); }
