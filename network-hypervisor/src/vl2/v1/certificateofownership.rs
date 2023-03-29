use std::collections::HashSet;
use std::io::Write;

use crate::vl1::identity::{Identity, IdentitySecret};
use crate::vl1::{Address, InetAddress, PartialAddress, MAC};
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::error::InvalidParameterError;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub enum Thing {
    Ipv4([u8; 4]),
    Ipv6([u8; 16]),
    Mac(MAC),
}

impl Thing {
    /// Get the type ID for this "thing."
    pub fn type_id(&self) -> u8 {
        match self {
            Self::Mac(_) => 1,
            Self::Ipv4(_) => 2,
            Self::Ipv6(_) => 3,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateOfOwnership {
    pub network_id: u64, // legacy 64-bit network ID
    pub timestamp: i64,
    pub things: HashSet<Thing>,
    pub issued_to: u64, // legacy 40-bit address
    pub signature: ArrayVec<u8, { Identity::MAX_SIGNATURE_SIZE }>,
}

impl CertificateOfOwnership {
    /// Create a new empty and unsigned certificate.
    pub fn new(network_id: &NetworkId, timestamp: i64, issued_to: &Address) -> Self {
        Self {
            network_id: network_id.to_legacy_u64(),
            timestamp,
            things: HashSet::with_capacity(4),
            issued_to: issued_to.legacy_u64(),
            signature: ArrayVec::new(),
        }
    }

    /// Add an IP address to this certificate.
    pub fn add_ip(&mut self, ip: &InetAddress) {
        if ip.is_ipv4() {
            let _ = self.things.insert(Thing::Ipv4(ip.ip_bytes().try_into().unwrap()));
        } else if ip.is_ipv6() {
            let _ = self.things.insert(Thing::Ipv6(ip.ip_bytes().try_into().unwrap()));
        }
    }

    /// Add a MAC address to this certificate.
    pub fn add_mac(&mut self, mac: MAC) {
        let _ = self.things.insert(Thing::Mac(mac));
    }

    fn internal_to_bytes(&self, for_sign: bool, signed_by: &Address) -> Option<Vec<u8>> {
        if self.things.len() > 0xffff {
            return None;
        }
        let mut v = Vec::with_capacity(256);
        if for_sign {
            let _ = v.write_all(&[0x7fu8; 8]);
        }
        let _ = v.write_all(&self.network_id.to_be_bytes());
        let _ = v.write_all(&self.timestamp.to_be_bytes());
        let _ = v.write_all(&[0u8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]); // obsolete flags and ID fields
        let _ = v.write_all(&(self.things.len() as u16).to_be_bytes());
        for t in self.things.iter() {
            match t {
                Thing::Ipv4(ip) => {
                    v.push(2);
                    let _ = v.write_all(ip);
                    let _ = v.write_all(&[0u8; 12]);
                }
                Thing::Ipv6(ip) => {
                    v.push(3);
                    let _ = v.write_all(ip);
                }
                Thing::Mac(m) => {
                    v.push(1);
                    let mut tmp = [0u8; 16];
                    tmp[..6].copy_from_slice(&m.to_bytes());
                    let _ = v.write_all(&tmp);
                }
            }
        }
        let _ = v.write_all(&self.issued_to.to_be_bytes()[3..8]);
        let _ = v.write_all(signed_by.legacy_bytes());
        if for_sign {
            v.push(0);
            v.push(0);
            let _ = v.write_all(&[0x7fu8; 8]);
        } else {
            v.push(1);
            v.push(0);
            v.push(96); // size of legacy signature, 16 bits
            let _ = v.write_all(self.signature.as_bytes());
            v.push(0);
            v.push(0);
        }
        return Some(v);
    }

    #[inline(always)]
    pub fn to_bytes(&self, signed_by: &Address) -> Option<Vec<u8>> {
        self.internal_to_bytes(false, signed_by)
    }

    /// Decode a V1 legacy format certificate of ownership in byte format.
    /// The certificate and the current position slice are returned so multiple certs can be easily read from a buffer.
    pub fn from_bytes(mut b: &[u8]) -> Result<(Self, &[u8]), InvalidParameterError> {
        if b.len() < 30 {
            return Err(InvalidParameterError("incomplete"));
        }
        let network_id = u64::from_be_bytes(b[0..8].try_into().unwrap());
        let timestamp = i64::from_be_bytes(b[8..16].try_into().unwrap());
        let thing_count = u16::from_be_bytes(b[28..30].try_into().unwrap());
        let mut things: HashSet<Thing> = HashSet::with_capacity(thing_count as usize);
        b = &b[30..];
        for _ in 0..thing_count {
            if b.len() < 17 {
                return Err(InvalidParameterError("incomplete"));
            }
            match b[0] {
                1 => {
                    let _ = things.insert(Thing::Mac(MAC::from_bytes(&b[1..7]).ok_or(InvalidParameterError("invalid MAC"))?));
                }
                2 => {
                    let _ = things.insert(Thing::Ipv4(b[1..5].try_into().unwrap()));
                }
                3 => {
                    let _ = things.insert(Thing::Ipv6(b[1..17].try_into().unwrap()));
                }
                _ => {
                    return Err(InvalidParameterError("unknown thing type"));
                }
            }
            b = &b[17..];
        }
        const END_LEN: usize = 5 + 5 + 3 + 96 + 2;
        if b.len() < END_LEN {
            return Err(InvalidParameterError("incomplete"));
        }
        Ok((
            Self {
                network_id: NetworkId::from_legacy_u64(network_id)?.to_legacy_u64(),
                timestamp,
                things,
                issued_to: PartialAddress::from_bytes(&b[..5])?.legacy_u64(),
                signature: {
                    let mut s = ArrayVec::new();
                    s.push_slice(&b[13..109]);
                    s
                },
            },
            &b[END_LEN..],
        ))
    }

    /// Sign certificate of ownership for use by V1 nodes.
    pub fn sign(&mut self, issuer: &IdentitySecret, issued_to: &Identity) -> bool {
        self.issued_to = issued_to.address.legacy_u64();
        if let Some(to_sign) = self.internal_to_bytes(true, &issuer.public.address) {
            self.signature = issuer.sign(&to_sign.as_slice());
            return true;
        }
        return false;
    }
}
