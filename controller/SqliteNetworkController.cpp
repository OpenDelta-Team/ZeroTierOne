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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include <algorithm>
#include <utility>
#include <stdexcept>
#include <set>

#include "../include/ZeroTierOne.h"
#include "../node/Constants.hpp"

#include "../ext/json-parser/json.h"

#include "SqliteNetworkController.hpp"
#include "../node/Utils.hpp"
#include "../node/CertificateOfMembership.hpp"
#include "../node/NetworkConfig.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "../node/Address.hpp"
#include "../osdep/OSUtils.hpp"

// Include ZT_NETCONF_SCHEMA_SQL constant to init database
#include "schema.sql.c"

// Stored in database as schemaVersion key in Config.
// If not present, database is assumed to be empty and at the current schema version
// and this key/value is added automatically.
#define ZT_NETCONF_SQLITE_SCHEMA_VERSION 1
#define ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR "1"

// API version reported via JSON control plane
#define ZT_NETCONF_CONTROLLER_API_VERSION 1

namespace ZeroTier {

namespace {

static std::string _jsonEscape(const char *s)
{
	if (!s)
		return std::string();
	std::string buf;
	for(const char *p=s;(*p);++p) {
		switch(*p) {
			case '\t': buf.append("\\t");  break;
			case '\b': buf.append("\\b");  break;
			case '\r': buf.append("\\r");  break;
			case '\n': buf.append("\\n");  break;
			case '\f': buf.append("\\f");  break;
			case '"':  buf.append("\\\""); break;
			case '\\': buf.append("\\\\"); break;
			case '/':  buf.append("\\/");  break;
			default:   buf.push_back(*p);  break;
		}
	}
	return buf;
}
static std::string _jsonEscape(const std::string &s) { return _jsonEscape(s.c_str()); }

struct MemberRecord {
	int64_t rowid;
	char nodeId[16];
	bool authorized;
	bool activeBridge;
};

struct NetworkRecord {
	char id[24];
	const char *name;
	const char *v4AssignMode;
	const char *v6AssignMode;
	bool isPrivate;
	bool enableBroadcast;
	bool allowPassiveBridging;
	int multicastLimit;
	uint64_t creationTime;
	uint64_t revision;
};

} // anonymous namespace

SqliteNetworkController::SqliteNetworkController(const char *dbPath) :
	_dbPath(dbPath),
	_db((sqlite3 *)0)
{
	if (sqlite3_open_v2(dbPath,&_db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,(const char *)0) != SQLITE_OK)
		throw std::runtime_error("SqliteNetworkController cannot open database file");
	sqlite3_busy_timeout(_db,10000);

	sqlite3_stmt *s = (sqlite3_stmt *)0;
	if ((sqlite3_prepare_v2(_db,"SELECT v FROM Config WHERE k = 'schemaVersion';",-1,&s,(const char **)0) == SQLITE_OK)&&(s)) {
		int schemaVersion = -1234;
		if (sqlite3_step(s) == SQLITE_ROW) {
			schemaVersion = sqlite3_column_int(s,0);
		}

		sqlite3_finalize(s);

		if (schemaVersion == -1234) {
			sqlite3_close(_db);
			throw std::runtime_error("SqliteNetworkController schemaVersion not found in Config table (init failure?)");
		} else if (schemaVersion != ZT_NETCONF_SQLITE_SCHEMA_VERSION) {
			// Note -- this will eventually run auto-upgrades so this isn't how it'll work going forward
			sqlite3_close(_db);
			throw std::runtime_error("SqliteNetworkController database schema version mismatch");
		}
	} else {
		// Prepare statement will fail if Config table doesn't exist, which means our DB
		// needs to be initialized.
		if (sqlite3_exec(_db,ZT_NETCONF_SCHEMA_SQL"INSERT INTO Config (k,v) VALUES ('schemaVersion',"ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR");",0,0,0) != SQLITE_OK) {
			sqlite3_close(_db);
			throw std::runtime_error("SqliteNetworkController cannot initialize database and/or insert schemaVersion into Config table");
		}
	}

	if (
			  (sqlite3_prepare_v2(_db,"SELECT name,private,enableBroadcast,allowPassiveBridging,v4AssignMode,v6AssignMode,multicastLimit,creationTime,revision FROM Network WHERE id = ?",-1,&_sGetNetworkById,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT rowid,authorized,activeBridge FROM Member WHERE networkId = ? AND nodeId = ?",-1,&_sGetMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Member (networkId,nodeId,authorized,activeBridge) VALUES (?,?,?,0)",-1,&_sCreateMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT identity FROM Node WHERE id = ?",-1,&_sGetNodeIdentity,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Node (id,identity,lastAt,lastSeen,firstSeen) VALUES (?,?,?,?,?)",-1,&_sCreateNode,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Node SET lastAt = ?,lastSeen = ? WHERE id = ?",-1,&_sUpdateNode,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Node SET lastSeen = ? WHERE id = ?",-1,&_sUpdateNode2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT etherType FROM Rule WHERE networkId = ? AND \"action\" = 'accept'",-1,&_sGetEtherTypesFromRuleTable,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId FROM Member WHERE networkId = ? AND activeBridge > 0 AND authorized > 0",-1,&_sGetActiveBridges,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ip,ipNetmaskBits FROM IpAssignment WHERE networkId = ? AND nodeId = ? AND ipVersion = ?",-1,&_sGetIpAssignmentsForNode,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT p.ipFirst,p.ipLast,r.ipNetmaskBits FROM IpAssignmentPool AS p JOIN Route AS r ON r.ip = p.routeIp WHERE p.networkId = ? AND r.ipVersion = ?",-1,&_sGetIpAssignmentPools,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT 1 FROM IpAssignment WHERE networkId = ? AND ip = ? AND ipVersion = ?",-1,&_sCheckIfIpIsAllocated,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO IpAssignment (routeIp,networkId,nodeId,ip,ipNetmaskBits,ipVersion) VALUES ((SELECT routeIp FROM IpAssignmentPool WHERE networkId = ? AND ipFirst <= ? AND ipLast >= ?),?,?,?,?,?)",-1,&_sAllocateIp,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM IpAssignment WHERE networkId = ? AND nodeId = ?",-1,&_sDeleteIpAllocations,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId,phyAddress FROM Relay WHERE networkId = ? ORDER BY nodeId ASC",-1,&_sGetRelays,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT id FROM Network ORDER BY id ASC",-1,&_sListNetworks,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT m.nodeId FROM Member AS m WHERE m.networkId = ? ORDER BY m.nodeId ASC",-1,&_sListNetworkMembers,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT m.authorized,m.activeBridge,n.identity,n.lastAt,n.lastSeen,n.firstSeen FROM Member AS m JOIN Node AS n ON n.id = m.nodeId WHERE m.networkId = ? AND m.nodeId = ?",-1,&_sGetMember2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId,ip,ipNetmaskBits,ipVersion FROM Route WHERE networkId = ? ORDER BY ip ASC",-1,&_sGetRoutes,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT p.routeIp,p.ipFirst,p.ipLast,r.ipNetmaskBits,r.ipVersion FROM IpAssignmentPool AS p JOIN Route AS r ON r.ip = p.routeIp WHERE p.networkId = ? ORDER BY p.routeIp ASC",-1,&_sGetIpAssignmentPools2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ruleNo,nodeId,vlanId,vlanPcp,etherType,macSource,macDest,ipSource,ipDest,ipTos,ipProtocol,ipSourcePort,ipDestPort,\"flags\",invFlags,\"action\" FROM Rule WHERE networkId = ? ORDER BY ruleNo ASC",-1,&_sListRules,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Rule (networkId,ruleNo,nodeId,vlanId,vlanPcP,etherType,macSource,macDest,ipSource,ipDest,ipTos,ipProtocol,ipSourcePort,ipDestPort,\"action\") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",-1,&_sCreateRule,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Network (id,name,creationTime,revision) VALUES (?,?,?,1)",-1,&_sCreateNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT revision FROM Network WHERE id = ?",-1,&_sGetNetworkRevision,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Network SET revision = ? WHERE id = ?",-1,&_sSetNetworkRevision,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ip,ipNetmaskBits,ipVersion FROM IpAssignment WHERE networkId = ? AND nodeId = ? ORDER BY ip ASC",-1,&_sGetIpAssignmentsForNode2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Relay WHERE networkId = ?",-1,&_sDeleteRelaysForNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Relay (networkId,nodeId,phyAddress) VALUES (?,?,?)",-1,&_sCreateRelay,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Route WHERE networkId = ?",-1,&_sDeleteRoutesForNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Route (networkId,nodeId,ip,ipNetmaskBits,ipVersion) VALUES (?,?,?,?,?)",-1,&_sCreateRoute,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM IpAssignmentPool WHERE networkId = ?",-1,&_sDeleteIpAssignmentPoolsForNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Rule WHERE networkId = ?",-1,&_sDeleteRulesForNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO IpAssignmentPool (networkId,routeIp,ipFirst,ipLast) VALUES (?,?,?,?)",-1,&_sCreateIpAssignmentPool,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET authorized = ? WHERE rowid = ?",-1,&_sUpdateMemberAuthorized,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET activeBridge = ? WHERE rowid = ?",-1,&_sUpdateMemberActiveBridge,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Member WHERE networkId = ? AND nodeId = ?",-1,&_sDeleteMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Network WHERE id = ?",-1,&_sDeleteNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ip,ipVersion,metric FROM Gateway WHERE networkId = ? ORDER BY metric ASC",-1,&_sGetGateways,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Gateway WHERE networkId = ?",-1,&_sDeleteGateways,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Gateway (networkId,ip,ipVersion,metric) VALUES (?,?,?,?)",-1,&_sCreateGateway,(const char **)0) != SQLITE_OK)
		 ) {
		//printf("!!! %s\n",sqlite3_errmsg(_db));
		sqlite3_close(_db);
		throw std::runtime_error("SqliteNetworkController unable to initialize one or more prepared statements");
	}
}

SqliteNetworkController::~SqliteNetworkController()
{
	Mutex::Lock _l(_lock);
	if (_db) {
		sqlite3_finalize(_sGetNetworkById);
		sqlite3_finalize(_sGetMember);
		sqlite3_finalize(_sCreateMember);
		sqlite3_finalize(_sGetNodeIdentity);
		sqlite3_finalize(_sCreateNode);
		sqlite3_finalize(_sUpdateNode);
		sqlite3_finalize(_sUpdateNode2);
		sqlite3_finalize(_sGetEtherTypesFromRuleTable);
		sqlite3_finalize(_sGetActiveBridges);
		sqlite3_finalize(_sGetIpAssignmentsForNode);
		sqlite3_finalize(_sGetIpAssignmentPools);
		sqlite3_finalize(_sCheckIfIpIsAllocated);
		sqlite3_finalize(_sAllocateIp);
		sqlite3_finalize(_sDeleteIpAllocations);
		sqlite3_finalize(_sGetRelays);
		sqlite3_finalize(_sListNetworks);
		sqlite3_finalize(_sListNetworkMembers);
		sqlite3_finalize(_sGetMember2);
		sqlite3_finalize(_sGetRoutes);
		sqlite3_finalize(_sGetIpAssignmentPools2);
		sqlite3_finalize(_sListRules);
		sqlite3_finalize(_sCreateRule);
		sqlite3_finalize(_sCreateNetwork);
		sqlite3_finalize(_sGetNetworkRevision);
		sqlite3_finalize(_sSetNetworkRevision);
		sqlite3_finalize(_sGetIpAssignmentsForNode2);
		sqlite3_finalize(_sDeleteRelaysForNetwork);
		sqlite3_finalize(_sCreateRelay);
		sqlite3_finalize(_sDeleteRoutesForNetwork);
		sqlite3_finalize(_sCreateRoute);
		sqlite3_finalize(_sDeleteIpAssignmentPoolsForNetwork);
		sqlite3_finalize(_sDeleteRulesForNetwork);
		sqlite3_finalize(_sCreateIpAssignmentPool);
		sqlite3_finalize(_sUpdateMemberAuthorized);
		sqlite3_finalize(_sUpdateMemberActiveBridge);
		sqlite3_finalize(_sDeleteMember);
		sqlite3_finalize(_sDeleteNetwork);
		sqlite3_finalize(_sGetGateways);
		sqlite3_finalize(_sDeleteGateways);
		sqlite3_finalize(_sCreateGateway);
		sqlite3_close(_db);
	}
}

NetworkController::ResultCode SqliteNetworkController::doNetworkConfigRequest(const InetAddress &fromAddr,const Identity &signingId,const Identity &identity,uint64_t nwid,const Dictionary &metaData,uint64_t haveRevision,Dictionary &netconf)
{
	Mutex::Lock _l(_lock);

	// Note: we can't reuse prepared statements that return const char * pointers without
	// making our own copy in e.g. a std::string first.

	if ((!signingId)||(!signingId.hasPrivate())) {
		netconf["error"] = "signing identity invalid or lacks private key";
		return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}
	if (signingId.address().toInt() != (nwid >> 24)) {
		netconf["error"] = "signing identity address does not match most significant 40 bits of network ID";
		return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}

	NetworkRecord network;
	memset(&network,0,sizeof(network));
	Utils::snprintf(network.id,sizeof(network.id),"%.16llx",(unsigned long long)nwid);

	MemberRecord member;
	memset(&member,0,sizeof(member));
	Utils::snprintf(member.nodeId,sizeof(member.nodeId),"%.10llx",(unsigned long long)identity.address().toInt());

	// Create/update Node record and check identity fully -- identities are first-come-first-claim

	sqlite3_reset(_sGetNodeIdentity);
	sqlite3_bind_text(_sGetNodeIdentity,1,member.nodeId,10,SQLITE_STATIC);
	if (sqlite3_step(_sGetNodeIdentity) == SQLITE_ROW) {
		try {
			Identity alreadyKnownIdentity((const char *)sqlite3_column_text(_sGetNodeIdentity,0));
			if (alreadyKnownIdentity == identity) {
				char lastSeen[64];
				Utils::snprintf(lastSeen,sizeof(lastSeen),"%llu",(unsigned long long)OSUtils::now());
				if (fromAddr) {
					std::string lastAt(fromAddr.toString());
					sqlite3_reset(_sUpdateNode);
					sqlite3_bind_text(_sUpdateNode,1,lastAt.c_str(),-1,SQLITE_STATIC);
					sqlite3_bind_text(_sUpdateNode,2,lastSeen,-1,SQLITE_STATIC);
					sqlite3_bind_text(_sUpdateNode,3,member.nodeId,10,SQLITE_STATIC);
					sqlite3_step(_sUpdateNode);
				} else { // fromAddr is empty, which means this was a relayed packet -- so don't update lastAt
					sqlite3_reset(_sUpdateNode2);
					sqlite3_bind_text(_sUpdateNode2,1,lastSeen,-1,SQLITE_STATIC);
					sqlite3_bind_text(_sUpdateNode2,2,member.nodeId,10,SQLITE_STATIC);
					sqlite3_step(_sUpdateNode2);
				}
			} else {
				return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
			}
		} catch ( ... ) { // identity stored in database is not valid or is NULL
			return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
		}
	} else {
		std::string idstr(identity.toString(false));
		std::string lastAt;
		if (fromAddr)
			lastAt = fromAddr.toString();
		char lastSeen[64];
		Utils::snprintf(lastSeen,sizeof(lastSeen),"%llu",(unsigned long long)OSUtils::now());
		sqlite3_reset(_sCreateNode);
		sqlite3_bind_text(_sCreateNode,1,member.nodeId,10,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateNode,2,idstr.c_str(),-1,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateNode,3,lastAt.c_str(),-1,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateNode,4,lastSeen,-1,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateNode,5,lastSeen,-1,SQLITE_STATIC);
		if (sqlite3_step(_sCreateNode) != SQLITE_DONE) {
			netconf["error"] = "unable to create new node record";
			return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}
	}

	// Fetch Network record

	sqlite3_reset(_sGetNetworkById);
	sqlite3_bind_text(_sGetNetworkById,1,network.id,16,SQLITE_STATIC);
	if (sqlite3_step(_sGetNetworkById) == SQLITE_ROW) {
		network.name = (const char *)sqlite3_column_text(_sGetNetworkById,0);
		network.isPrivate = (sqlite3_column_int(_sGetNetworkById,1) > 0);
		network.enableBroadcast = (sqlite3_column_int(_sGetNetworkById,2) > 0);
		network.allowPassiveBridging = (sqlite3_column_int(_sGetNetworkById,3) > 0);
		network.v4AssignMode = (const char *)sqlite3_column_text(_sGetNetworkById,4);
		network.v6AssignMode = (const char *)sqlite3_column_text(_sGetNetworkById,5);
		network.multicastLimit = sqlite3_column_int(_sGetNetworkById,6);
		network.creationTime = (uint64_t)sqlite3_column_int64(_sGetNetworkById,7);
		network.revision = (uint64_t)sqlite3_column_int64(_sGetNetworkById,8);
	} else return NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND;

	// Fetch Member record

	bool foundMember = false;
	sqlite3_reset(_sGetMember);
	sqlite3_bind_text(_sGetMember,1,network.id,16,SQLITE_STATIC);
	sqlite3_bind_text(_sGetMember,2,member.nodeId,10,SQLITE_STATIC);
	if (sqlite3_step(_sGetMember) == SQLITE_ROW) {
		foundMember = true;
		member.rowid = (int64_t)sqlite3_column_int64(_sGetMember,0);
		member.authorized = (sqlite3_column_int(_sGetMember,1) > 0);
		member.activeBridge = (sqlite3_column_int(_sGetMember,2) > 0);
	}

	// Create Member record for unknown nodes, auto-authorizing if network is public

	if (!foundMember) {
		member.authorized = (network.isPrivate ? false : true);
		member.activeBridge = false;
		sqlite3_reset(_sCreateMember);
		sqlite3_bind_text(_sCreateMember,1,network.id,16,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateMember,2,member.nodeId,10,SQLITE_STATIC);
		sqlite3_bind_int(_sCreateMember,3,(member.authorized ? 1 : 0));
		if (sqlite3_step(_sCreateMember) != SQLITE_DONE) {
			netconf["error"] = "unable to create new member record";
			return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}
		member.rowid = (int64_t)sqlite3_last_insert_rowid(_db);
	}

	// Check member authorization

	if (!member.authorized)
		return NetworkController::NETCONF_QUERY_ACCESS_DENIED;

	// If netconf is unchanged from client reported revision, just tell client they're up to date

	if ((haveRevision > 0)&&(haveRevision == network.revision))
		return NetworkController::NETCONF_QUERY_OK_BUT_NOT_NEWER;

	// Create and sign netconf

	netconf.clear();
	{
		char tss[24],rs[24];
		Utils::snprintf(tss,sizeof(tss),"%.16llx",(unsigned long long)OSUtils::now());
		Utils::snprintf(rs,sizeof(rs),"%.16llx",(unsigned long long)network.revision);
		netconf[ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP] = tss;
		netconf[ZT_NETWORKCONFIG_DICT_KEY_REVISION] = rs;
		netconf[ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID] = network.id;
		netconf[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO] = member.nodeId;
		netconf[ZT_NETWORKCONFIG_DICT_KEY_PRIVATE] = network.isPrivate ? "1" : "0";
		netconf[ZT_NETWORKCONFIG_DICT_KEY_NAME] = (network.name) ? network.name : "";
		netconf[ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST] = network.enableBroadcast ? "1" : "0";
		netconf[ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING] = network.allowPassiveBridging ? "1" : "0";

		{
			std::vector<int> allowedEtherTypes;
			sqlite3_reset(_sGetEtherTypesFromRuleTable);
			sqlite3_bind_text(_sGetEtherTypesFromRuleTable,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetEtherTypesFromRuleTable) == SQLITE_ROW) {
				int et = sqlite3_column_int(_sGetEtherTypesFromRuleTable,0);
				if ((et >= 0)&&(et <= 0xffff))
					allowedEtherTypes.push_back(et);
			}
			std::sort(allowedEtherTypes.begin(),allowedEtherTypes.end());
			allowedEtherTypes.erase(std::unique(allowedEtherTypes.begin(),allowedEtherTypes.end()),allowedEtherTypes.end());
			std::string allowedEtherTypesCsv;
			for(std::vector<int>::const_iterator i(allowedEtherTypes.begin());i!=allowedEtherTypes.end();++i) {
				if (allowedEtherTypesCsv.length())
					allowedEtherTypesCsv.push_back(',');
				char tmp[16];
				Utils::snprintf(tmp,sizeof(tmp),"%.4x",(unsigned int)*i);
				allowedEtherTypesCsv.append(tmp);
			}
			netconf[ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES] = allowedEtherTypesCsv;
		}

		if (network.multicastLimit > 0) {
			char ml[16];
			Utils::snprintf(ml,sizeof(ml),"%lx",(unsigned long)network.multicastLimit);
			netconf[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT] = ml;
		}

		{
			std::string activeBridges;
			sqlite3_reset(_sGetActiveBridges);
			sqlite3_bind_text(_sGetActiveBridges,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetActiveBridges) == SQLITE_ROW) {
				const char *ab = (const char *)sqlite3_column_text(_sGetActiveBridges,0);
				if ((ab)&&(strlen(ab) == 10)) {
					if (activeBridges.length())
						activeBridges.push_back(',');
					activeBridges.append(ab);
				}
				if (activeBridges.length() > 1024) // sanity check -- you can't have too many active bridges at the moment
					break;
			}
			if (activeBridges.length())
				netconf[ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES] = activeBridges;
		}

		{
			std::string relays;
			sqlite3_reset(_sGetRelays);
			sqlite3_bind_text(_sGetRelays,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetRelays) == SQLITE_ROW) {
				const char *n = (const char *)sqlite3_column_text(_sGetRelays,0);
				const char *a = (const char *)sqlite3_column_text(_sGetRelays,1);
				if ((n)&&(a)) {
					Address node(n);
					InetAddress addr(a);
					if ((node)&&(addr)) {
						if (relays.length())
							relays.push_back(',');
						relays.append(node.toString());
						relays.push_back(';');
						relays.append(addr.toString());
					}
				}
			}
			if (relays.length())
				netconf[ZT_NETWORKCONFIG_DICT_KEY_RELAYS] = relays;
		}

		{
			char tmp[128];
			std::string gateways;
			sqlite3_reset(_sGetGateways);
			sqlite3_bind_text(_sGetGateways,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetGateways) == SQLITE_ROW) {
				const unsigned char *ip = (const unsigned char *)sqlite3_column_blob(_sGetGateways,0);
				switch(sqlite3_column_int(_sGetGateways,1)) { // ipVersion
					case 4:
						Utils::snprintf(tmp,sizeof(tmp),"%s%d.%d.%d.%d/%d",
							(gateways.length() > 0) ? "," : "",
							(int)ip[0],
							(int)ip[1],
							(int)ip[2],
							(int)ip[3],
							(int)sqlite3_column_int(_sGetGateways,2)); // metric
						gateways.append(tmp);
						break;
					case 6:
						Utils::snprintf(tmp,sizeof(tmp),"%s%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x/%d",
							(gateways.length() > 0) ? "," : "",
							(int)ip[0],
							(int)ip[1],
							(int)ip[2],
							(int)ip[3],
							(int)ip[4],
							(int)ip[5],
							(int)ip[6],
							(int)ip[7],
							(int)ip[8],
							(int)ip[9],
							(int)ip[10],
							(int)ip[11],
							(int)ip[12],
							(int)ip[13],
							(int)ip[14],
							(int)ip[15],
							(int)sqlite3_column_int(_sGetGateways,2)); // metric
						gateways.append(tmp);
						break;
				}
			}
			if (gateways.length())
				netconf[ZT_NETWORKCONFIG_DICT_KEY_GATEWAYS] = gateways;
		}

		if ((network.v4AssignMode)&&(!strcmp(network.v4AssignMode,"zt"))) {
			std::string v4s;

			sqlite3_reset(_sGetIpAssignmentsForNode);
			sqlite3_bind_text(_sGetIpAssignmentsForNode,1,network.id,16,SQLITE_STATIC);
			sqlite3_bind_text(_sGetIpAssignmentsForNode,2,member.nodeId,10,SQLITE_STATIC);
			sqlite3_bind_int(_sGetIpAssignmentsForNode,3,4); // 4 == IPv4
			while (sqlite3_step(_sGetIpAssignmentsForNode) == SQLITE_ROW) {
				const unsigned char *ip = (const unsigned char *)sqlite3_column_blob(_sGetIpAssignmentsForNode,0);
				int ipNetmaskBits = sqlite3_column_int(_sGetIpAssignmentsForNode,1);
				if ((ip)&&(sqlite3_column_bytes(_sGetIpAssignmentsForNode,0) >= 4)&&(ipNetmaskBits > 0)&&(ipNetmaskBits <= 32)) {
					char tmp[32];
					Utils::snprintf(tmp,sizeof(tmp),"%d.%d.%d.%d/%d",(int)ip[12+0],(int)ip[12+1],(int)ip[12+2],(int)ip[12+3],ipNetmaskBits);
					if (v4s.length())
						v4s.push_back(',');
					v4s.append(tmp);
				}
			}

			if (!v4s.length()) {
				// Attempt to auto-assign an IPv4 address from an available pool if one isn't assigned already
				sqlite3_reset(_sGetIpAssignmentPools);
				sqlite3_bind_text(_sGetIpAssignmentPools,1,network.id,16,SQLITE_STATIC);
				sqlite3_bind_int(_sGetIpAssignmentPools,2,4); // 4 == IPv4
				while ((!v4s.length())&&(sqlite3_step(_sGetIpAssignmentPools) == SQLITE_ROW)) {
					InetAddress ipFirst = BlobToInetAddress((const char *)sqlite3_column_blob(_sGetIpAssignmentPools,0), 32, 4);
					InetAddress ipLast = BlobToInetAddress((const char *)sqlite3_column_blob(_sGetIpAssignmentPools,0), 32, 4);
					int ipNetmaskBits = sqlite3_column_int(_sGetIpAssignmentPools,2);
					if ((ipFirst)&&(ipLast)&&(sqlite3_column_bytes(_sGetIpAssignmentPools,0) >= 4)&&(sqlite3_column_bytes(_sGetIpAssignmentPools,1) >= 4)&&(ipNetmaskBits > 0)&&(ipNetmaskBits < 32)) {
						uint32_t nFirst = Utils::ntoh(*((const uint32_t *)ipFirst.rawIpData())); // network in host byte order e.g. 192.168.0.0
						uint32_t nLast = Utils::ntoh(*((const uint32_t *)ipLast.rawIpData())); // network in host byte order e.g. 192.168.0.0

						for(uint32_t ip=nFirst;ip<=nLast;++ip){
							uint32_t nip = Utils::hton(ip); // IP in big-endian "network" byte order
							unsigned char ipBlob[16];
							memset(ipBlob,0,12);
							memcpy(ipBlob + 12,&nip,4);

							sqlite3_reset(_sCheckIfIpIsAllocated);
							sqlite3_bind_text(_sCheckIfIpIsAllocated,1,network.id,16,SQLITE_STATIC);
							sqlite3_bind_blob(_sCheckIfIpIsAllocated,2,(const void *)ipBlob,16,SQLITE_STATIC);
							sqlite3_bind_int(_sCheckIfIpIsAllocated,3,4); // 4 == IPv4
							if (sqlite3_step(_sCheckIfIpIsAllocated) != SQLITE_ROW) {
								// No rows returned, so the IP is available
								sqlite3_reset(_sAllocateIp);
								sqlite3_bind_text(_sAllocateIp,1,network.id,16,SQLITE_STATIC);
								sqlite3_bind_blob(_sAllocateIp,2,(const void *)ipBlob,16,SQLITE_STATIC);
								sqlite3_bind_blob(_sAllocateIp,3,(const void *)ipBlob,16,SQLITE_STATIC);
								sqlite3_bind_text(_sAllocateIp,4,network.id,16,SQLITE_STATIC);
								sqlite3_bind_text(_sAllocateIp,5,member.nodeId,10,SQLITE_STATIC);
								sqlite3_bind_blob(_sAllocateIp,6,(const void *)ipBlob,16,SQLITE_STATIC);
								sqlite3_bind_int(_sAllocateIp,7,ipNetmaskBits);
								sqlite3_bind_int(_sAllocateIp,8,4); // 4 == IPv4
								if (sqlite3_step(_sAllocateIp) == SQLITE_DONE) {
									char tmp[32];
									Utils::snprintf(tmp,sizeof(tmp),"%d.%d.%d.%d/%d",(int)((ip >> 24) & 0xff),(int)((ip >> 16) & 0xff),(int)((ip >> 8) & 0xff),(int)(ip & 0xff),ipNetmaskBits);
									if (v4s.length())
										v4s.push_back(',');
									v4s.append(tmp);
									break; // IP found and reserved! v4s containing something will cause outer while() to break.
								} else {
									printf("!!! %s\n",sqlite3_errmsg(_db));
								}
							}
						}
					}
				}
			}

			if (v4s.length()) {
				netconf[ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC] = v4s;
			}
		}

		// TODO: IPv6 auto-assign once it's supported in UI

		if (network.isPrivate) {
			CertificateOfMembership com(network.revision,ZT1_CERTIFICATE_OF_MEMBERSHIP_REVISION_MAX_DELTA,nwid,identity.address());
			if (com.sign(signingId)) // basically can't fail unless our identity is invalid
				netconf[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP] = com.toString();
			else {
				netconf["error"] = "unable to sign COM";
				return NETCONF_QUERY_INTERNAL_SERVER_ERROR;
			}
		}

		if (!netconf.sign(signingId,OSUtils::now())) {
			netconf["error"] = "unable to sign netconf dictionary";
			return NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}
	}

	return NetworkController::NETCONF_QUERY_OK;
}

unsigned int SqliteNetworkController::handleControlPlaneHttpGET(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	Mutex::Lock _l(_lock);
	return _doCPGet(path,urlArgs,headers,body,responseBody,responseContentType);
}

unsigned int SqliteNetworkController::handleControlPlaneHttpPOST(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	if (path.empty())
		return 404;
	Mutex::Lock _l(_lock);

	if (path[0] == "network") {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			int64_t revision = 0;
			sqlite3_reset(_sGetNetworkRevision);
			sqlite3_bind_text(_sGetNetworkRevision,1,nwids,16,SQLITE_STATIC);
			bool networkExists = false;
			if (sqlite3_step(_sGetNetworkRevision) == SQLITE_ROW) {
				networkExists = true;
				revision = sqlite3_column_int64(_sGetNetworkRevision,0);
			}

			if (path.size() >= 3) {

				if (!networkExists)
					return 404;

				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

					int64_t memberRowId = 0;
					sqlite3_reset(_sGetMember);
					sqlite3_bind_text(_sGetMember,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sGetMember,2,addrs,10,SQLITE_STATIC);
					bool memberExists = false;
					if (sqlite3_step(_sGetMember) == SQLITE_ROW) {
						memberExists = true;
						memberRowId = sqlite3_column_int64(_sGetMember,0);
					}

					if (!memberExists) {
						sqlite3_reset(_sCreateMember);
						sqlite3_bind_text(_sCreateMember,1,nwids,16,SQLITE_STATIC);
						sqlite3_bind_text(_sCreateMember,2,addrs,10,SQLITE_STATIC);
						sqlite3_bind_int(_sCreateMember,3,0);
						if (sqlite3_step(_sCreateMember) != SQLITE_DONE)
							return 500;
						memberRowId = (int64_t)sqlite3_last_insert_rowid(_db);
					}

					json_value *j = json_parse(body.c_str(),body.length());
					if (j) {
						if (j->type == json_object) {
							for(unsigned int k=0;k<j->u.object.length;++k) {

								if (!strcmp(j->u.object.values[k].name,"authorized")) {
									if (j->u.object.values[k].value->type == json_boolean) {
										sqlite3_reset(_sUpdateMemberAuthorized);
										sqlite3_bind_int(_sUpdateMemberAuthorized,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
										sqlite3_bind_int64(_sUpdateMemberAuthorized,2,memberRowId);
										if (sqlite3_step(_sUpdateMemberAuthorized) != SQLITE_DONE)
											return 500;
									}
								} else if (!strcmp(j->u.object.values[k].name,"activeBridge")) {
									if (j->u.object.values[k].value->type == json_boolean) {
										sqlite3_reset(_sUpdateMemberActiveBridge);
										sqlite3_bind_int(_sUpdateMemberActiveBridge,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
										sqlite3_bind_int64(_sUpdateMemberActiveBridge,2,memberRowId);
										if (sqlite3_step(_sUpdateMemberActiveBridge) != SQLITE_DONE)
											return 500;
									}
								} else if (!strcmp(j->u.object.values[k].name,"ipAssignments")) {
									if (j->u.object.values[k].value->type == json_array) {
										sqlite3_reset(_sDeleteIpAllocations);
										sqlite3_bind_text(_sDeleteIpAllocations,1,nwids,16,SQLITE_STATIC);
										sqlite3_bind_text(_sDeleteIpAllocations,2,addrs,10,SQLITE_STATIC);
										if (sqlite3_step(_sDeleteIpAllocations) != SQLITE_DONE)
											return 500;
										for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
											json_value *ipalloc = j->u.object.values[k].value->u.array.values[kk];
											if (ipalloc->type == json_string) {
												InetAddress a(ipalloc->u.string.ptr);
												char ipBlob[16];
												InetAddressToBlob(ipBlob, &a);
												int ipVersion = a.ss_family == AF_INET ? 4 : (a.ss_family == AF_INET6 ? 6 : 0);
												if (ipVersion > 0) {
													sqlite3_reset(_sAllocateIp);
													sqlite3_bind_text(_sAllocateIp,1,nwids,16,SQLITE_STATIC);
													sqlite3_bind_blob(_sAllocateIp,2,(const void *)ipBlob,16,SQLITE_STATIC);
													sqlite3_bind_blob(_sAllocateIp,3,(const void *)ipBlob,16,SQLITE_STATIC);
													sqlite3_bind_text(_sAllocateIp,4,nwids,16,SQLITE_STATIC);
													sqlite3_bind_text(_sAllocateIp,5,addrs,10,SQLITE_STATIC);
													sqlite3_bind_blob(_sAllocateIp,6,(const void *)ipBlob,16,SQLITE_STATIC);
													sqlite3_bind_int(_sAllocateIp,7,(int)a.netmaskBits());
													sqlite3_bind_int(_sAllocateIp,8,ipVersion);
													if (sqlite3_step(_sAllocateIp) != SQLITE_DONE)
														return 500;
												}
											}
										}
									}
								}

							}
						}
						json_value_free(j);
					}

					return _doCPGet(path,urlArgs,headers,body,responseBody,responseContentType);
				} // else 404

			} else {
				std::vector<std::string> path_copy(path);

				if (!networkExists) {
					if (path[1].substr(10) == "______") {
						// A special POST /network/##########______ feature lets users create a network
						// with an arbitrary unused network ID.
						nwid = 0;

						uint64_t nwidPrefix = (Utils::hexStrToU64(path[1].substr(0,10).c_str()) << 24) & 0xffffffffff000000ULL;
						uint64_t nwidPostfix = 0;
						Utils::getSecureRandom(&nwidPostfix,sizeof(nwidPostfix));
						nwidPostfix &= 0xffffffULL;
						uint64_t nwidOriginalPostfix = nwidPostfix;
						do {
							uint64_t tryNwid = nwidPrefix | nwidPostfix;
							Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)tryNwid);

							sqlite3_reset(_sGetNetworkRevision);
							sqlite3_bind_text(_sGetNetworkRevision,1,nwids,16,SQLITE_STATIC);
							if (sqlite3_step(_sGetNetworkRevision) != SQLITE_ROW) {
								nwid = tryNwid;
								break;
							}

							++nwidPostfix;
							nwidPostfix &= 0xffffffULL;
						} while (nwidPostfix != nwidOriginalPostfix);

						// 503 means we have no more free IDs for this prefix. You shouldn't host anywhere
						// near 16 million networks on the same controller, so shouldn't happen.
						if (!nwid)
							return 503;
					}

					sqlite3_reset(_sCreateNetwork);
					sqlite3_bind_text(_sCreateNetwork,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sCreateNetwork,2,nwids,16,SQLITE_STATIC); // default name, will be changed below if a name is specified in JSON
					sqlite3_bind_int64(_sCreateNetwork,3,(long long)OSUtils::now());
					if (sqlite3_step(_sCreateNetwork) != SQLITE_DONE)
						return 500;
					path_copy[1].assign(nwids);
				}

				json_value *j = json_parse(body.c_str(),body.length());
				if (j) {
					if (j->type == json_object) {
						for(unsigned int k=0;k<j->u.object.length;++k) {
							sqlite3_stmt *stmt = (sqlite3_stmt *)0;

							if (!strcmp(j->u.object.values[k].name,"name")) {
								if ((j->u.object.values[k].value->type == json_string)&&(j->u.object.values[k].value->u.string.ptr[0])) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET name = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_text(stmt,1,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
								}
							} else if (!strcmp(j->u.object.values[k].name,"private")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET private = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
								}
							} else if (!strcmp(j->u.object.values[k].name,"enableBroadcast")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET enableBroadcast = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
								}
							} else if (!strcmp(j->u.object.values[k].name,"allowPassiveBridging")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET allowPassiveBridging = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
								}
							} else if (!strcmp(j->u.object.values[k].name,"v4AssignMode")) {
								if (j->u.object.values[k].value->type == json_string) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET v4AssignMode = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_text(stmt,1,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
								}
							} else if (!strcmp(j->u.object.values[k].name,"v6AssignMode")) {
								if (j->u.object.values[k].value->type == json_string) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET v6AssignMode = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_text(stmt,1,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
								}
							} else if (!strcmp(j->u.object.values[k].name,"multicastLimit")) {
								if (j->u.object.values[k].value->type == json_integer) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET multicastLimit = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(int)j->u.object.values[k].value->u.integer);
								}
							} else if (!strcmp(j->u.object.values[k].name,"relays")) {
								if (j->u.object.values[k].value->type == json_array) {
									std::map<Address,InetAddress> nodeIdToPhyAddress;
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *relay = j->u.object.values[k].value->u.array.values[kk];
										const char *address = (const char *)0;
										const char *phyAddress = (const char *)0;
										if ((relay)&&(relay->type == json_object)) {
											for(unsigned int rk=0;rk<relay->u.object.length;++rk) {
												if ((!strcmp(relay->u.object.values[rk].name,"address"))&&(relay->u.object.values[rk].value->type == json_string))
													address = relay->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(relay->u.object.values[rk].name,"phyAddress"))&&(relay->u.object.values[rk].value->type == json_string))
													phyAddress = relay->u.object.values[rk].value->u.string.ptr;
											}
										}
										if ((address)&&(phyAddress))
											nodeIdToPhyAddress[Address(address)] = InetAddress(phyAddress);
									}

									sqlite3_reset(_sDeleteRelaysForNetwork);
									sqlite3_bind_text(_sDeleteRelaysForNetwork,1,nwids,16,SQLITE_STATIC);
									sqlite3_step(_sDeleteRelaysForNetwork);

									for(std::map<Address,InetAddress>::iterator rl(nodeIdToPhyAddress.begin());rl!=nodeIdToPhyAddress.end();++rl) {
										sqlite3_reset(_sCreateRelay);
										sqlite3_bind_text(_sCreateRelay,1,nwids,16,SQLITE_STATIC);
										sqlite3_bind_text(_sCreateRelay,2,rl->first.toString().c_str(),-1,SQLITE_STATIC);
										sqlite3_bind_text(_sCreateRelay,3,rl->second.toString().c_str(),-1,SQLITE_STATIC);
										sqlite3_step(_sCreateRelay);
									}
								}
							} else if (!strcmp(j->u.object.values[k].name,"routes")) {
								if (j->u.object.values[k].value->type == json_array) {
									std::set<InetAddress> routes;
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *route = j->u.object.values[k].value->u.array.values[kk];
										const char *nodeId = (const char *)0;
										const char *net = (const char *)0;
										int bits = 0;
										if ((route)&&(route->type == json_object)) {
											for(unsigned int rk=0;rk<route->u.object.length;++rk) {
												if ((!strcmp(route->u.object.values[rk].name,"nodeId"))&&(route->u.object.values[rk].value->type == json_string))
													nodeId = route->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(route->u.object.values[rk].name,"network"))&&(route->u.object.values[rk].value->type == json_string))
													net = route->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(route->u.object.values[rk].name,"netmaskBits"))&&(route->u.object.values[rk].value->type == json_integer))
													bits = (int)route->u.object.values[rk].value->u.integer;
											}
										}
										if ((net)&&(bits > 0)) {
											char tmp[128];
											Utils::snprintf(tmp,sizeof(tmp),"%s/%d",net,bits);
											InetAddress n(tmp);
											if (((n.ss_family == AF_INET)&&(n.netmaskBits() < 32))||((n.ss_family == AF_INET6)&&(n.netmaskBits() < 128)))
												routes.insert(n);
										}

										sqlite3_reset(_sDeleteRoutesForNetwork);
										sqlite3_bind_text(_sDeleteRoutesForNetwork,1,nwids,16,SQLITE_STATIC);
										sqlite3_step(_sDeleteRoutesForNetwork);

										for(std::set<InetAddress>::const_iterator p(routes.begin());p!=routes.end();++p) {
											char ipBlob[16];
											sqlite3_reset(_sCreateRoute);
											sqlite3_bind_text(_sCreateRoute,1,nwids,16,SQLITE_STATIC);
											if (nodeId)
												sqlite3_bind_text(_sCreateRoute,2,nodeId,16,SQLITE_STATIC);
											else
												sqlite3_bind_null(_sCreateRoute,2);
											sqlite3_bind_blob(_sCreateRoute,3,InetAddressToBlob(ipBlob,&(*p)),16,SQLITE_STATIC);
											sqlite3_bind_int(_sCreateRoute,4,(int)p->netmaskBits());
											sqlite3_bind_int(_sCreateRoute,5,(p->ss_family == AF_INET6) ? 6 : 4);
											if (sqlite3_step(_sCreateRoute) != SQLITE_DONE)
												return 500;
										}
									}
								}
							} else if (!strcmp(j->u.object.values[k].name,"gateways")) {
								sqlite3_reset(_sDeleteGateways);
								sqlite3_bind_text(_sDeleteGateways,1,nwids,16,SQLITE_STATIC);
								sqlite3_step(_sDeleteGateways);
								if (j->u.object.values[k].value->type == json_array) {
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *gateway = j->u.object.values[k].value->u.array.values[kk];
										if ((gateway)&&(gateway->type == json_string)) {
											InetAddress gwip(gateway->u.string.ptr);
											int ipVersion = 0;
											if (gwip.ss_family == AF_INET)
												ipVersion = 4;
											else if (gwip.ss_family == AF_INET6)
												ipVersion = 6;
											if (ipVersion) {
												char ipBlob[16];
												sqlite3_reset(_sCreateGateway);
												sqlite3_bind_text(_sCreateGateway,1,nwids,16,SQLITE_STATIC);
												sqlite3_bind_blob(_sCreateGateway,2,InetAddressToBlob(ipBlob,&gwip),16,SQLITE_STATIC);
												sqlite3_bind_int(_sCreateGateway,3,ipVersion);
												sqlite3_bind_int(_sCreateGateway,4,(int)gwip.metric());
												sqlite3_step(_sCreateGateway);
											}
										}
									}
								}
							} else if (!strcmp(j->u.object.values[k].name,"ipAssignmentPools")) {
								if (j->u.object.values[k].value->type == json_array) {
									std::set< std::vector<InetAddress> > pools;
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *pool = j->u.object.values[k].value->u.array.values[kk];
										const char *routeIp = (const char *)0;
										const char *ipFirst = (const char *)0;
										const char *ipLast = (const char *)0;
										InetAddress nRoute;
										if ((pool)&&(pool->type == json_object)) {
											for(unsigned int rk=0;rk<pool->u.object.length;++rk) {
												if ((!strcmp(pool->u.object.values[rk].name,"network"))&&(pool->u.object.values[rk].value->type == json_string))
													routeIp = pool->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(pool->u.object.values[rk].name,"ipFirst"))&&(pool->u.object.values[rk].value->type == json_string))
													ipFirst = pool->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(pool->u.object.values[rk].name,"ipLast"))&&(pool->u.object.values[rk].value->type == json_string))
													ipLast = pool->u.object.values[rk].value->u.string.ptr;
											}
										}
										if (routeIp && ipFirst && ipLast) {
											char tmp[128];
											std::vector<InetAddress> v;
											Utils::snprintf(tmp,sizeof(tmp),"%s/0",routeIp);
											InetAddress nRoute(tmp);
											v.push_back(InetAddress (tmp));
											Utils::snprintf(tmp,sizeof(tmp),"%s/0",ipFirst);
											v.push_back(InetAddress (tmp));
											Utils::snprintf(tmp,sizeof(tmp),"%s/0",ipLast);
											v.push_back(InetAddress (tmp));

											if ((v[0].ss_family == v[1].ss_family) && (v[0].ss_family == v[2].ss_family))
												pools.insert(v);
										}

										sqlite3_reset(_sDeleteIpAssignmentPoolsForNetwork);
										sqlite3_bind_text(_sDeleteIpAssignmentPoolsForNetwork,1,nwids,16,SQLITE_STATIC);
										sqlite3_step(_sDeleteIpAssignmentPoolsForNetwork);

										for(std::set< std::vector<InetAddress> >::const_iterator p(pools.begin());p!=pools.end();++p) {
											char ipBlob[3*16];
											int i = 0;
											sqlite3_reset(_sCreateIpAssignmentPool);
											sqlite3_bind_text(_sCreateIpAssignmentPool,1,nwids,16,SQLITE_STATIC);
											for(std::vector<InetAddress>::const_iterator ip(p->begin());ip!=p->end();++i, ip++) {
												sqlite3_bind_blob(_sCreateIpAssignmentPool,i+2,InetAddressToBlob(ipBlob+i*16,&(*ip)),16,SQLITE_STATIC);
											}
											if (sqlite3_step(_sCreateIpAssignmentPool) != SQLITE_DONE)
												return 500;
										}
									}
								}
							} else if (!strcmp(j->u.object.values[k].name,"rules")) {
								if (j->u.object.values[k].value->type == json_array) {
									sqlite3_reset(_sDeleteRulesForNetwork);
									sqlite3_bind_text(_sDeleteRulesForNetwork,1,nwids,16,SQLITE_STATIC);
									sqlite3_step(_sDeleteRulesForNetwork);

									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *rj = j->u.object.values[k].value->u.array.values[kk];
										if ((rj)&&(rj->type == json_object)) {
											struct { // NULL pointers indicate missing or NULL -- wildcards
												const json_int_t *ruleNo;
												const char *nodeId;
												const json_int_t *vlanId;
												const json_int_t *vlanPcp;
												const json_int_t *etherType;
												const char *macSource;
												const char *macDest;
												const char *ipSource;
												const char *ipDest;
												const json_int_t *ipTos;
												const json_int_t *ipProtocol;
												const json_int_t *ipSourcePort;
												const json_int_t *ipDestPort;
												const json_int_t *flags;
												const json_int_t *invFlags;
												const char *action;
											} rule;
											memset(&rule,0,sizeof(rule));

											for(unsigned int rk=0;rk<rj->u.object.length;++rk) {
												if ((!strcmp(rj->u.object.values[rk].name,"ruleNo"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ruleNo = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"nodeId"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.nodeId = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"vlanId"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.vlanId = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"vlanPcp"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.vlanPcp = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"etherType"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.etherType = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"macSource"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.macSource = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"macDest"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.macDest = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"ipSource"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.ipSource = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"ipDest"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.ipDest = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"ipTos"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ipTos = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"ipProtocol"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ipProtocol = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"ipSourcePort"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ipSourcePort = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"ipDestPort"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ipDestPort = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"flags"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.flags = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"invFlags"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.invFlags = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"action"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.action = rj->u.object.values[rk].value->u.string.ptr;
											}

											if ((rule.ruleNo)&&(rule.action)&&(rule.action[0])) {
												char mactmp1[16],mactmp2[16];
												sqlite3_reset(_sCreateRule);
												sqlite3_bind_text(_sCreateRule,1,nwids,16,SQLITE_STATIC);
												sqlite3_bind_int64(_sCreateRule,2,*rule.ruleNo);

												// Optional values: null by default
												for(int i=3;i<=16;++i)
													sqlite3_bind_null(_sCreateRule,i);
												if ((rule.nodeId)&&(strlen(rule.nodeId) == 10)) sqlite3_bind_text(_sCreateRule,3,rule.nodeId,10,SQLITE_STATIC);
												if (rule.vlanId) sqlite3_bind_int(_sCreateRule,4,(int)*rule.vlanId);
												if (rule.vlanPcp) sqlite3_bind_int(_sCreateRule,5,(int)*rule.vlanPcp);
												if (rule.etherType) sqlite3_bind_int(_sCreateRule,6,(int)*rule.etherType & (int)0xffff);
												if (rule.macSource) {
													MAC m(rule.macSource);
													Utils::snprintf(mactmp1,sizeof(mactmp1),"%.12llx",(unsigned long long)m.toInt());
													sqlite3_bind_text(_sCreateRule,7,mactmp1,-1,SQLITE_STATIC);
												}
												if (rule.macDest) {
													MAC m(rule.macDest);
													Utils::snprintf(mactmp2,sizeof(mactmp2),"%.12llx",(unsigned long long)m.toInt());
													sqlite3_bind_text(_sCreateRule,8,mactmp2,-1,SQLITE_STATIC);
												}
												if (rule.ipSource) sqlite3_bind_text(_sCreateRule,9,rule.ipSource,-1,SQLITE_STATIC);
												if (rule.ipDest) sqlite3_bind_text(_sCreateRule,10,rule.ipDest,-1,SQLITE_STATIC);
												if (rule.ipTos) sqlite3_bind_int(_sCreateRule,11,(int)*rule.ipTos);
												if (rule.ipProtocol) sqlite3_bind_int(_sCreateRule,12,(int)*rule.ipProtocol);
												if (rule.ipSourcePort) sqlite3_bind_int(_sCreateRule,13,(int)*rule.ipSourcePort & (int)0xffff);
												if (rule.ipDestPort) sqlite3_bind_int(_sCreateRule,14,(int)*rule.ipDestPort & (int)0xffff);
												if (rule.flags) sqlite3_bind_int64(_sCreateRule,15,(int64_t)*rule.flags);
												if (rule.invFlags) sqlite3_bind_int64(_sCreateRule,16,(int64_t)*rule.invFlags);

												sqlite3_bind_text(_sCreateRule,17,rule.action,-1,SQLITE_STATIC);
												sqlite3_step(_sCreateRule);
											}
										}
									}
								}
							}

							if (stmt) {
								sqlite3_bind_text(stmt,2,nwids,16,SQLITE_STATIC);
								sqlite3_step(stmt);
								sqlite3_finalize(stmt);
							}
						}
					}
					json_value_free(j);
				}

				sqlite3_reset(_sSetNetworkRevision);
				sqlite3_bind_int64(_sSetNetworkRevision,1,revision += 1);
				sqlite3_bind_text(_sSetNetworkRevision,2,nwids,16,SQLITE_STATIC);
				sqlite3_step(_sSetNetworkRevision);

				return _doCPGet(path_copy,urlArgs,headers,body,responseBody,responseContentType);
			}

		} // else 404

	} // else 404

	return 404;
}

unsigned int SqliteNetworkController::handleControlPlaneHttpDELETE(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	if (path.empty())
		return 404;
	Mutex::Lock _l(_lock);

	if (path[0] == "network") {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			sqlite3_reset(_sGetNetworkById);
			sqlite3_bind_text(_sGetNetworkById,1,nwids,16,SQLITE_STATIC);
			if (sqlite3_step(_sGetNetworkById) != SQLITE_ROW)
				return 404;

			if (path.size() >= 3) {

				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

					sqlite3_reset(_sGetMember);
					sqlite3_bind_text(_sGetMember,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sGetMember,2,addrs,10,SQLITE_STATIC);
					if (sqlite3_step(_sGetMember) != SQLITE_ROW)
						return 404;

					sqlite3_reset(_sDeleteIpAllocations);
					sqlite3_bind_text(_sDeleteIpAllocations,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sDeleteIpAllocations,2,addrs,10,SQLITE_STATIC);
					if (sqlite3_step(_sDeleteIpAllocations) == SQLITE_DONE) {
						sqlite3_reset(_sDeleteMember);
						sqlite3_bind_text(_sDeleteMember,1,nwids,16,SQLITE_STATIC);
						sqlite3_bind_text(_sDeleteMember,2,addrs,10,SQLITE_STATIC);
						if (sqlite3_step(_sDeleteMember) != SQLITE_DONE)
							return 500;
					} else return 500;

					return 200;
				}

			} else {

				sqlite3_reset(_sDeleteNetwork);
				sqlite3_bind_text(_sDeleteNetwork,1,nwids,16,SQLITE_STATIC);
				return ((sqlite3_step(_sDeleteNetwork) == SQLITE_DONE) ? 200 : 500);

			}
		} // else 404

	} // else 404

	return 404;
}

unsigned int SqliteNetworkController::_doCPGet(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	// Assumes _lock is locked
	char json[16384];

	if ((path.size() > 0)&&(path[0] == "network")) {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			if (path.size() >= 3) {
				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

					sqlite3_reset(_sGetMember2);
					sqlite3_bind_text(_sGetMember2,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sGetMember2,2,addrs,10,SQLITE_STATIC);
					if (sqlite3_step(_sGetMember2) == SQLITE_ROW) {
						Utils::snprintf(json,sizeof(json),
							"{\n"
							"\t\"nwid\": \"%s\",\n"
							"\t\"address\": \"%s\",\n"
							"\t\"authorized\": %s,\n"
							"\t\"activeBridge\": %s,\n"
							"\t\"lastAt\": \"%s\",\n"
							"\t\"lastSeen\": %llu,\n"
							"\t\"firstSeen\": %llu,\n"
							"\t\"identity\": \"%s\",\n"
							"\t\"ipAssignments\": [",
							nwids,
							addrs,
							(sqlite3_column_int(_sGetMember2,0) > 0) ? "true" : "false",
							(sqlite3_column_int(_sGetMember2,1) > 0) ? "true" : "false",
							_jsonEscape((const char *)sqlite3_column_text(_sGetMember2,3)).c_str(),
							(unsigned long long)sqlite3_column_int64(_sGetMember2,4),
							(unsigned long long)sqlite3_column_int64(_sGetMember2,5),
							_jsonEscape((const char *)sqlite3_column_text(_sGetMember2,2)).c_str());
						responseBody = json;

						sqlite3_reset(_sGetIpAssignmentsForNode2);
						sqlite3_bind_text(_sGetIpAssignmentsForNode2,1,nwids,16,SQLITE_STATIC);
						sqlite3_bind_text(_sGetIpAssignmentsForNode2,2,addrs,10,SQLITE_STATIC);
						bool firstIp = true;
						while (sqlite3_step(_sGetIpAssignmentsForNode2) == SQLITE_ROW) {
							int ipversion = sqlite3_column_int(_sGetIpAssignmentsForNode2,2);
							InetAddress ip = BlobToInetAddress(
								(const char *)sqlite3_column_blob(_sGetIpAssignmentsForNode2,0),
								(unsigned int)sqlite3_column_int(_sGetIpAssignmentsForNode2,1),
								(ipversion == 6 ? 16 : 4)
							);
							responseBody.append(firstIp ? "\"" : ",\"");
							firstIp = false;
							responseBody.append(_jsonEscape(ip.toString()));
							responseBody.push_back('"');
						}

						responseBody.append("]");

						/* It's possible to get the actual netconf dictionary by including these
						 * three URL arguments. The member identity must be the string
						 * serialized identity of this member, and the signing identity must be
						 * the full secret identity of this network controller. The have revision
						 * is optional but would designate the revision our hypothetical client
						 * already has.
						 *
						 * This is primarily for testing and is not used in production. It makes
						 * it easy to test the entire network controller via its JSON API.
						 *
						 * If these arguments are included, three more object fields are returned:
						 * 'netconf', 'netconfResult', and 'netconfResultMessage'. These are all
						 * string fields and contain the actual netconf dictionary, the query
						 * result code, and any verbose message e.g. an error description. */
						std::map<std::string,std::string>::const_iterator memids(urlArgs.find("memberIdentity"));
						std::map<std::string,std::string>::const_iterator sigids(urlArgs.find("signingIdentity"));
						std::map<std::string,std::string>::const_iterator hrs(urlArgs.find("haveRevision"));
						if ((memids != urlArgs.end())&&(sigids != urlArgs.end())) {
							Dictionary netconf;
							Identity memid,sigid;
							try {
								if (memid.fromString(memids->second)&&sigid.fromString(sigids->second)&&sigid.hasPrivate()) {
									uint64_t hr = 0;
									if (hrs != urlArgs.end())
										hr = Utils::strToU64(hrs->second.c_str());
									const char *result = "";
									switch(this->doNetworkConfigRequest(InetAddress(),sigid,memid,nwid,Dictionary(),hr,netconf)) {
										case NetworkController::NETCONF_QUERY_OK: result = "OK"; break;
										case NetworkController::NETCONF_QUERY_OK_BUT_NOT_NEWER: result = "OK_BUT_NOT_NEWER"; break;
										case NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND: result = "OBJECT_NOT_FOUND"; break;
										case NetworkController::NETCONF_QUERY_ACCESS_DENIED: result = "ACCESS_DENIED"; break;
										case NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR: result = "INTERNAL_SERVER_ERROR"; break;
										default: result = "(unrecognized result code)"; break;
									}
									responseBody.append(",\n\t\"netconf\": \"");
									responseBody.append(_jsonEscape(netconf.toString().c_str()));
									responseBody.append("\",\n\t\"netconfResult\": \"");
									responseBody.append(result);
									responseBody.append("\",\n\t\"netconfResultMessage\": \"");
									responseBody.append(_jsonEscape(netconf["error"].c_str()));
									responseBody.append("\"");
								} else {
									responseBody.append(",\n\t\"netconf\": \"\",\n\t\"netconfResult\": \"INTERNAL_SERVER_ERROR\",\n\t\"netconfResultMessage\": \"invalid member or signing identity\"");
								}
							} catch ( ... ) {
								responseBody.append(",\n\t\"netconf\": \"\",\n\t\"netconfResult\": \"INTERNAL_SERVER_ERROR\",\n\t\"netconfResultMessage\": \"unexpected exception\"");
							}
						}

						responseBody.append("\n}\n");

						responseContentType = "application/json";
						return 200;
					} // else 404
				} // else 404
			} else {
				// get network info
				sqlite3_reset(_sGetNetworkById);
				sqlite3_bind_text(_sGetNetworkById,1,nwids,16,SQLITE_STATIC);
				if (sqlite3_step(_sGetNetworkById) == SQLITE_ROW) {
					Utils::snprintf(json,sizeof(json),
						"{\n"
						"\t\"nwid\": \"%s\",\n"
						"\t\"name\": \"%s\",\n"
						"\t\"private\": %s,\n"
						"\t\"enableBroadcast\": %s,\n"
						"\t\"allowPassiveBridging\": %s,\n"
						"\t\"v4AssignMode\": \"%s\",\n"
						"\t\"v6AssignMode\": \"%s\",\n"
						"\t\"multicastLimit\": %d,\n"
						"\t\"creationTime\": %llu,\n"
						"\t\"revision\": %llu,\n"
						"\t\"members\": [",
						nwids,
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,0)).c_str(),
						(sqlite3_column_int(_sGetNetworkById,1) > 0) ? "true" : "false",
						(sqlite3_column_int(_sGetNetworkById,2) > 0) ? "true" : "false",
						(sqlite3_column_int(_sGetNetworkById,3) > 0) ? "true" : "false",
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,4)).c_str(),
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,5)).c_str(),
						sqlite3_column_int(_sGetNetworkById,6),
						(unsigned long long)sqlite3_column_int64(_sGetNetworkById,7),
						(unsigned long long)sqlite3_column_int64(_sGetNetworkById,8));
					responseBody = json;

					sqlite3_reset(_sListNetworkMembers);
					sqlite3_bind_text(_sListNetworkMembers,1,nwids,16,SQLITE_STATIC);
					bool firstMember = true;
					while (sqlite3_step(_sListNetworkMembers) == SQLITE_ROW) {
						if (!firstMember)
							responseBody.push_back(',');
						responseBody.push_back('"');
						responseBody.append((const char *)sqlite3_column_text(_sListNetworkMembers,0));
						responseBody.push_back('"');
						firstMember = false;
					}
					responseBody.append("],\n\t\"relays\": [");

					sqlite3_reset(_sGetRelays);
					sqlite3_bind_text(_sGetRelays,1,nwids,16,SQLITE_STATIC);
					bool firstRelay = true;
					while (sqlite3_step(_sGetRelays) == SQLITE_ROW) {
						responseBody.append(firstRelay ? "\n\t\t" : ",\n\t\t");
						firstRelay = false;
						responseBody.append("{\"address\":\"");
						responseBody.append((const char *)sqlite3_column_text(_sGetRelays,0));
						responseBody.append("\",\"phyAddress\":\"");
						responseBody.append(_jsonEscape((const char *)sqlite3_column_text(_sGetRelays,1)));
						responseBody.append("\"}");
					}
					responseBody.append("],\n\t\"gateways\": [");

					sqlite3_reset(_sGetGateways);
					sqlite3_bind_text(_sGetGateways,1,nwids,16,SQLITE_STATIC);
					bool firstGateway = true;
					while (sqlite3_step(_sGetGateways) == SQLITE_ROW) {
						char tmp[128];
						const unsigned char *ip = (const unsigned char *)sqlite3_column_blob(_sGetGateways,0);
						switch(sqlite3_column_int(_sGetGateways,1)) { // ipVersion
							case 4:
								Utils::snprintf(tmp,sizeof(tmp),"%s%d.%d.%d.%d/%d\"",
									(firstGateway) ? "\"" : ",\"",
									(int)ip[0],
									(int)ip[1],
									(int)ip[2],
									(int)ip[3],
									(int)sqlite3_column_int(_sGetGateways,2)); // metric
								break;
							case 6:
								Utils::snprintf(tmp,sizeof(tmp),"%s%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x/%d\"",
									(firstGateway) ? "\"" : ",\"",
									(int)ip[0],
									(int)ip[1],
									(int)ip[2],
									(int)ip[3],
									(int)ip[4],
									(int)ip[5],
									(int)ip[6],
									(int)ip[7],
									(int)ip[8],
									(int)ip[9],
									(int)ip[10],
									(int)ip[11],
									(int)ip[12],
									(int)ip[13],
									(int)ip[14],
									(int)ip[15],
									(int)sqlite3_column_int(_sGetGateways,2)); // metric
								break;
						}
						responseBody.append(tmp);
						firstGateway = false;
					}
					responseBody.append("],\n\t\"routes\": [");

					sqlite3_reset(_sGetRoutes);
					sqlite3_bind_text(_sGetRoutes,1,nwids,16,SQLITE_STATIC);
					bool firstRoute = true;
					while (sqlite3_step(_sGetRoutes) == SQLITE_ROW) {
						responseBody.append(firstRoute ? "\n\t\t" : ",\n\t\t");
						firstRoute = false;
						//nodeId is first
						InetAddress ipp = BlobToInetAddress(
							(const char *)sqlite3_column_blob(_sGetRoutes,1),
							(unsigned int)sqlite3_column_int(_sGetRoutes,2),
							sqlite3_column_int(_sGetRoutes,3)
						);
						Utils::snprintf(json,sizeof(json),"{\"network\":\"%s\",\"netmaskBits\":%u}",
							_jsonEscape(ipp.toIpString()).c_str(),
							ipp.netmaskBits());
						responseBody.append(json);
					}
					responseBody.append("],\n\t\"ipAssignmentPools\": [");

					sqlite3_reset(_sGetIpAssignmentPools2);
					sqlite3_bind_text(_sGetIpAssignmentPools2,1,nwids,16,SQLITE_STATIC);
					bool firstIpAssignmentPool = true;
					while (sqlite3_step(_sGetIpAssignmentPools2) == SQLITE_ROW) {
						responseBody.append(firstIpAssignmentPool ? "\n\t\t" : ",\n\t\t");
						firstIpAssignmentPool = false;
						InetAddress ipNetwork = BlobToInetAddress(
							(const char *)sqlite3_column_blob(_sGetIpAssignmentPools2,0),
							(unsigned int)sqlite3_column_int(_sGetIpAssignmentPools2,3),
							sqlite3_column_int(_sGetIpAssignmentPools2,4)
						);
						InetAddress ipFirst = BlobToInetAddress(
							(const char *)sqlite3_column_blob(_sGetIpAssignmentPools2,1),
							(unsigned int)sqlite3_column_int(_sGetIpAssignmentPools2,3),
							sqlite3_column_int(_sGetIpAssignmentPools2,4)
						);
						InetAddress ipLast = BlobToInetAddress(
							(const char *)sqlite3_column_blob(_sGetIpAssignmentPools2,2),
							(unsigned int)sqlite3_column_int(_sGetIpAssignmentPools2,3),
							sqlite3_column_int(_sGetIpAssignmentPools2,4)
						);
						Utils::snprintf(json,sizeof(json),"{\"network\":\"%s\",\"ipFirst\":\"%s\", \"ipLast\":\"%s\"}",
							_jsonEscape(ipNetwork.toIpString()).c_str(),
							_jsonEscape(ipFirst.toIpString()).c_str(),
							_jsonEscape(ipLast.toIpString()).c_str()
						);
						responseBody.append(json);
					}
					responseBody.append("],\n\t\"rules\": [");

					sqlite3_reset(_sListRules);
					sqlite3_bind_text(_sListRules,1,nwids,16,SQLITE_STATIC);
					bool firstRule = true;
					while (sqlite3_step(_sListRules) == SQLITE_ROW) {
						responseBody.append(firstRule ? "\n\t{\n" : ",{\n");
						Utils::snprintf(json,sizeof(json),"\t\t\"ruleNo\": %lld,\n",sqlite3_column_int64(_sListRules,0));
						responseBody.append(json);
						if (sqlite3_column_type(_sListRules,1) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"nodeId\": \"%s\",\n",(const char *)sqlite3_column_text(_sListRules,1));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,2) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"vlanId\": %d,\n",sqlite3_column_int(_sListRules,2));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,3) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"vlanPcp\": %d,\n",sqlite3_column_int(_sListRules,3));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,4) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"etherType\": %d,\n",sqlite3_column_int(_sListRules,4));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,5) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"macSource\": \"%s\",\n",MAC((const char *)sqlite3_column_text(_sListRules,5)).toString().c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,6) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"macDest\": \"%s\",\n",MAC((const char *)sqlite3_column_text(_sListRules,6)).toString().c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,7) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipSource\": \"%s\",\n",_jsonEscape((const char *)sqlite3_column_text(_sListRules,7)).c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,8) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipDest\": \"%s\",\n",_jsonEscape((const char *)sqlite3_column_text(_sListRules,8)).c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,9) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipTos\": %d,\n",sqlite3_column_int(_sListRules,9));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,10) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipProtocol\": %d,\n",sqlite3_column_int(_sListRules,10));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,11) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipSourcePort\": %d,\n",sqlite3_column_int(_sListRules,11));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,12) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipDestPort\": %d,\n",sqlite3_column_int(_sListRules,12));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,13) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"flags\": %lu,\n",(unsigned long)sqlite3_column_int64(_sListRules,13));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,14) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"invFlags\": %lu,\n",(unsigned long)sqlite3_column_int64(_sListRules,14));
							responseBody.append(json);
						}
						responseBody.append("\t\t\"action\": \"");
						responseBody.append(_jsonEscape( (sqlite3_column_type(_sListRules,15) == SQLITE_NULL) ? "drop" : (const char *)sqlite3_column_text(_sListRules,15) ));
						responseBody.append("\"\n\t}");
					}

					responseBody.append("]\n}\n");
					responseContentType = "application/json";
					return 200;
				} // else 404
			}
		} else if (path.size() == 1) {
			// list networks
			sqlite3_reset(_sListNetworks);
			responseContentType = "application/json";
			responseBody = "[";
			bool first = true;
			while (sqlite3_step(_sListNetworks) == SQLITE_ROW) {
				if (first) {
					first = false;
					responseBody.push_back('"');
				} else responseBody.append(",\"");
				responseBody.append((const char *)sqlite3_column_text(_sListNetworks,0));
				responseBody.push_back('"');
			}
			responseBody.push_back(']');
			return 200;
		} // else 404

	} else {
		// GET /controller returns status and API version if controller is supported
		Utils::snprintf(json,sizeof(json),"{\n\t\"controller\": true,\n\t\"apiVersion\": %d,\n\t\"clock\": %llu\n}",ZT_NETCONF_CONTROLLER_API_VERSION,(unsigned long long)OSUtils::now());
		responseBody = json;
		responseContentType = "applicaiton/json";
		return 200;
	}

	return 404;
}

} // namespace ZeroTier
