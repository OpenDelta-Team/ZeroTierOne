mod certificateofmembership;
mod certificateofownership;
pub mod networkconfig;
mod revocation;
mod tag;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum CredentialType {
    Null = 0u8,
    CertificateOfMembership = 1,
    Capability = 2,
    Tag = 3,
    CertificateOfOwnership = 4,
    Revocation = 5,
}

pub use certificateofmembership::CertificateOfMembership;
pub use certificateofownership::{CertificateOfOwnership, Thing};
pub use revocation::Revocation;
pub use tag::Tag;
