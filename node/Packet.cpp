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

#include "Packet.hpp"

namespace ZeroTier {

const unsigned char Packet::ZERO_KEY[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

const char *Packet::verbString(Verb v)
	throw()
{
	switch(v) {
		case VERB_NOP: return "NOP";
		case VERB_HELLO: return "HELLO";
		case VERB_ERROR: return "ERROR";
		case VERB_OK: return "OK";
		case VERB_WHOIS: return "WHOIS";
		case VERB_RENDEZVOUS: return "RENDEZVOUS";
		case VERB_FRAME: return "FRAME";
		case VERB_EXT_FRAME: return "EXT_FRAME";
		case VERB_P5_MULTICAST_FRAME: return "P5_MULTICAST_FRAME";
		case VERB_MULTICAST_LIKE: return "MULTICAST_LIKE";
		case VERB_NETWORK_MEMBERSHIP_CERTIFICATE: return "NETWORK_MEMBERSHIP_CERTIFICATE";
		case VERB_NETWORK_CONFIG_REQUEST: return "NETWORK_CONFIG_REQUEST";
		case VERB_NETWORK_CONFIG_REFRESH: return "NETWORK_CONFIG_REFRESH";
		case VERB_MULTICAST_GATHER: return "MULTICAST_GATHER";
		case VERB_MULTICAST_FRAME: return "MULTICAST_FRAME";
		case VERB_SET_EPHEMERAL_KEY: return "SET_EPHEMERAL_KEY";
		case VERB_CMA: return "CMA";
	}
	return "(unknown)";
}

const char *Packet::errorString(ErrorCode e)
	throw()
{
	switch(e) {
		case ERROR_NONE: return "NONE";
		case ERROR_INVALID_REQUEST: return "INVALID_REQUEST";
		case ERROR_BAD_PROTOCOL_VERSION: return "BAD_PROTOCOL_VERSION";
		case ERROR_OBJ_NOT_FOUND: return "OBJECT_NOT_FOUND";
		case ERROR_IDENTITY_COLLISION: return "IDENTITY_COLLISION";
		case ERROR_UNSUPPORTED_OPERATION: return "UNSUPPORTED_OPERATION";
		case ERROR_NEED_MEMBERSHIP_CERTIFICATE: return "NEED_MEMBERSHIP_CERTIFICATE";
		case ERROR_NETWORK_ACCESS_DENIED_: return "NETWORK_ACCESS_DENIED";
		case ERROR_UNWANTED_MULTICAST: return "UNWANTED_MULTICAST";
	}
	return "(unknown)";
}

void Packet::armor(const void *key,bool encryptPayload)
{
	unsigned char mangledKey[32];
	unsigned char macKey[32];
	unsigned char mac[16];
	const unsigned int payloadLen = size() - ZT_PACKET_IDX_VERB;
	unsigned char *const payload = field(ZT_PACKET_IDX_VERB,payloadLen);

	// Set flag now, since it affects key mangle function
	setCipher(encryptPayload ? ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_SALSA2012 : ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_NONE);

	_salsa20MangleKey((const unsigned char *)key,mangledKey);
	Salsa20 s20(mangledKey,256,field(ZT_PACKET_IDX_IV,8),ZT_PROTO_SALSA20_ROUNDS);

	// MAC key is always the first 32 bytes of the Salsa20 key stream
	// This is the same construction DJB's NaCl library uses
	s20.encrypt(ZERO_KEY,macKey,sizeof(macKey));

	if (encryptPayload)
		s20.encrypt(payload,payload,payloadLen);

	Poly1305::compute(mac,payload,payloadLen,macKey);
	memcpy(field(ZT_PACKET_IDX_MAC,8),mac,8);
}

bool Packet::dearmor(const void *key)
{
	unsigned char mangledKey[32];
	unsigned char macKey[32];
	unsigned char mac[16];
	const unsigned int payloadLen = size() - ZT_PACKET_IDX_VERB;
	unsigned char *const payload = field(ZT_PACKET_IDX_VERB,payloadLen);
	unsigned int cs = cipher();

	if ((cs == ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_NONE)||(cs == ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_SALSA2012)) {
		_salsa20MangleKey((const unsigned char *)key,mangledKey);
		Salsa20 s20(mangledKey,256,field(ZT_PACKET_IDX_IV,8),ZT_PROTO_SALSA20_ROUNDS);

		s20.encrypt(ZERO_KEY,macKey,sizeof(macKey));
		Poly1305::compute(mac,payload,payloadLen,macKey);
		if (!Utils::secureEq(mac,field(ZT_PACKET_IDX_MAC,8),8))
			return false;

		if (cs == ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_SALSA2012)
			s20.decrypt(payload,payload,payloadLen);

		return true;
	} else return false; // unrecognized cipher suite
}

bool Packet::compress()
{
	unsigned char buf[ZT_PROTO_MAX_PACKET_LENGTH * 2];
	if ((!compressed())&&(size() > (ZT_PACKET_IDX_PAYLOAD + 32))) {
		int pl = (int)(size() - ZT_PACKET_IDX_PAYLOAD);
		int cl = LZ4_compress((const char *)field(ZT_PACKET_IDX_PAYLOAD,(unsigned int)pl),(char *)buf,pl);
		if ((cl > 0)&&(cl < pl)) {
			(*this)[ZT_PACKET_IDX_VERB] |= (char)ZT_PROTO_VERB_FLAG_COMPRESSED;
			setSize((unsigned int)cl + ZT_PACKET_IDX_PAYLOAD);
			memcpy(field(ZT_PACKET_IDX_PAYLOAD,(unsigned int)cl),buf,cl);
			return true;
		}
	}
	(*this)[ZT_PACKET_IDX_VERB] &= (char)(~ZT_PROTO_VERB_FLAG_COMPRESSED);
	return false;
}

bool Packet::uncompress()
{
	unsigned char buf[ZT_PROTO_MAX_PACKET_LENGTH];
	if ((compressed())&&(size() >= ZT_PROTO_MIN_PACKET_LENGTH)) {
		if (size() > ZT_PACKET_IDX_PAYLOAD) {
			unsigned int compLen = size() - ZT_PACKET_IDX_PAYLOAD;
			int ucl = LZ4_decompress_safe((const char *)field(ZT_PACKET_IDX_PAYLOAD,compLen),(char *)buf,compLen,sizeof(buf));
			if ((ucl > 0)&&(ucl <= (int)(capacity() - ZT_PACKET_IDX_PAYLOAD))) {
				setSize((unsigned int)ucl + ZT_PACKET_IDX_PAYLOAD);
				memcpy(field(ZT_PACKET_IDX_PAYLOAD,(unsigned int)ucl),buf,ucl);
			} else return false;
		}
		(*this)[ZT_PACKET_IDX_VERB] &= (char)(~ZT_PROTO_VERB_FLAG_COMPRESSED);
	}
	return true;
}

} // namespace ZeroTier
