/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::fmt::Debug;
use std::hash::Hash;

use serde::ser::SerializeTuple;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

use crate::hex;

/// Fixed size Serde serializable byte array.
/// This makes it easier to deal with blobs larger than 32 bytes (due to serde array limitations)
#[repr(transparent)]
#[derive(Clone, Eq, PartialEq)]
pub struct Blob<const L: usize>([u8; L]);

impl<const L: usize> Blob<L> {
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; L] {
        &self.0
    }

    #[inline(always)]
    pub const fn len(&self) -> usize {
        L
    }
}

impl<const L: usize> From<[u8; L]> for Blob<L> {
    #[inline(always)]
    fn from(a: [u8; L]) -> Self {
        Self(a)
    }
}

impl<const L: usize> From<&[u8; L]> for Blob<L> {
    #[inline(always)]
    fn from(a: &[u8; L]) -> Self {
        Self(a.clone())
    }
}

impl<const L: usize> Default for Blob<L> {
    #[inline(always)]
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

impl<const L: usize> AsRef<[u8; L]> for Blob<L> {
    #[inline(always)]
    fn as_ref(&self) -> &[u8; L] {
        &self.0
    }
}

impl<const L: usize> AsMut<[u8; L]> for Blob<L> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut [u8; L] {
        &mut self.0
    }
}

impl<const L: usize> ToString for Blob<L> {
    #[inline(always)]
    fn to_string(&self) -> String {
        hex::to_string(&self.0)
    }
}

impl<const L: usize> PartialOrd for Blob<L> {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        self.0.partial_cmp(&other.0)
    }
}

impl<const L: usize> Ord for Blob<L> {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.0.cmp(&other.0)
    }
}

impl<const L: usize> Hash for Blob<L> {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.0.hash(state);
    }
}

impl<const L: usize> Debug for Blob<L> {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl<const L: usize> Serialize for Blob<L> {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut t = serializer.serialize_tuple(L)?;
        for i in self.0.iter() {
            t.serialize_element(i)?;
        }
        t.end()
    }
}

struct BlobVisitor<const L: usize>;

impl<'de, const L: usize> serde::de::Visitor<'de> for BlobVisitor<L> {
    type Value = Blob<L>;

    #[inline]
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str(format!("array of {} bytes", L).as_str())
    }

    #[inline]
    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
    where
        A: serde::de::SeqAccess<'de>,
    {
        let mut blob = Blob::<L>::default();
        for i in 0..L {
            blob.0[i] = seq.next_element()?.ok_or_else(|| serde::de::Error::invalid_length(i, &self))?;
        }
        Ok(blob)
    }
}

impl<'de, const L: usize> Deserialize<'de> for Blob<L> {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_tuple(L, BlobVisitor::<L>)
    }
}
