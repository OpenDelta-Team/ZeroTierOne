/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/// The poly1305 message authentication function.
pub struct Poly1305(gcrypt::mac::Mac);

pub const POLY1305_ONE_TIME_KEY_SIZE: usize = 32;
pub const POLY1305_MAC_SIZE: usize = 16;

impl Poly1305 {
    pub fn new(key: &[u8]) -> Option<Poly1305> {
        if key.len() == 32 {
            gcrypt::mac::Mac::new(gcrypt::mac::Algorithm::Poly1305).map_or(None, |mut poly| {
                let _ = poly.set_key(key);
                Some(Poly1305(poly))
            })
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn update(&mut self, data: &[u8]) {
        let _ = self.0.update(data);
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; POLY1305_MAC_SIZE] {
        let mut mac = [0_u8; POLY1305_MAC_SIZE];
        let _ = self.0.get_mac(&mut mac);
        mac
    }
}
