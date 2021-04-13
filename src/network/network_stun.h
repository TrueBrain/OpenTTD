/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_stun.h Part of the network protocol handling STUN requests. */

#ifndef NETWORK_STUN_H
#define NETWORK_STUN_H

#include "core/tcp_stun.h"

class ClientNetworkStunSocketHandler : public NetworkStunSocketHandler {
public:
	NetworkAddress local_addr;

	void Connect();
	void SendReceive();

	void Stun(const char *token);
};

extern ClientNetworkStunSocketHandler _network_stun_client;

#endif /* NETWORK_STUN_H */
