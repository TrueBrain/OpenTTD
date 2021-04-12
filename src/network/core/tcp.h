/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp.h Basic functions to receive and send TCP packets.
 */

#ifndef NETWORK_CORE_TCP_H
#define NETWORK_CORE_TCP_H

#include "address.h"
#include "packet.h"

#include <atomic>

/** The states of sending the packets. */
enum SendPacketsState {
	SPS_CLOSED,      ///< The connection got closed.
	SPS_NONE_SENT,   ///< The buffer is still full, so no (parts of) packets could be sent.
	SPS_PARTLY_SENT, ///< The packets are partly sent; there are more packets to be sent in the queue.
	SPS_ALL_SENT,    ///< All packets in the queue are sent.
};

/** Base socket handler for all TCP sockets */
class NetworkTCPSocketHandler : public NetworkSocketHandler {
private:
	Packet *packet_queue;     ///< Packets that are awaiting delivery
	Packet *packet_recv;      ///< Partially received packet
public:
	SOCKET sock;              ///< The socket currently connected to
	bool writable;            ///< Can we write to this socket?

	/**
	 * Whether this socket is currently bound to a socket.
	 * @return true when the socket is bound, false otherwise
	 */
	bool IsConnected() const { return this->sock != INVALID_SOCKET; }

	NetworkRecvStatus CloseConnection(bool error = true) override;
	virtual void SendPacket(Packet *packet);
	SendPacketsState SendPackets(bool closing_down = false);

	virtual Packet *ReceivePacket();

	bool CanSendReceive();

	/**
	 * Whether there is something pending in the send queue.
	 * @return true when something is pending in the send queue.
	 */
	bool HasSendQueue() { return this->packet_queue != nullptr; }

	NetworkTCPSocketHandler(SOCKET s = INVALID_SOCKET);
	~NetworkTCPSocketHandler();
};

/**
 * "Helper" class for creating TCP connections in a non-blocking manner
 */
class TCPConnecter {
private:
	std::atomic<bool> connected;///< Whether we succeeded in making the connection
	std::atomic<bool> aborted;  ///< Whether we bailed out (i.e. connection making failed)
	bool killed;                ///< Whether we got killed
	SOCKET sock;                ///< The socket we're connecting with

	void Connect();

	static void ThreadEntry(TCPConnecter *param);

	/* TCPServerConnecter acts like a TCPConnecter with some clever bits added. */
	friend class TCPServerConnecter;

protected:
	/** Address we're connecting to */
	NetworkAddress address;

public:
	TCPConnecter();
	TCPConnecter(const NetworkAddress &address);
	/** Silence the warnings */
	virtual ~TCPConnecter() {}

	/**
	 * Callback when the connection succeeded.
	 * @param s the socket that we opened
	 */
	virtual void OnConnect(SOCKET s) {}

	/**
	 * Callback for when the connection attempt failed.
	 */
	virtual void OnFailure() {}

	static void CheckCallbacks();
	static void KillAll();
};

/**
 * "Helper" class for creating TCP connection either via a direct IP connection
 * or via a GameCoordinator exchange, like STUN.
 *
 * The caller doesn't need to care how the connection is established. Either
 * OnFailure() is called if all possible ways to connect to the server are
 * exhausted, or OnConnect() is called with a valid socket to talk to the
 * server with.
 */
class TCPServerConnecter : public TCPConnecter {
protected:
	/* Server address we're connecting to. */
	ServerAddress server_address;

public:
	TCPServerConnecter(const ServerAddress &address);

	void SetResult(SOCKET sock);
};

#endif /* NETWORK_CORE_TCP_H */
