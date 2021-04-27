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

/** Class for handling the client side of the STUN connection. */
class ClientNetworkStunSocketHandler : public NetworkStunSocketHandler {
public:
	NetworkAddress local_addr;

	void Connect(int family);
	void SendReceive();

	static std::unique_ptr<ClientNetworkStunSocketHandler> Stun(const std::string &token, int family);
};

#endif /* NETWORK_STUN_H */
