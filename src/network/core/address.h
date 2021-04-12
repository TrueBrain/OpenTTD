/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file core/address.h Wrapper for network addresses. */

#ifndef NETWORK_CORE_ADDRESS_H
#define NETWORK_CORE_ADDRESS_H

#include "os_abstraction.h"
#include "config.h"
#include "../../string_func.h"
#include "../../core/smallmap_type.hpp"

#include <string>

class NetworkAddress;
typedef std::vector<NetworkAddress> NetworkAddressList; ///< Type for a list of addresses.
typedef SmallMap<NetworkAddress, SOCKET> SocketList;    ///< Type for a mapping between address and socket.

/**
 * Wrapper for (un)resolved network addresses; there's no reason to transform
 * a numeric IP to a string and then back again to pass it to functions. It
 * furthermore allows easier delaying of the hostname lookup.
 */
class NetworkAddress {
private:
	char hostname[NETWORK_HOSTNAME_LENGTH]; ///< The hostname
	int address_length;                     ///< The length of the resolved address
	sockaddr_storage address;               ///< The resolved address
	bool connect_blocking;                  ///< Whether Connect() should be blocking or not.
	int connect_bind_address_length;        ///< The length of connect_bind_address.
	sockaddr_storage connect_bind_address;  ///< Where to bind the connecting socket to.
	bool resolved;                          ///< Whether the address has been (tried to be) resolved

	/**
	 * Helper function to resolve something to a socket.
	 * @param runp information about the socket to try not
	 * @param source the source NetworkAddress being resolved
	 * @return the opened socket or INVALID_SOCKET
	 */
	typedef SOCKET (*LoopProc)(addrinfo *runp, NetworkAddress *source);

	SOCKET Resolve(int family, int socktype, int flags, SocketList *sockets, LoopProc func);

public:
	/**
	 * Create a network address based on a resolved IP and port.
	 * @param address The IP address with port.
	 * @param address_length The length of the address.
	 */
	NetworkAddress(struct sockaddr_storage &address, int address_length) :
		address_length(address_length),
		address(address),
		connect_blocking(true),
		connect_bind_address_length(0),
		resolved(address_length != 0)
	{
		*this->hostname = '\0';
	}

	/**
	 * Create a network address based on a resolved IP and port.
	 * @param address The IP address with port.
	 * @param address_length The length of the address.
	 */
	NetworkAddress(sockaddr *address, int address_length) :
		address_length(address_length),
		connect_blocking(true),
		connect_bind_address_length(0),
		resolved(address_length != 0)
	{
		*this->hostname = '\0';
		memset(&this->address, 0, sizeof(this->address));
		memcpy(&this->address, address, address_length);
	}

	/**
	 * Create a network address based on a unresolved host and port
	 * @param hostname the unresolved hostname
	 * @param port the port
	 * @param family the address family
	 */
	NetworkAddress(const char *hostname = "", uint16 port = 0, int family = AF_UNSPEC) :
		address_length(0),
		connect_blocking(true),
		connect_bind_address_length(0),
		resolved(false)
	{
		/* Also handle IPv6 bracket enclosed hostnames */
		if (StrEmpty(hostname)) hostname = "";
		if (*hostname == '[') hostname++;
		strecpy(this->hostname, StrEmpty(hostname) ? "" : hostname, lastof(this->hostname));
		char *tmp = strrchr(this->hostname, ']');
		if (tmp != nullptr) *tmp = '\0';

		memset(&this->address, 0, sizeof(this->address));
		this->address.ss_family = family;
		this->SetPort(port);
	}

	const char *GetHostname();
	void GetAddressAsString(char *buffer, const char *last, bool with_family = true);
	std::string GetAddressAsString(bool with_family = true);
	const sockaddr_storage *GetAddress();

	/**
	 * Get the (valid) length of the address.
	 * @return the length
	 */
	int GetAddressLength()
	{
		/* Resolve it if we didn't do it already */
		if (!this->IsResolved()) this->GetAddress();
		return this->address_length;
	}

	uint16 GetPort() const;
	void SetPort(uint16 port);

	/**
	 * Set if Connect() should be blocking or not.
	 */
	void SetConnectBlocking(bool blocking) { this->connect_blocking = blocking; }

	/** Get whether Connect() should be blocking. */
	bool GetConnectBlocking() { return this->connect_blocking; }

	/**
	 * Set if and to what address Connect() should bind before connecting.
	 */
	void SetConnectBindAddress(NetworkAddress bind) {
		this->connect_bind_address = *bind.GetAddress();
		this->connect_bind_address_length = bind.GetAddressLength();
	}

	/**
	 * Get the bind address for Connect().
	 */
	void GetConnectBindAddress(const sockaddr **bind_address, int *bind_address_length) {
		*bind_address = (sockaddr *)&this->connect_bind_address;
		*bind_address_length = this->connect_bind_address_length;
	}

	/**
	 * Check whether the IP address has been resolved already
	 * @return true iff the port has been resolved
	 */
	bool IsResolved() const
	{
		return this->resolved;
	}

	bool IsFamily(int family);
	bool IsInNetmask(const char *netmask);

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return < 0 if address is less, 0 if equal and > 0 if address is more
	 */
	int CompareTo(NetworkAddress &address)
	{
		int r = this->GetAddressLength() - address.GetAddressLength();
		if (r == 0) r = this->address.ss_family - address.address.ss_family;
		if (r == 0) r = memcmp(&this->address, &address.address, this->address_length);
		if (r == 0) r = this->GetPort() - address.GetPort();
		return r;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return true if both match.
	 */
	bool operator == (NetworkAddress &address)
	{
		return this->CompareTo(address) == 0;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return true if both match.
	 */
	bool operator == (NetworkAddress &address) const
	{
		return const_cast<NetworkAddress *>(this)->CompareTo(address) == 0;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return true if both do not match.
	 */
	bool operator != (NetworkAddress address) const
	{
		return const_cast<NetworkAddress *>(this)->CompareTo(address) != 0;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 */
	bool operator < (NetworkAddress &address)
	{
		return this->CompareTo(address) < 0;
	}

	SOCKET Connect();
	void Listen(int socktype, SocketList *sockets);

	static const char *SocketTypeAsString(int socktype);
	static const char *AddressFamilyAsString(int family);
};

/**
 * Address of a server, which can either be a direct address or a join-key.
 */
class ServerAddress {
public:
	NetworkAddress direct_address;          ///< Is this server a direct IP:port address.
	char join_key[NETWORK_JOIN_KEY_LENGTH]; ///< Is this server identified with a join_key.

	ServerAddress(NetworkAddress address) : direct_address(address)
	{
		*this->join_key = '\0';
	}

	ServerAddress(const char *hostname, uint16 port) : direct_address(NetworkAddress(hostname, port))
	{
		*this->join_key = '\0';
	}

	ServerAddress(const char *join_key)
	{
		strecpy(this->join_key, join_key, lastof(this->join_key));
	}

	bool IsDirectAddress()
	{
		return StrEmpty(join_key);
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return < 0 if address is less, 0 if equal and > 0 if address is more
	 */
	int CompareTo(ServerAddress &address)
	{
		int r = this->IsDirectAddress() - address.IsDirectAddress();
		if (r == 0) {
			if (this->IsDirectAddress()) {
				r = this->direct_address.CompareTo(address.direct_address);
			} else {
				r = strcmp(this->join_key, address.join_key);
			}
		}
		return r;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return true if both match.
	 */
	bool operator == (ServerAddress &address)
	{
		return this->CompareTo(address) == 0;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return true if both match.
	 */
	bool operator == (ServerAddress &address) const
	{
		return const_cast<ServerAddress *>(this)->CompareTo(address) == 0;
	}
};

#endif /* NETWORK_CORE_ADDRESS_H */
