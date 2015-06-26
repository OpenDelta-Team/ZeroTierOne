/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_N_PACKET_HPP
#define ZT_N_PACKET_HPP

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <string>
#include <iostream>

#include "Constants.hpp"

#include "Address.hpp"
#include "Poly1305.hpp"
#include "Salsa20.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"

#include "../ext/lz4/lz4.h"

/**
 * Protocol version -- incremented only for MAJOR changes
 *
 * 1 - 0.2.0 ... 0.2.5
 * 2 - 0.3.0 ... 0.4.5
 *   * Added signature and originating peer to multicast frame
 *   * Double size of multicast frame bloom filter
 * 3 - 0.5.0 ... 0.6.0
 *   * Yet another multicast redesign
 *   * New crypto completely changes key agreement cipher
 * 4 - 0.6.0 ... CURRENT
 *   * New identity format based on hashcash design
 *
 * This isn't going to change again for a long time unless your
 * author wakes up again at 4am with another great idea. :P
 */
#define ZT_PROTO_VERSION 4

/**
 * Minimum supported protocol version
 */
#define ZT_PROTO_VERSION_MIN 4

/**
 * Maximum hop count allowed by packet structure (3 bits, 0-7)
 * 
 * This is not necessarily the maximum hop counter after which
 * relaying is no longer performed.
 */
#define ZT_PROTO_MAX_HOPS 7

/**
 * Cipher suite: Curve25519/Poly1305/Salsa20/12 without payload encryption
 *
 * This specifies Poly1305 MAC using a 32-bit key derived from the first
 * 32 bytes of a Salsa20/12 keystream as in the Salsa20/12 cipher suite,
 * but the payload is not encrypted. This is currently only used to send
 * HELLO since that's the public key specification packet and must be
 * sent in the clear. Key agreement is performed using Curve25519 elliptic
 * curve Diffie-Hellman.
 */
#define ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_NONE 0

/**
 * Cipher suite: Curve25519/Poly1305/Salsa20/12
 *
 * This specifies Poly1305 using the first 32 bytes of a Salsa20/12 key
 * stream as its one-time-use key followed by payload encryption with
 * the remaining Salsa20/12 key stream. Key agreement is performed using
 * Curve25519 elliptic curve Diffie-Hellman.
 */
#define ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_SALSA2012 1

/**
 * Cipher suite: PFS negotiated ephemeral cipher suite and authentication
 *
 * This message is encrypted with the latest negotiated ephemeral (PFS)
 * key pair and cipher suite. If authentication fails, VERB_SET_EPHEMERAL_KEY
 * may be sent to renegotiate ephemeral keys. To prevent attacks, this
 * attempted renegotiation should be limited to some sane rate such as
 * once per second.
 */
#define ZT_PROTO_CIPHER_SUITE__EPHEMERAL 7

/**
 * DEPRECATED payload encrypted flag, will be removed for re-use soon.
 *
 * This has been replaced by the two-bit cipher suite selection field where
 * a value of 0 indicated unencrypted (but authenticated) messages.
 */
#define ZT_PROTO_FLAG_ENCRYPTED 0x80

/**
 * Header flag indicating that a packet is fragmented
 *
 * If this flag is set, the receiver knows to expect more than one fragment.
 * See Packet::Fragment for details.
 */
#define ZT_PROTO_FLAG_FRAGMENTED 0x40

/**
 * Verb flag indicating payload is compressed with LZ4
 */
#define ZT_PROTO_VERB_FLAG_COMPRESSED 0x80

/**
 * Rounds used for Salsa20 encryption in ZT
 */
#define ZT_PROTO_SALSA20_ROUNDS 12

// Indices of fields in normal packet header -- do not change as this
// might require both code rework and will break compatibility.
#define ZT_PACKET_IDX_IV 0
#define ZT_PACKET_IDX_DEST 8
#define ZT_PACKET_IDX_SOURCE 13
#define ZT_PACKET_IDX_FLAGS 18
#define ZT_PACKET_IDX_MAC 19
#define ZT_PACKET_IDX_VERB 27
#define ZT_PACKET_IDX_PAYLOAD 28

/**
 * Packet buffer size (can be changed)
 */
#define ZT_PROTO_MAX_PACKET_LENGTH (ZT_MAX_PACKET_FRAGMENTS * ZT_UDP_DEFAULT_PAYLOAD_MTU)

/**
 * Minimum viable packet length (also length of header)
 */
#define ZT_PROTO_MIN_PACKET_LENGTH ZT_PACKET_IDX_PAYLOAD

// Indexes of fields in fragment header -- also can't be changed without
// breaking compatibility.
#define ZT_PACKET_FRAGMENT_IDX_PACKET_ID 0
#define ZT_PACKET_FRAGMENT_IDX_DEST 8
#define ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR 13
#define ZT_PACKET_FRAGMENT_IDX_FRAGMENT_NO 14
#define ZT_PACKET_FRAGMENT_IDX_HOPS 15
#define ZT_PACKET_FRAGMENT_IDX_PAYLOAD 16

/**
 * Value found at ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR in fragments
 */
#define ZT_PACKET_FRAGMENT_INDICATOR ZT_ADDRESS_RESERVED_PREFIX

/**
 * Minimum viable fragment length
 */
#define ZT_PROTO_MIN_FRAGMENT_LENGTH ZT_PACKET_FRAGMENT_IDX_PAYLOAD

/**
 * Length of LAN beacon packets
 */
#define ZT_PROTO_BEACON_LENGTH 13

/**
 * Index of address in a LAN beacon
 */
#define ZT_PROTO_BEACON_IDX_ADDRESS 8

// Destination address types from HELLO and OK(HELLO)
#define ZT_PROTO_DEST_ADDRESS_TYPE_NONE 0
#define ZT_PROTO_DEST_ADDRESS_TYPE_ETHERNET 1
#define ZT_PROTO_DEST_ADDRESS_TYPE_IPV4 4
#define ZT_PROTO_DEST_ADDRESS_TYPE_IPV6 6

// Ephemeral key record flags
#define ZT_PROTO_EPHEMERAL_KEY_FLAG_FIPS 0x01

// Ephemeral key record symmetric cipher types
#define ZT_PROTO_EPHEMERAL_KEY_SYMMETRIC_CIPHER_SALSA2012_POLY1305 0x01
#define ZT_PROTO_EPHEMERAL_KEY_SYMMETRIC_CIPHER_AES256_GCM 0x02

// Ephemeral key record public key types
#define ZT_PROTO_EPHEMERAL_KEY_PK_C25519 0x01
#define ZT_PROTO_EPHEMERAL_KEY_PK_NISTP256 0x02

// Field incides for parsing verbs -------------------------------------------

// Some verbs have variable-length fields. Those aren't fully defined here
// yet-- instead they are parsed using relative indexes in IncomingPacket.
// See their respective handler functions.

#define ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION (ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION + 1)
#define ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION (ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION + 1)
#define ZT_PROTO_VERB_HELLO_IDX_REVISION (ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION + 1)
#define ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP (ZT_PROTO_VERB_HELLO_IDX_REVISION + 2)
#define ZT_PROTO_VERB_HELLO_IDX_IDENTITY (ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP + 8)

#define ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID (ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB + 1)
#define ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE (ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID + 8)
#define ZT_PROTO_VERB_ERROR_IDX_PAYLOAD (ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE + 1)

#define ZT_PROTO_VERB_OK_IDX_IN_RE_VERB (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID (ZT_PROTO_VERB_OK_IDX_IN_RE_VERB + 1)
#define ZT_PROTO_VERB_OK_IDX_PAYLOAD (ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID + 8)

#define ZT_PROTO_VERB_WHOIS_IDX_ZTADDRESS (ZT_PACKET_IDX_PAYLOAD)

#define ZT_PROTO_VERB_RENDEZVOUS_IDX_FLAGS (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS (ZT_PROTO_VERB_RENDEZVOUS_IDX_FLAGS + 1)
#define ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT (ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS + 5)
#define ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN (ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT + 2)
#define ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS (ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN + 1)

#define ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE (ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_FRAME_IDX_PAYLOAD (ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE + 2)

#define ZT_PROTO_VERB_EXT_FRAME_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_NETWORK_ID 8
#define ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS (ZT_PROTO_VERB_EXT_FRAME_IDX_NETWORK_ID + ZT_PROTO_VERB_EXT_FRAME_LEN_NETWORK_ID)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_FLAGS 1
#define ZT_PROTO_VERB_EXT_FRAME_IDX_COM (ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS + ZT_PROTO_VERB_EXT_FRAME_LEN_FLAGS)
#define ZT_PROTO_VERB_EXT_FRAME_IDX_TO (ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS + ZT_PROTO_VERB_EXT_FRAME_LEN_FLAGS)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_TO 6
#define ZT_PROTO_VERB_EXT_FRAME_IDX_FROM (ZT_PROTO_VERB_EXT_FRAME_IDX_TO + ZT_PROTO_VERB_EXT_FRAME_LEN_TO)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_FROM 6
#define ZT_PROTO_VERB_EXT_FRAME_IDX_ETHERTYPE (ZT_PROTO_VERB_EXT_FRAME_IDX_FROM + ZT_PROTO_VERB_EXT_FRAME_LEN_FROM)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_ETHERTYPE 2
#define ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD (ZT_PROTO_VERB_EXT_FRAME_IDX_ETHERTYPE + ZT_PROTO_VERB_EXT_FRAME_LEN_ETHERTYPE)

#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN + 2)

#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS + 1)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC + 6)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI + 4)

// Note: COM, GATHER_LIMIT, and SOURCE_MAC are optional, and so are specified without size
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FLAGS (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_COM (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FLAGS + 1)
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_GATHER_LIMIT (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FLAGS + 1)
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SOURCE_MAC (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FLAGS + 1)
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_MAC (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FLAGS + 1)
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_ADI (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_MAC + 6)
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_ADI + 4)
#define ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE + 2)

#define ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP (ZT_PROTO_VERB_OK_IDX_PAYLOAD)
#define ZT_PROTO_VERB_HELLO__OK__IDX_PROTOCOL_VERSION (ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP + 8)
#define ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION (ZT_PROTO_VERB_HELLO__OK__IDX_PROTOCOL_VERSION + 1)
#define ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION (ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION + 1)
#define ZT_PROTO_VERB_HELLO__OK__IDX_REVISION (ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION + 1)

#define ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY (ZT_PROTO_VERB_OK_IDX_PAYLOAD)

#define ZT_PROTO_VERB_WHOIS__ERROR__IDX_ZTADDRESS (ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)

#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_NETWORK_ID (ZT_PROTO_VERB_OK_IDX_PAYLOAD)
#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT_LEN (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT_LEN + 2)

#define ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_NETWORK_ID (ZT_PROTO_VERB_OK_IDX_PAYLOAD)
#define ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_MAC (ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_ADI (ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_MAC + 6)
#define ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS (ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_ADI + 4)

#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_NETWORK_ID (ZT_PROTO_VERB_OK_IDX_PAYLOAD)
#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_MAC (ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_ADI (ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_MAC + 6)
#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_FLAGS (ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_ADI + 4)
#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_COM_AND_GATHER_RESULTS (ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_FLAGS + 1)

// ---------------------------------------------------------------------------

namespace ZeroTier {

/**
 * ZeroTier packet
 * 
 * Packet format:
 *   <[8] random initialization vector (doubles as 64-bit packet ID)>
 *   <[5] destination ZT address>
 *   <[5] source ZT address>
 *   <[1] flags/cipher (top 5 bits) and ZT hop count (last 3 bits)>
 *   <[8] 8-bit MAC (currently first 8 bytes of poly1305 tag)>
 *   [... -- begin encryption envelope -- ...]
 *   <[1] encrypted flags (top 3 bits) and verb (last 5 bits)>
 *   [... verb-specific payload ...]
 * 
 * Packets smaller than 28 bytes are invalid and silently discarded.
 *
 * The flags/cipher/hops bit field is: FFCCCHHH where C is a 3-bit cipher
 * selection allowing up to 7 cipher suites, F is outside-envelope flags,
 * and H is hop count.
 *
 * The three-bit hop count is the only part of a packet that is mutable in
 * transit without invalidating the MAC. All other bits in the packet are
 * immutable. This is because intermediate nodes can increment the hop
 * count up to 7 (protocol max).
 *
 * http://tonyarcieri.com/all-the-crypto-code-youve-ever-written-is-probably-broken
 *
 * For unencrypted packets, MAC is computed on plaintext. Only HELLO is ever
 * sent in the clear, as it's the "here is my public key" message.
 *
 * Beacon format and beacon packets:
 *   <[8] 8 random bytes>
 *   <[5] sender ZT address>
 *
 * A beacon is a 13-byte packet containing only the address of the sender.
 * Receiving peers may or may not respond to beacons with a HELLO or other
 * message to initiate direct communication.
 *
 * Beacons may be used for direct LAN announcement or NAT traversal.
 */
class Packet : public Buffer<ZT_PROTO_MAX_PACKET_LENGTH>
{
public:
	/**
	 * A packet fragment
	 * 
	 * Fragments are sent if a packet is larger than UDP MTU. The first fragment
	 * is sent with its normal header with the fragmented flag set. Remaining
	 * fragments are sent this way.
	 * 
	 * The fragmented bit indicates that there is at least one fragment. Fragments
	 * themselves contain the total, so the receiver must "learn" this from the
	 * first fragment it receives.
	 * 
	 * Fragments are sent with the following format:
	 *   <[8] packet ID of packet whose fragment this belongs to>
	 *   <[5] destination ZT address>
	 *   <[1] 0xff, a reserved address, signals that this isn't a normal packet>
	 *   <[1] total fragments (most significant 4 bits), fragment no (LS 4 bits)>
	 *   <[1] ZT hop count (top 5 bits unused and must be zero)>
	 *   <[...] fragment data>
	 *
	 * The protocol supports a maximum of 16 fragments. If a fragment is received
	 * before its main packet header, it should be cached for a brief period of
	 * time to see if its parent arrives. Loss of any fragment constitutes packet
	 * loss; there is no retransmission mechanism. The receiver must wait for full
	 * receipt to authenticate and decrypt; there is no per-fragment MAC. (But if
	 * fragments are corrupt, the MAC will fail for the whole assembled packet.)
	 */
	class Fragment : public Buffer<ZT_PROTO_MAX_PACKET_LENGTH>
	{
	public:
		Fragment() :
			Buffer<ZT_PROTO_MAX_PACKET_LENGTH>()
		{
		}

		template<unsigned int C2>
		Fragment(const Buffer<C2> &b)
	 		throw(std::out_of_range) :
	 		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(b)
		{
		}

		Fragment(const void *data,unsigned int len) :
			Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(data,len)
		{
		}

		/**
		 * Initialize from a packet
		 * 
		 * @param p Original assembled packet
		 * @param fragStart Start of fragment (raw index in packet data)
		 * @param fragLen Length of fragment in bytes
		 * @param fragNo Which fragment (>= 1, since 0 is Packet with end chopped off)
		 * @param fragTotal Total number of fragments (including 0)
		 * @throws std::out_of_range Packet size would exceed buffer
		 */
		Fragment(const Packet &p,unsigned int fragStart,unsigned int fragLen,unsigned int fragNo,unsigned int fragTotal)
			throw(std::out_of_range)
		{
			init(p,fragStart,fragLen,fragNo,fragTotal);
		}

		/**
		 * Initialize from a packet
		 * 
		 * @param p Original assembled packet
		 * @param fragStart Start of fragment (raw index in packet data)
		 * @param fragLen Length of fragment in bytes
		 * @param fragNo Which fragment (>= 1, since 0 is Packet with end chopped off)
		 * @param fragTotal Total number of fragments (including 0)
		 * @throws std::out_of_range Packet size would exceed buffer
		 */
		inline void init(const Packet &p,unsigned int fragStart,unsigned int fragLen,unsigned int fragNo,unsigned int fragTotal)
			throw(std::out_of_range)
		{
			if ((fragStart + fragLen) > p.size())
				throw std::out_of_range("Packet::Fragment: tried to construct fragment of packet past its length");
			setSize(fragLen + ZT_PROTO_MIN_FRAGMENT_LENGTH);

			// NOTE: this copies both the IV/packet ID and the destination address.
			memcpy(field(ZT_PACKET_FRAGMENT_IDX_PACKET_ID,13),p.field(ZT_PACKET_IDX_IV,13),13);

			(*this)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] = ZT_PACKET_FRAGMENT_INDICATOR;
			(*this)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_NO] = (char)(((fragTotal & 0xf) << 4) | (fragNo & 0xf));
			(*this)[ZT_PACKET_FRAGMENT_IDX_HOPS] = 0;

			memcpy(field(ZT_PACKET_FRAGMENT_IDX_PAYLOAD,fragLen),p.field(fragStart,fragLen),fragLen);
		}

		/**
		 * Get this fragment's destination
		 * 
		 * @return Destination ZT address
		 */
		inline Address destination() const { return Address(field(ZT_PACKET_FRAGMENT_IDX_DEST,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

		/**
		 * @return True if fragment is of a valid length
		 */
		inline bool lengthValid() const { return (size() >= ZT_PACKET_FRAGMENT_IDX_PAYLOAD); }

		/**
		 * @return ID of packet this is a fragment of
		 */
		inline uint64_t packetId() const { return at<uint64_t>(ZT_PACKET_FRAGMENT_IDX_PACKET_ID); }

		/**
		 * @return Total number of fragments in packet
		 */
		inline unsigned int totalFragments() const { return (((unsigned int)((*this)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_NO]) >> 4) & 0xf); }

		/**
		 * @return Fragment number of this fragment
		 */
		inline unsigned int fragmentNumber() const { return ((unsigned int)((*this)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_NO]) & 0xf); }

		/**
		 * @return Fragment ZT hop count
		 */
		inline unsigned int hops() const { return (unsigned int)((*this)[ZT_PACKET_FRAGMENT_IDX_HOPS]); }

		/**
		 * Increment this packet's hop count
		 */
		inline void incrementHops()
		{
			(*this)[ZT_PACKET_FRAGMENT_IDX_HOPS] = (((*this)[ZT_PACKET_FRAGMENT_IDX_HOPS]) + 1) & ZT_PROTO_MAX_HOPS;
		}

		/**
		 * @return Length of payload in bytes
		 */
		inline unsigned int payloadLength() const { return ((size() > ZT_PACKET_FRAGMENT_IDX_PAYLOAD) ? (size() - ZT_PACKET_FRAGMENT_IDX_PAYLOAD) : 0); }

		/**
		 * @return Raw packet payload
		 */
		inline const unsigned char *payload() const
		{
			return field(ZT_PACKET_FRAGMENT_IDX_PAYLOAD,size() - ZT_PACKET_FRAGMENT_IDX_PAYLOAD);
		}
	};

	/**
	 * ZeroTier protocol verbs
	 */
	enum Verb /* Max value: 32 (5 bits) */
	{
		/* No operation, payload ignored, no reply */
		VERB_NOP = 0,

		/* Announcement of a node's existence:
		 *   <[1] protocol version>
		 *   <[1] software major version>
		 *   <[1] software minor version>
		 *   <[2] software revision>
		 *   <[8] timestamp (ms since epoch)>
		 *   <[...] binary serialized identity (see Identity)>
		 *   <[1] destination address type>
		 *   [<[...] destination address>]
		 *
		 * This is the only message that ever must be sent in the clear, since it
		 * is used to push an identity to a new peer.
		 *
		 * The destination address is the wire address to which this packet is
		 * being sent, and in OK is *also* the destination address of the OK
		 * packet. This can be used by the receiver to detect NAT, learn its real
		 * external address if behind NAT, and detect changes to its external
		 * address that require re-establishing connectivity.
		 *
		 * Destination address types and formats (not all of these are used now):
		 *   0 - None -- no destination address data present
		 *   1 - Ethernet address -- format: <[6] Ethernet MAC>
		 *   4 - 6-byte IPv4 UDP address/port -- format: <[4] IP>, <[2] port>
		 *   6 - 18-byte IPv6 UDP address/port -- format: <[16] IP>, <[2] port>
		 *
		 * OK payload:
		 *   <[8] timestamp (echoed from original HELLO)>
		 *   <[1] protocol version (of responder)>
		 *   <[1] software major version (of responder)>
		 *   <[1] software minor version (of responder)>
		 *   <[2] software revision (of responder)>
		 *   <[1] destination address type (for this OK, not copied from HELLO)>
		 *   [<[...] destination address>]
		 *
		 * ERROR has no payload.
		 */
		VERB_HELLO = 1,

		/* Error response:
		 *   <[1] in-re verb>
		 *   <[8] in-re packet ID>
		 *   <[1] error code>
		 *   <[...] error-dependent payload>
		 */
		VERB_ERROR = 2,

		/* Success response:
		 *   <[1] in-re verb>
		 *   <[8] in-re packet ID>
		 *   <[...] request-specific payload>
		 */
		VERB_OK = 3,

		/* Query an identity by address:
		 *   <[5] address to look up>
		 *
		 * OK response payload:
		 *   <[...] binary serialized identity>
		 *
		 * ERROR response payload:
		 *   <[5] address>
		 */
		VERB_WHOIS = 4,

		/* Meet another node at a given protocol address:
		 *   <[1] flags (unused, currently 0)>
		 *   <[5] ZeroTier address of peer that might be found at this address>
		 *   <[2] 16-bit protocol address port>
		 *   <[1] protocol address length (4 for IPv4, 16 for IPv6)>
		 *   <[...] protocol address (network byte order)>
		 *
		 * This is sent by a relaying node to initiate NAT traversal between two
		 * peers that are communicating by way of indirect relay. The relay will
		 * send this to both peers at the same time on a periodic basis, telling
		 * each where it might find the other on the network.
		 *
		 * Upon receipt a peer sends HELLO to establish a direct link.
		 *
		 * Nodes should implement rate control, limiting the rate at which they
		 * respond to these packets to prevent their use in DDOS attacks. Nodes
		 * may also ignore these messages if a peer is not known or is not being
		 * actively communicated with.
		 *
		 * No OK or ERROR is generated.
		 */
		VERB_RENDEZVOUS = 5,

		/* ZT-to-ZT unicast ethernet frame (shortened EXT_FRAME):
		 *   <[8] 64-bit network ID>
		 *   <[2] 16-bit ethertype>
		 *   <[...] ethernet payload>
		 *
		 * MAC addresses are derived from the packet's source and destination
		 * ZeroTier addresses. This is a shortened EXT_FRAME that elides full
		 * Ethernet framing and other optional flags and features when they
		 * are not necessary.
		 *
		 * ERROR may be generated if a membership certificate is needed for a
		 * closed network. Payload will be network ID.
		 */
		VERB_FRAME = 6,

		/* Full Ethernet frame with MAC addressing and optional fields:
		 *   <[8] 64-bit network ID>
		 *   <[1] flags>
		 *  [<[...] certificate of network membership>]
		 *   <[6] destination MAC or all zero for destination node>
		 *   <[6] source MAC or all zero for node of origin>
		 *   <[2] 16-bit ethertype>
		 *   <[...] ethernet payload>
		 *
		 * Flags:
		 *   0x01 - Certificate of network membership is attached
		 *
		 * An extended frame carries full MAC addressing, making them a
		 * superset of VERB_FRAME. They're used for bridging or when we
		 * want to attach a certificate since FRAME does not support that.
		 *
		 * Multicast frames may not be sent as EXT_FRAME.
		 *
		 * ERROR may be generated if a membership certificate is needed for a
		 * closed network. Payload will be network ID.
		 */
		VERB_EXT_FRAME = 7,

		/* DEPRECATED */
		VERB_P5_MULTICAST_FRAME = 8,

		/* Announce interest in multicast group(s):
		 *   <[8] 64-bit network ID>
		 *   <[6] multicast Ethernet address>
		 *   <[4] multicast additional distinguishing information (ADI)>
		 *   [... additional tuples of network/address/adi ...]
		 *
		 * LIKEs are sent to peers with whom you have a direct peer to peer
		 * connection, and always including root servers.
		 *
		 * OK/ERROR are not generated.
		 */
		VERB_MULTICAST_LIKE = 9,

		/* Network member certificate replication/push:
		 *   <[...] serialized certificate of membership>
		 *   [ ... additional certificates may follow ...]
		 *
		 * Certificate contains network ID, peer it was issued for, etc.
		 *
		 * OK/ERROR are not generated.
		 */
		VERB_NETWORK_MEMBERSHIP_CERTIFICATE = 10,

		/* Network configuration request:
		 *   <[8] 64-bit network ID>
		 *   <[2] 16-bit length of request meta-data dictionary>
		 *   <[...] string-serialized request meta-data>
		 *  [<[8] 64-bit revision of netconf we currently have>]
		 *
		 * This message requests network configuration from a node capable of
		 * providing it. If the optional revision is included, a response is
		 * only generated if there is a newer network configuration available.
		 *
		 * OK response payload:
		 *   <[8] 64-bit network ID>
		 *   <[2] 16-bit length of network configuration dictionary>
		 *   <[...] network configuration dictionary>
		 *
		 * OK returns a Dictionary (string serialized) containing the network's
		 * configuration and IP address assignment information for the querying
		 * node. It also contains a membership certificate that the querying
		 * node can push to other peers to demonstrate its right to speak on
		 * a given network.
		 *
		 * When a new network configuration is received, another config request
		 * should be sent with the new netconf's revision. This confirms receipt
		 * and also causes any subsequent changes to rapidly propagate as this
		 * cycle will repeat until there are no changes. This is optional but
		 * recommended behavior.
		 *
		 * ERROR response payload:
		 *   <[8] 64-bit network ID>
		 *
		 * UNSUPPORTED_OPERATION is returned if this service is not supported,
		 * and OBJ_NOT_FOUND if the queried network ID was not found.
		 */
		VERB_NETWORK_CONFIG_REQUEST = 11,

		/* Network configuration refresh request:
		 *   <[...] array of 64-bit network IDs>
		 *
		 * This message can be sent by the network configuration master node
		 * to request that nodes refresh their network configuration. It can
		 * thus be used to "push" updates so that network config changes will
		 * take effect quickly.
		 *
		 * It does not generate an OK or ERROR message, and is treated only as
		 * a hint to refresh now.
		 */
		VERB_NETWORK_CONFIG_REFRESH = 12,

		/* Request endpoints for multicast distribution:
		 *   <[8] 64-bit network ID>
		 *   <[1] flags>
		 *   <[6] MAC address of multicast group being queried>
		 *   <[4] 32-bit ADI for multicast group being queried>
		 *   <[4] 32-bit requested max number of multicast peers>
		 *  [<[...] network certificate of membership>]
		 *
		 * Flags:
		 *   0x01 - Network certificate of membership is attached
		 *
		 * This message asks a peer for additional known endpoints that have
		 * LIKEd a given multicast group. It's sent when the sender wishes
		 * to send multicast but does not have the desired number of recipient
		 * peers.
		 *
		 * OK response payload:
		 *   <[8] 64-bit network ID>
		 *   <[6] MAC address of multicast group being queried>
		 *   <[4] 32-bit ADI for multicast group being queried>
		 *   [begin gather results -- these same fields can be in OK(MULTICAST_FRAME)]
		 *   <[4] 32-bit total number of known members in this multicast group>
		 *   <[2] 16-bit number of members enumerated in this packet>
		 *   <[...] series of 5-byte ZeroTier addresses of enumerated members>
		 *
		 * If no endpoints are known, OK and ERROR are both optional. It's okay
		 * to return nothing in that case since gathering is "lazy."
		 *
		 * ERROR response payload:
		 *   <[8] 64-bit network ID>
		 *   <[6] MAC address of multicast group being queried>
		 *   <[4] 32-bit ADI for multicast group being queried>
		 *
		 * ERRORs are optional and are only generated if permission is denied,
		 * certificate of membership is out of date, etc.
		 */
		VERB_MULTICAST_GATHER = 13,

		/* Multicast frame:
		 *   <[8] 64-bit network ID>
		 *   <[1] flags>
		 *  [<[...] network certificate of membership>]
		 *  [<[4] 32-bit implicit gather limit>]
		 *  [<[6] source MAC>]
		 *   <[6] destination MAC (multicast address)>
		 *   <[4] 32-bit multicast ADI (multicast address extension)>
		 *   <[2] 16-bit ethertype>
		 *   <[...] ethernet payload>
		 *
		 * Flags:
		 *   0x01 - Network certificate of membership is attached
		 *   0x02 - Implicit gather limit field is present
		 *   0x04 - Source MAC is specified -- otherwise it's computed from sender
		 *
		 * OK and ERROR responses are optional. OK may be generated if there are
		 * implicit gather results or if the recipient wants to send its own
		 * updated certificate of network membership to the sender. ERROR may be
		 * generated if a certificate is needed or if multicasts to this group
		 * are no longer wanted (multicast unsubscribe).
		 *
		 * OK response payload:
		 *   <[8] 64-bit network ID>
		 *   <[6] MAC address of multicast group>
		 *   <[4] 32-bit ADI for multicast group>
		 *   <[1] flags>
		 *  [<[...] network certficate of membership>]
		 *  [<[...] implicit gather results if flag 0x01 is set>]
		 *
		 * OK flags (same bits as request flags):
		 *   0x01 - OK includes certificate of network membership
		 *   0x02 - OK includes implicit gather results
		 *
		 * ERROR response payload:
		 *   <[8] 64-bit network ID>
		 *   <[6] multicast group MAC>
		 *   <[4] 32-bit multicast group ADI>
		 */
		VERB_MULTICAST_FRAME = 14,

		/* Ephemeral (PFS) key push:
		 *   <[2] flags (unused and reserved, must be 0)>
		 *   <[2] length of padding / extra field section>
		 *   <[...] padding / extra field section>
		 *   <[8] 64-bit PFS key set ID sender holds for recipient (0==none)>
		 *   <[8] 64-bit PFS key set ID of this key set>
		 *   [... begin PFS key record ...]
		 *   <[1] flags>
		 *   <[1] symmetric cipher ID>
		 *   <[1] public key type ID>
		 *   <[2] public key length in bytes>
		 *   <[...] public key>
		 *   [... additional records may follow up to max packet length ...]
		 *
		 * This message is sent to negotiate an ephemeral key. If the recipient's
		 * current key pair for the sender does not match the one the sender
		 * claims to have on file, it must respond with its own SET_EPHEMERAL_KEY.
		 *
		 * PFS key IDs are random and must not be zero, since zero indicates that
		 * the sender does not have an ephemeral key on file for the recipient.
		 *
		 * One or more records may be sent. If multiple records are present,
		 * the first record with common symmetric cipher, public key type,
		 * and relevant flags must be used.
		 *
		 * The padding section may be filled with an arbitrary amount of random
		 * or empty payload. This may be used as a countermeasure to prevent PFS
		 * key pushes from being recognized by packet size vs. other packets in
		 * the stream. This also provides potential space for additional fields
		 * that might be indicated in the future by flags.
		 *
		 * Flags (all unspecified flags must be zero):
		 *   0x01 - FIPS mode, only use record if FIPS compliant crypto in use
		 *
		 * Symmetric cipher IDs:
		 *   0x01 - Salsa20/12 with Poly1305 authentication (ZT default)
		 *   0x02 - AES256-GCM combined crypto and authentication
		 *
		 * Public key types:
		 *   0x01 - Curve25519 ECDH with SHA-512 KDF
		 *   0x02 - NIST P-256 ECDH with SHA-512 KDF
		 *
		 * Once both peers have a PFS key, they will attempt to send PFS key
		 * encrypted messages with the PFS flag set using the negotiated
		 * cipher/auth type.
		 *
		 * Note: most of these features such as FIPS and other cipher suites are
		 * not implemented yet. They're just specified in the protocol for future
		 * use to support e.g. FIPS requirements.
		 *
		 * OK response payload:
		 *   <[8] PFS key set ID of received key set>
		 *   <[1] index in record list of chosen key record>
		 */
		VERB_SET_EPHEMERAL_KEY = 15,

		/* "Call me at" -- push of potential endpoints for direct communication:
		 *   <[1] flags>
		 *   <[2] number of addresses>
		 *   <[...] address types and addresses>
		 *
		 * Address types and addresses are of the same format as the destination
		 * address type and address in HELLO.
		 *
		 * The receiver may, upon receiving a CMA push, attempt to establish a
		 * direct link to one or more of the indicated addresses. Senders should
		 * only send CMA pushes to peers that they have some relationship
		 * with such as a shared network membership or a mutual trust.
		 *
		 * OK/ERROR are not generated.
		 */
		VERB_CMA = 16
	};

	/**
	 * Error codes for VERB_ERROR
	 */
	enum ErrorCode
	{
		/* No error, not actually used in transit */
		ERROR_NONE = 0,

		/* Invalid request */
		ERROR_INVALID_REQUEST = 1,

		/* Bad/unsupported protocol version */
		ERROR_BAD_PROTOCOL_VERSION = 2,

		/* Unknown object queried (e.g. with WHOIS) */
		ERROR_OBJ_NOT_FOUND = 3,

		/* HELLO pushed an identity whose address is already claimed */
		ERROR_IDENTITY_COLLISION = 4,

		/* Verb or use case not supported/enabled by this node */
		ERROR_UNSUPPORTED_OPERATION = 5,

		/* Message to private network rejected -- no unexpired certificate on file */
		ERROR_NEED_MEMBERSHIP_CERTIFICATE = 6,

		/* Tried to join network, but you're not a member */
		ERROR_NETWORK_ACCESS_DENIED_ = 7, /* extra _ to avoid Windows name conflict */

		/* Multicasts to this group are not wanted */
		ERROR_UNWANTED_MULTICAST = 8
	};

	/**
	 * @param v Verb
	 * @return String representation (e.g. HELLO, OK)
	 */
	static const char *verbString(Verb v)
		throw();

	/**
	 * @param e Error code
	 * @return String error name
	 */
	static const char *errorString(ErrorCode e)
		throw();

	template<unsigned int C2>
	Packet(const Buffer<C2> &b) :
 		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(b)
	{
	}

	Packet(const void *data,unsigned int len) :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(data,len)
	{
	}

	/**
	 * Construct a new empty packet with a unique random packet ID
	 * 
	 * Flags and hops will be zero. Other fields and data region are undefined.
	 * Use the header access methods (setDestination() and friends) to fill out
	 * the header. Payload should be appended; initial size is header size.
	 */
	Packet() :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(ZT_PROTO_MIN_PACKET_LENGTH)
	{
		Utils::getSecureRandom(field(ZT_PACKET_IDX_IV,8),8);
		(*this)[ZT_PACKET_IDX_FLAGS] = 0; // zero flags, cipher ID, and hops
	}

	/**
	 * Make a copy of a packet with a new initialization vector and destination address
	 *
	 * This can be used to take one draft prototype packet and quickly make copies to
	 * encrypt for different destinations.
	 *
	 * @param prototype Prototype packet
	 * @param dest Destination ZeroTier address for new packet
	 */
	Packet(const Packet &prototype,const Address &dest) :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(prototype)
	{
		Utils::getSecureRandom(field(ZT_PACKET_IDX_IV,8),8);
		setDestination(dest);
	}

	/**
	 * Construct a new empty packet with a unique random packet ID
	 * 
	 * @param dest Destination ZT address
	 * @param source Source ZT address
	 * @param v Verb
	 */
	Packet(const Address &dest,const Address &source,const Verb v) :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(ZT_PROTO_MIN_PACKET_LENGTH)
	{
		Utils::getSecureRandom(field(ZT_PACKET_IDX_IV,8),8);
		setDestination(dest);
		setSource(source);
		(*this)[ZT_PACKET_IDX_FLAGS] = 0; // zero flags and hops
		setVerb(v);
	}

	/**
	 * Reset this packet structure for reuse in place
	 * 
	 * @param dest Destination ZT address
	 * @param source Source ZT address
	 * @param v Verb
	 */
	inline void reset(const Address &dest,const Address &source,const Verb v)
	{
		setSize(ZT_PROTO_MIN_PACKET_LENGTH);
		Utils::getSecureRandom(field(ZT_PACKET_IDX_IV,8),8);
		setDestination(dest);
		setSource(source);
		(*this)[ZT_PACKET_IDX_FLAGS] = 0; // zero flags, cipher ID, and hops
		setVerb(v);
	}

	/**
	 * Generate a new IV / packet ID in place
	 *
	 * This can be used to re-use a packet buffer multiple times to send
	 * technically different but otherwise identical copies of the same
	 * packet.
	 */
	inline void newInitializationVector() { Utils::getSecureRandom(field(ZT_PACKET_IDX_IV,8),8); }

	/**
	 * Set this packet's destination
	 * 
	 * @param dest ZeroTier address of destination
	 */
	inline void setDestination(const Address &dest) { dest.copyTo(field(ZT_PACKET_IDX_DEST,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

	/**
	 * Set this packet's source
	 * 
	 * @param source ZeroTier address of source
	 */
	inline void setSource(const Address &source) { source.copyTo(field(ZT_PACKET_IDX_SOURCE,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

	/**
	 * Get this packet's destination
	 * 
	 * @return Destination ZT address
	 */
	inline Address destination() const { return Address(field(ZT_PACKET_IDX_DEST,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

	/**
	 * Get this packet's source
	 * 
	 * @return Source ZT address
	 */
	inline Address source() const { return Address(field(ZT_PACKET_IDX_SOURCE,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

	/**
	 * @return True if packet is of valid length
	 */
	inline bool lengthValid() const { return (size() >= ZT_PROTO_MIN_PACKET_LENGTH); }

	/**
	 * @return True if packet is fragmented (expect fragments)
	 */
	inline bool fragmented() const { return (((unsigned char)(*this)[ZT_PACKET_IDX_FLAGS] & ZT_PROTO_FLAG_FRAGMENTED) != 0); }

	/**
	 * Set this packet's fragmented flag
	 *
	 * @param f Fragmented flag value
	 */
	inline void setFragmented(bool f)
	{
		if (f)
			(*this)[ZT_PACKET_IDX_FLAGS] |= (char)ZT_PROTO_FLAG_FRAGMENTED;
		else (*this)[ZT_PACKET_IDX_FLAGS] &= (char)(~ZT_PROTO_FLAG_FRAGMENTED);
	}

	/**
	 * @return True if compressed (result only valid if unencrypted)
	 */
	inline bool compressed() const { return (((unsigned char)(*this)[ZT_PACKET_IDX_VERB] & ZT_PROTO_VERB_FLAG_COMPRESSED) != 0); }

	/**
	 * @return ZeroTier forwarding hops (0 to 7)
	 */
	inline unsigned int hops() const { return ((unsigned int)(*this)[ZT_PACKET_IDX_FLAGS] & 0x07); }

	/**
	 * Increment this packet's hop count
	 */
	inline void incrementHops()
	{
		unsigned char &b = (*this)[ZT_PACKET_IDX_FLAGS];
		b = (b & 0xf8) | ((b + 1) & 0x07);
	}

	/**
	 * @return Cipher suite selector: 0 - 7 (see #defines)
	 */
	inline unsigned int cipher() const
	{
		// Note: this uses the new cipher spec field, which is incompatible with <1.0.0 peers
		return (((unsigned int)(*this)[ZT_PACKET_IDX_FLAGS] & 0x38) >> 3);
	}

	/**
	 * Set this packet's cipher suite
	 */
	inline void setCipher(unsigned int c)
	{
		unsigned char &b = (*this)[ZT_PACKET_IDX_FLAGS];
		b = (b & 0xc7) | (unsigned char)((c << 3) & 0x38); // bits: FFCCCHHH
		// DEPRECATED "encrypted" flag -- used by pre-1.0.3 peers
		if (c == ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_SALSA2012)
			b |= ZT_PROTO_FLAG_ENCRYPTED;
		else b &= (~ZT_PROTO_FLAG_ENCRYPTED);
	}

	/**
	 * Get this packet's unique ID (the IV field interpreted as uint64_t)
	 * 
	 * @return Packet ID
	 */
	inline uint64_t packetId() const { return at<uint64_t>(ZT_PACKET_IDX_IV); }

	/**
	 * Set packet verb
	 * 
	 * This also has the side-effect of clearing any verb flags, such as
	 * compressed, and so must only be done during packet composition.
	 * 
	 * @param v New packet verb
	 */
	inline void setVerb(Verb v) { (*this)[ZT_PACKET_IDX_VERB] = (char)v; }

	/**
	 * @return Packet verb (not including flag bits)
	 */
	inline Verb verb() const { return (Verb)((*this)[ZT_PACKET_IDX_VERB] & 0x1f); }

	/**
	 * @return Length of packet payload
	 */
	inline unsigned int payloadLength() const { return ((size() < ZT_PROTO_MIN_PACKET_LENGTH) ? 0 : (size() - ZT_PROTO_MIN_PACKET_LENGTH)); }

	/**
	 * @return Raw packet payload
	 */
	inline const unsigned char *payload() const { return field(ZT_PACKET_IDX_PAYLOAD,size() - ZT_PACKET_IDX_PAYLOAD); }

	/**
	 * Armor packet for transport
	 *
	 * @param key 32-byte key
	 * @param encryptPayload If true, encrypt packet payload, else just MAC
	 */
	void armor(const void *key,bool encryptPayload);

	/**
	 * Verify and (if encrypted) decrypt packet
	 *
	 * @param key 32-byte key
	 * @return False if packet is invalid or failed MAC authenticity check
	 */
	bool dearmor(const void *key);

	/**
	 * Attempt to compress payload if not already (must be unencrypted)
	 * 
	 * This requires that the payload at least contain the verb byte already
	 * set. The compressed flag in the verb is set if compression successfully
	 * results in a size reduction. If no size reduction occurs, compression
	 * is not done and the flag is left cleared.
	 * 
	 * @return True if compression occurred
	 */
	bool compress();

	/**
	 * Attempt to decompress payload if it is compressed (must be unencrypted)
	 * 
	 * If payload is compressed, it is decompressed and the compressed verb
	 * flag is cleared. Otherwise nothing is done and true is returned.
	 * 
	 * @return True if data is now decompressed and valid, false on error
	 */
	bool uncompress();

private:
	static const unsigned char ZERO_KEY[32];

	/**
	 * Deterministically mangle a 256-bit crypto key based on packet
	 *
	 * This uses extra data from the packet to mangle the secret, giving us an
	 * effective IV that is somewhat more than 64 bits. This is "free" for
	 * Salsa20 since it has negligible key setup time so using a different
	 * key each time is fine.
	 *
	 * @param in Input key (32 bytes)
	 * @param out Output buffer (32 bytes)
	 */
	inline void _salsa20MangleKey(const unsigned char *in,unsigned char *out) const
	{
		const unsigned char *d = (const unsigned char *)data();

		// IV and source/destination addresses. Using the addresses divides the
		// key space into two halves-- A->B and B->A (since order will change).
		for(unsigned int i=0;i<18;++i) // 8 + (ZT_ADDRESS_LENGTH * 2) == 18
			out[i] = in[i] ^ d[i];

		// Flags, but with hop count masked off. Hop count is altered by forwarding
		// nodes. It's one of the only parts of a packet modifiable by people
		// without the key.
		out[18] = in[18] ^ (d[ZT_PACKET_IDX_FLAGS] & 0xf8);

		// Raw packet size in bytes -- thus each packet size defines a new
		// key space.
		out[19] = in[19] ^ (unsigned char)(size() & 0xff);
		out[20] = in[20] ^ (unsigned char)((size() >> 8) & 0xff); // little endian

		// Rest of raw key is used unchanged
		for(unsigned int i=21;i<32;++i)
			out[i] = in[i];
	}
};

} // namespace ZeroTier

#endif
