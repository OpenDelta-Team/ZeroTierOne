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

package com.zerotier.sdk;

import java.net.InetSocketAddress;

public final class VirtualNetworkRoute implements Comparable<VirtualNetworkRoute>
{
	private VirtualNetworkRoute() {
        target = null;
        via = null;
        flags = 0;
        metric = 0;
    }

	/**
	 * Target network / netmask bits (in port field) or NULL or 0.0.0.0/0 for default
	 */
	public InetSocketAddress target;

	/**
	 * Gateway IP address (port ignored) or NULL (family == 0) for LAN-local (no gateway)
	 */
	public InetSocketAddress via;

	/**
	 * Route flags
	 */
	public int flags;

	/**
	 * Route metric (not currently used)
	 */
	public int metric;

	@Override
    public String toString() {
	    StringBuilder sb = new StringBuilder();
	    sb.append(target.toString());
        if (via != null) {
            sb.append(via.toString());
        }
        return sb.toString();
    }

    @Override
    public int compareTo(VirtualNetworkRoute other) {
        throw new RuntimeException("Unimplemented");
    }

    @Override
    public boolean equals(Object o) {

        if (!(o instanceof VirtualNetworkRoute)) {
            return false;
        }

        VirtualNetworkRoute other = (VirtualNetworkRoute) o;

        boolean targetEquals;
        if (target == null) {
            //noinspection RedundantIfStatement
            if (other.target == null) {
                targetEquals = true;
            } else {
                targetEquals = false;
            }
        } else {
            if (other.target == null) {
                targetEquals = false;
            } else {
                targetEquals = target.equals(other.target);
            }
        }

        if (!targetEquals) {
            return false;
        }

        boolean viaEquals;
        if (via == null) {
            //noinspection RedundantIfStatement
            if (other.via == null) {
                viaEquals = true;
            } else {
                viaEquals = false;
            }
        } else {
            if (other.via == null) {
                viaEquals = false;
            } else {
                viaEquals = via.equals(other.via);
            }
        }

        if (!viaEquals) {
            return false;
        }

        if (flags != other.flags) {
            return false;
        }

        if (metric != other.metric) {
            return false;
        }

        return true;
    }
}
