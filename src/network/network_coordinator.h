/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_coordinator.h Part of the network protocol handling Game Coordinator requests. */

#ifndef NETWORK_COORDINATOR_H
#define NETWORK_COORDINATOR_H

#include "core/tcp_coordinator.h"

class ClientNetworkCoordinatorSocketHandler : public NetworkCoordinatorSocketHandler {
protected:
    bool Receive_SERVER_REGISTER_ACK(Packet *p) override;
    bool Receive_SERVER_STUN_REQUEST(Packet *p) override;
    bool Receive_SERVER_STUN_PEER(Packet *p) override;

public:
    bool connecting;

    ClientNetworkCoordinatorSocketHandler() : connecting(false) {}

    NetworkRecvStatus CloseConnection(bool error = true) override;

    void Connect();
    void SendReceive();

    void Register();
    void SendServerUpdate();

    void Join(const char *join_key);

    void StunFailed(const char *token);
};

extern ClientNetworkCoordinatorSocketHandler _network_coordinator_client;

#endif /* NETWORK_COORDINATOR_H */
