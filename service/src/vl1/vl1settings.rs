// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::HashSet;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::InetAddress;

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct VL1Settings {
    /// Primary ZeroTier port that is always bound, default is 9993.
    pub fixed_ports: HashSet<u16>,

    /// Number of additional random ports to bind.
    pub random_port_count: usize,

    /// Enable uPnP, NAT-PMP, and other router port mapping technologies?
    pub port_mapping: bool,

    /// Interface name prefix blacklist for local bindings (not remote IPs).
    pub interface_prefix_blacklist: HashSet<String>,

    /// IP/bits CIDR blacklist for local bindings (not remote IPs).
    pub cidr_blacklist: HashSet<InetAddress>,
}

impl VL1Settings {
    #[cfg(target_os = "macos")]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 11] = ["lo", "utun", "gif", "stf", "iptap", "pktap", "feth", "zt", "llw", "anpi", "bridge"];

    #[cfg(target_os = "linux")]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 5] = ["lo", "tun", "tap", "ipsec", "zt"];

    #[cfg(windows)]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 0] = [];
}

impl Default for VL1Settings {
    fn default() -> Self {
        Self {
            fixed_ports: HashSet::from([9993u16]),
            random_port_count: 5,
            port_mapping: true,
            interface_prefix_blacklist: Self::DEFAULT_PREFIX_BLACKLIST.iter().map(|s| s.to_string()).collect(),
            cidr_blacklist: HashSet::new(),
        }
    }
}

/// A list of unassigned or obsolete ports under 1024 that could possibly be squatted.
pub const UNASSIGNED_PRIVILEGED_PORTS: [u16; 299] = [
    4, 6, 8, 10, 12, 14, 15, 16, 26, 28, 30, 32, 34, 36, 40, 60, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 285, 288, 289, 290, 291, 292,
    293, 294, 295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307, 323, 324, 325, 326, 327, 328, 329, 330, 331, 332, 334, 335, 336, 337,
    338, 339, 340, 341, 342, 343, 703, 708, 713, 714, 715, 716, 717, 718, 719, 720, 721, 722, 723, 724, 725, 726, 727, 728, 732, 733, 734, 735, 736,
    737, 738, 739, 740, 743, 745, 746, 755, 756, 766, 768, 778, 779, 781, 782, 783, 784, 785, 786, 787, 788, 789, 790, 791, 792, 793, 794, 795, 796,
    797, 798, 799, 802, 803, 804, 805, 806, 807, 808, 809, 811, 812, 813, 814, 815, 816, 817, 818, 819, 820, 821, 822, 823, 824, 825, 826, 827, 834,
    835, 836, 837, 838, 839, 840, 841, 842, 843, 844, 845, 846, 849, 850, 851, 852, 853, 854, 855, 856, 857, 858, 859, 862, 863, 864, 865, 866, 867,
    868, 869, 870, 871, 872, 874, 875, 876, 877, 878, 879, 880, 881, 882, 883, 884, 885, 889, 890, 891, 892, 893, 894, 895, 896, 897, 898, 899, 904,
    905, 906, 907, 908, 909, 910, 911, 914, 915, 916, 917, 918, 919, 920, 921, 922, 923, 924, 925, 926, 927, 928, 929, 930, 931, 932, 933, 934, 935,
    936, 937, 938, 939, 940, 941, 942, 943, 944, 945, 946, 947, 948, 949, 950, 951, 952, 953, 954, 955, 956, 957, 958, 959, 960, 961, 962, 963, 964,
    965, 966, 967, 968, 969, 970, 971, 972, 973, 974, 975, 976, 977, 978, 979, 980, 981, 982, 983, 984, 985, 986, 987, 988, 1001, 1002, 1003, 1004,
    1005, 1006, 1007, 1008, 1009, 1023,
];
