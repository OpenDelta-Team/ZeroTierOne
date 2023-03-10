// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::{BTreeMap, BTreeSet};
use std::hash::Hash;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::InetAddress;
use zerotier_network_hypervisor::vl2::rule::Rule;
use zerotier_network_hypervisor::vl2::{IpRoute, NetworkId};

use crate::database::Database;
use crate::model::Member;

pub const CREDENTIAL_WINDOW_SIZE_DEFAULT: u64 = 1000 * 60 * 60; // 1 hour

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Default, Debug)]
pub struct Ipv4AssignMode {
    pub zt: bool,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Default, Debug)]
pub struct Ipv6AssignMode {
    pub zt: bool,
    pub rfc4193: bool,
    #[serde(rename = "6plane")]
    pub _6plane: bool,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, PartialOrd, Ord, Hash, Debug)]
pub struct IpAssignmentPool {
    #[serde(rename = "ipRangeStart")]
    pub ip_range_start: InetAddress,
    #[serde(rename = "ipRangeEnd")]
    pub ip_range_end: InetAddress,
}

/// Virtual network configuration.
///
/// This contains only fields of relevance to the controller. Other fields can be tracked by various
/// database implementations such as row last modified, creation time, ownership in an admin panel, etc.
#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Debug)]
pub struct Network {
    pub id: NetworkId,

    /// Network name that's sent to network members
    #[serde(skip_serializing_if = "String::is_empty")]
    #[serde(default)]
    pub name: String,

    /// Guideline for the maximum number of multicast recipients on a network (not a hard limit).
    /// Setting to zero disables multicast entirely. The default is used if this is not set.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "multicastLimit")]
    pub multicast_limit: Option<u32>,

    /// If true, this network supports ff:ff:ff:ff:ff:ff Ethernet broadcast.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "enableBroadcast")]
    #[serde(default)]
    pub enable_broadcast: Option<bool>,

    /// Auto IP assignment mode(s) for IPv4 addresses.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "v4AssignMode")]
    #[serde(default)]
    pub v4_assign_mode: Option<Ipv4AssignMode>,

    /// Auto IP assignment mode(s) for IPv6 addresses.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "v6AssignMode")]
    #[serde(default)]
    pub v6_assign_mode: Option<Ipv6AssignMode>,

    /// IPv4 or IPv6 auto-assignment pools available, must be present to use 'zt' mode.
    #[serde(skip_serializing_if = "BTreeSet::is_empty")]
    #[serde(rename = "ipAssignmentPools")]
    #[serde(default)]
    pub ip_assignment_pools: BTreeSet<IpAssignmentPool>,

    /// IPv4 or IPv6 routes to advertise.
    #[serde(rename = "ipRoutes")]
    #[serde(skip_serializing_if = "BTreeSet::is_empty")]
    #[serde(default)]
    pub ip_routes: BTreeSet<IpRoute>,

    /// DNS records to push to members.
    #[serde(default)]
    #[serde(skip_serializing_if = "BTreeMap::is_empty")]
    pub dns: BTreeMap<String, BTreeSet<InetAddress>>,

    /// Network rule set. (Default: one 'accept' rule.)
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub rules: Option<Vec<Rule>>,

    /// If set this overrides the default TTL for certificates and credentials.
    ///
    /// Making it smaller causes deauthorized nodes to fall out of the window more rapidly but can
    /// come at the expense of reliability if it's too short for everyone to update their certs
    /// on time from the controller. Note that revocations are also used to deauthorize nodes
    /// promptly, so nodes will still deauthorize quickly even if the window is long.
    ///
    /// Usually this does not need to be changed.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "credentialTtl")]
    pub credential_ttl: Option<u64>,

    /// Minimum supported ZeroTier protocol version for this network (default: undefined, up to members)
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "minSupportedVersion")]
    pub min_supported_version: Option<u32>,

    /// MTU inside the virtual network, default of 2800 is used if not set.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mtu: Option<u16>,

    /// If true the network has access control, which is usually what you want and is the default if not specified.
    #[serde(default = "troo")]
    pub private: bool,

    /// If true this network will add not-authorized members for anyone who requests a config.
    #[serde(rename = "learnMembers")]
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub learn_members: Option<bool>,
}

impl Hash for Network {
    #[inline]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.id.hash(state)
    }
}

#[inline(always)]
fn troo() -> bool {
    true
}

impl Network {
    pub fn new(id: NetworkId) -> Self {
        Network {
            id,
            name: String::new(),
            multicast_limit: None,
            enable_broadcast: None,
            v4_assign_mode: None,
            v6_assign_mode: None,
            ip_assignment_pools: BTreeSet::new(),
            ip_routes: BTreeSet::new(),
            dns: BTreeMap::new(),
            rules: None,
            credential_ttl: None,
            min_supported_version: None,
            mtu: None,
            private: true,
            learn_members: None,
        }
    }

    /// Check member IP assignments and return 'true' if IP assignments were created or modified.
    pub async fn assign_ip_addresses<DatabaseImpl: Database + ?Sized>(&self, database: &DatabaseImpl, member: &mut Member) -> bool {
        let mut modified = false;

        if self.v4_assign_mode.as_ref().map_or(false, |m| m.zt) {
            if !member.ip_assignments.iter().any(|ip| ip.is_ipv4()) {
                'ip_search: for pool in self.ip_assignment_pools.iter() {
                    if pool.ip_range_start.is_ipv4() && pool.ip_range_end.is_ipv4() {
                        let mut ip_ptr = u32::from_be_bytes(pool.ip_range_start.ip_bytes().try_into().unwrap());
                        let ip_end = u32::from_be_bytes(pool.ip_range_end.ip_bytes().try_into().unwrap());
                        while ip_ptr < ip_end {
                            for route in self.ip_routes.iter() {
                                let ip = InetAddress::from_ip_port(&ip_ptr.to_be_bytes(), route.target.port()); // IP/bits
                                if ip.is_within(&route.target) {
                                    if let Ok(is_ip_assigned) = database.is_ip_assigned(self.id, &ip).await {
                                        if !is_ip_assigned {
                                            modified = true;
                                            let _ = member.ip_assignments.insert(ip);
                                            break 'ip_search;
                                        }
                                    } else {
                                        return false;
                                    }
                                }
                            }
                            ip_ptr += 1;
                        }
                    }
                }
            }
        }

        if self.v6_assign_mode.as_ref().map_or(false, |m| m.zt) {
            if !member.ip_assignments.iter().any(|ip| ip.is_ipv6()) {
                'ip_search: for pool in self.ip_assignment_pools.iter() {
                    if pool.ip_range_start.is_ipv6() && pool.ip_range_end.is_ipv6() {
                        let mut ip_ptr = u128::from_be_bytes(pool.ip_range_start.ip_bytes().try_into().unwrap());
                        let ip_end = u128::from_be_bytes(pool.ip_range_end.ip_bytes().try_into().unwrap());
                        while ip_ptr < ip_end {
                            for route in self.ip_routes.iter() {
                                let ip = InetAddress::from_ip_port(&ip_ptr.to_be_bytes(), route.target.port()); // IP/bits
                                if ip.is_within(&route.target) {
                                    if let Ok(is_ip_assigned) = database.is_ip_assigned(self.id, &ip).await {
                                        if !is_ip_assigned {
                                            modified = true;
                                            let _ = member.ip_assignments.insert(ip);
                                            break 'ip_search;
                                        }
                                    } else {
                                        return false;
                                    }
                                }
                            }
                            ip_ptr += 1;
                        }
                    }
                }
            }
        }

        return modified;
    }
}
