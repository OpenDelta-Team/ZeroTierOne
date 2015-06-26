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

#ifndef ZT_PHY_HPP
#define ZT_PHY_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <list>
#include <stdexcept>

#if defined(_WIN32) || defined(_WIN64)

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#define ZT_PHY_SOCKFD_TYPE SOCKET
#define ZT_PHY_SOCKFD_NULL (INVALID_SOCKET)
#define ZT_PHY_SOCKFD_VALID(s) ((s) != INVALID_SOCKET)
#define ZT_PHY_CLOSE_SOCKET(s) ::closesocket(s)
#define ZT_PHY_MAX_SOCKETS (FD_SETSIZE)
#define ZT_PHY_SOCKADDR_STORAGE_TYPE struct sockaddr_storage

#else // not Windows

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define ZT_PHY_SOCKFD_TYPE int
#define ZT_PHY_SOCKFD_NULL (-1)
#define ZT_PHY_SOCKFD_VALID(s) ((s) > -1)
#define ZT_PHY_CLOSE_SOCKET(s) ::close(s)
#define ZT_PHY_MAX_SOCKETS (FD_SETSIZE)
#define ZT_PHY_SOCKADDR_STORAGE_TYPE struct sockaddr_storage

#endif // Windows or not

namespace ZeroTier {

/**
 * Opaque socket type
 */
typedef void PhySocket;

/**
 * Simple templated non-blocking sockets implementation
 *
 * Yes there is boost::asio and libuv, but I like small binaries and I hate
 * build dependencies. Both drag in a whole bunch of pasta with them.
 *
 * This class is templated on a pointer to a handler class which must
 * implement the following functions:
 *
 * phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len)
 * phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
 * phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
 * phyOnTcpClose(PhySocket *sock,void **uptr)
 * phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
 * phyOnTcpWritable(PhySocket *sock,void **uptr)
 *
 * These templates typically refer to function objects. Templates are used to
 * avoid the call overhead of indirection, which is surprisingly high for high
 * bandwidth applications pushing a lot of packets.
 *
 * The 'sock' pointer above is an opaque pointer to a socket. Each socket
 * has a 'uptr' user-settable/modifiable pointer associated with it, which
 * can be set on bind/connect calls and is passed as a void ** to permit
 * resetting at any time. The ACCEPT handler takes two sets of sock and
 * uptr: sockL and uptrL for the listen socket, and sockN and uptrN for
 * the new TCP connection socket that has just been created.
 *
 * Handlers are always called. On outgoing TCP connection, CONNECT is always
 * called on either success or failure followed by DATA and/or WRITABLE as
 * indicated. On socket close, handlers are called unless close() is told
 * explicitly not to call handlers. It is safe to close a socket within a
 * handler, and in that case close() can be told not to call handlers to
 * prevent recursion.
 *
 * This isn't thread-safe with the exception of whack(), which is safe to
 * call from another thread to abort poll().
 */
template <typename HANDLER_PTR_TYPE>
class Phy
{
private:
	HANDLER_PTR_TYPE _handler;

	enum PhySocketType
	{
		ZT_PHY_SOCKET_CLOSED = 0x00, // socket is closed, will be removed on next poll()
		ZT_PHY_SOCKET_TCP_OUT_PENDING = 0x01,
		ZT_PHY_SOCKET_TCP_OUT_CONNECTED = 0x02,
		ZT_PHY_SOCKET_TCP_IN = 0x03,
		ZT_PHY_SOCKET_TCP_LISTEN = 0x04,
		ZT_PHY_SOCKET_RAW = 0x05,
		ZT_PHY_SOCKET_UDP = 0x06
	};

	struct PhySocketImpl
	{
		PhySocketType type;
		ZT_PHY_SOCKFD_TYPE sock;
		void *uptr; // user-settable pointer
		ZT_PHY_SOCKADDR_STORAGE_TYPE saddr; // remote for TCP_OUT and TCP_IN, local for TCP_LISTEN, RAW, and UDP
	};

	std::list<PhySocketImpl> _socks;
	fd_set _readfds;
	fd_set _writefds;
#if defined(_WIN32) || defined(_WIN64)
	fd_set _exceptfds;	
#endif
	long _nfds;

	ZT_PHY_SOCKFD_TYPE _whackReceiveSocket;
	ZT_PHY_SOCKFD_TYPE _whackSendSocket;

	bool _noDelay;

public:
	/**
	 * @param handler Pointer of type HANDLER_PTR_TYPE to handler
	 * @param noDelay If true, disable TCP NAGLE algorithm on TCP sockets
	 */
	Phy(HANDLER_PTR_TYPE handler,bool noDelay) :
		_handler(handler)
	{
		FD_ZERO(&_readfds);
		FD_ZERO(&_writefds);

#if defined(_WIN32) || defined(_WIN64)
		FD_ZERO(&_exceptfds);

		SOCKET pipes[2];
		{	// hack copied from StackOverflow, behaves a bit like pipe() on *nix systems
			struct sockaddr_in inaddr;
			struct sockaddr addr;
			SOCKET lst=::socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
			if (lst == INVALID_SOCKET)
				throw std::runtime_error("unable to create pipes for select() abort");
			memset(&inaddr, 0, sizeof(inaddr));
			memset(&addr, 0, sizeof(addr));
			inaddr.sin_family = AF_INET;
			inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			inaddr.sin_port = 0;
			int yes=1;
			setsockopt(lst,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(yes));
			bind(lst,(struct sockaddr *)&inaddr,sizeof(inaddr));
			listen(lst,1);
			int len=sizeof(inaddr);
			getsockname(lst, &addr,&len);
			pipes[0]=::socket(AF_INET, SOCK_STREAM,0);
			if (pipes[0] == INVALID_SOCKET)
				throw std::runtime_error("unable to create pipes for select() abort");
			connect(pipes[0],&addr,len);
			pipes[1]=accept(lst,0,0);
			closesocket(lst);
		}
#else // not Windows
		int pipes[2];
		if (::pipe(pipes))
			throw std::runtime_error("unable to create pipes for select() abort");
#endif // Windows or not

		_nfds = (pipes[0] > pipes[1]) ? (long)pipes[0] : (long)pipes[1];
		_whackReceiveSocket = pipes[0];
		_whackSendSocket = pipes[1];
		_noDelay = noDelay;
	}

	~Phy()
	{
		for(typename std::list<PhySocketImpl>::const_iterator s(_socks.begin());s!=_socks.end();++s) {
			if (s->type != ZT_PHY_SOCKET_CLOSED)
				this->close((PhySocket *)&(*s),true);
		}
		ZT_PHY_CLOSE_SOCKET(_whackReceiveSocket);
		ZT_PHY_CLOSE_SOCKET(_whackSendSocket);
	}

	/**
	 * Cause poll() to stop waiting immediately
	 */
	inline void whack()
	{
#if defined(_WIN32) || defined(_WIN64)
		::send(_whackSendSocket,(const char *)this,1,0);
#else
		::write(_whackSendSocket,(PhySocket *)this,1);
#endif
	}

	/**
	 * @return Number of open sockets
	 */
	inline unsigned long count() const throw() { return _socks.size(); }

	/**
	 * @return Maximum number of sockets allowed
	 */
	inline unsigned long maxCount() const throw() { return ZT_PHY_MAX_SOCKETS; }

	/**
	 * Bind a UDP socket
	 *
	 * @param localAddress Local endpoint address and port
	 * @param uptr Initial value of user pointer associated with this socket (default: NULL)
	 * @param bufferSize Desired socket receive/send buffer size -- will set as close to this as possible (default: 0, leave alone)
	 * @return Socket or NULL on failure to bind
	 */
	inline PhySocket *udpBind(const struct sockaddr *localAddress,void *uptr = (void *)0,int bufferSize = 0)
	{
		if (_socks.size() >= ZT_PHY_MAX_SOCKETS)
			return (PhySocket *)0;

		ZT_PHY_SOCKFD_TYPE s = ::socket(localAddress->sa_family,SOCK_DGRAM,0);
		if (!ZT_PHY_SOCKFD_VALID(s))
			return (PhySocket *)0;

		if (bufferSize > 0) {
			int bs = bufferSize;
			while (bs >= 65536) {
				int tmpbs = bs;
				if (setsockopt(s,SOL_SOCKET,SO_RCVBUF,(const char *)&tmpbs,sizeof(tmpbs)) == 0)
					break;
				bs -= 16384;
			}
			bs = bufferSize;
			while (bs >= 65536) {
				int tmpbs = bs;
				if (setsockopt(s,SOL_SOCKET,SO_SNDBUF,(const char *)&tmpbs,sizeof(tmpbs)) == 0)
					break;
				bs -= 16384;
			}
		}

#if defined(_WIN32) || defined(_WIN64)
		{
			BOOL f;
			if (localAddress->sa_family == AF_INET6) {
				f = TRUE; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f));
				f = FALSE; setsockopt(s,IPPROTO_IPV6,IPV6_DONTFRAG,(const char *)&f,sizeof(f));
			}
			f = FALSE; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
			f = TRUE; setsockopt(s,SOL_SOCKET,SO_BROADCAST,(const char *)&f,sizeof(f));
		}
#else // not Windows
		{
			int f;
			if (localAddress->sa_family == AF_INET6) {
				f = 1; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
#ifdef IPV6_MTU_DISCOVER
				f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_MTU_DISCOVER,&f,sizeof(f));
#endif
			}
			f = 0; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
			f = 1; setsockopt(s,SOL_SOCKET,SO_BROADCAST,(void *)&f,sizeof(f));
#ifdef IP_DONTFRAG
			f = 0; setsockopt(s,IPPROTO_IP,IP_DONTFRAG,&f,sizeof(f));
#endif
#ifdef IP_MTU_DISCOVER
			f = 0; setsockopt(s,IPPROTO_IP,IP_MTU_DISCOVER,&f,sizeof(f));
#endif
		}
#endif // Windows or not

		if (::bind(s,localAddress,(localAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}

#if defined(_WIN32) || defined(_WIN64)
		{ u_long iMode=1; ioctlsocket(s,FIONBIO,&iMode); }
#else
		fcntl(s,F_SETFL,O_NONBLOCK);
#endif

		try {
			_socks.push_back(PhySocketImpl());
		} catch ( ... ) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}
		PhySocketImpl &sws = _socks.back();

		if ((long)s > _nfds)
			_nfds = (long)s;
		FD_SET(s,&_readfds);
		sws.type = ZT_PHY_SOCKET_UDP;
		sws.sock = s;
		sws.uptr = uptr;
		memset(&(sws.saddr),0,sizeof(struct sockaddr_storage));
		memcpy(&(sws.saddr),localAddress,(localAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));

		return (PhySocket *)&sws;
	}

	/**
	 * Send a UDP packet
	 *
	 * @param sock UDP socket
	 * @param remoteAddress Destination address (must be correct type for socket)
	 * @param data Data to send
	 * @param len Length of packet
	 * @return True if packet appears to have been sent successfully
	 */
	inline bool udpSend(PhySocket *sock,const struct sockaddr *remoteAddress,const void *data,unsigned long len)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
#if defined(_WIN32) || defined(_WIN64)
		return ((long)::sendto(sws.sock,reinterpret_cast<const char *>(data),len,0,remoteAddress,(remoteAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)) == (long)len);
#else
		return ((long)::sendto(sws.sock,data,len,0,remoteAddress,(remoteAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)) == (long)len);
#endif
	}

	/**
	 * Bind a local listen socket to listen for new TCP connections
	 *
	 * @param localAddress Local address and port
	 * @param uptr Initial value of uptr for new socket (default: NULL)
	 * @return Socket or NULL on failure to bind
	 */
	inline PhySocket *tcpListen(const struct sockaddr *localAddress,void *uptr = (void *)0)
	{
		if (_socks.size() >= ZT_PHY_MAX_SOCKETS)
			return (PhySocket *)0;

		ZT_PHY_SOCKFD_TYPE s = ::socket(localAddress->sa_family,SOCK_STREAM,0);
		if (!ZT_PHY_SOCKFD_VALID(s))
			return (PhySocket *)0;

#if defined(_WIN32) || defined(_WIN64)
		{
			BOOL f;
			f = TRUE; ::setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f));
			f = TRUE; ::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
			f = (_noDelay ? TRUE : FALSE); setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f));
			u_long iMode=1;
			ioctlsocket(s,FIONBIO,&iMode);
		}
#else
		{
			int f;
			f = 1; ::setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
			f = 1; ::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
			f = (_noDelay ? 1 : 0); setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f));
			fcntl(s,F_SETFL,O_NONBLOCK);
		}
#endif

		if (::bind(s,localAddress,(localAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}

		if (::listen(s,1024)) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}

		try {
			_socks.push_back(PhySocketImpl());
		} catch ( ... ) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}
		PhySocketImpl &sws = _socks.back();

		if ((long)s > _nfds)
			_nfds = (long)s;
		FD_SET(s,&_readfds);
		sws.type = ZT_PHY_SOCKET_TCP_LISTEN;
		sws.sock = s;
		sws.uptr = uptr;
		memset(&(sws.saddr),0,sizeof(struct sockaddr_storage));
		memcpy(&(sws.saddr),localAddress,(localAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));

		return (PhySocket *)&sws;
	}

	/**
	 * Start a non-blocking connect; CONNECT handler is called on success or failure
	 *
	 * A return value of NULL indicates a synchronous failure such as a
	 * failure to open a socket. The TCP connection handler is not called
	 * in this case.
	 *
	 * It is possible on some platforms for an "instant connect" to occur,
	 * such as when connecting to a loopback address. In this case, the
	 * 'connected' result parameter will be set to 'true' and if the
	 * 'callConnectHandler' flag is true (the default) the TCP connect
	 * handler will be called before the function returns.
	 *
	 * These semantics can be a bit confusing, but they're less so than
	 * the underlying semantics of asynchronous TCP connect.
	 *
	 * @param remoteAddress Remote address
	 * @param connected Result parameter: set to whether an "instant connect" has occurred (true if yes)
	 * @param uptr Initial value of uptr for new socket (default: NULL)
	 * @param callConnectHandler If true, call TCP connect handler even if result is known before function exit (default: true)
	 * @return New socket or NULL on failure
	 */
	inline PhySocket *tcpConnect(const struct sockaddr *remoteAddress,bool &connected,void *uptr = (void *)0,bool callConnectHandler = true)
	{
		if (_socks.size() >= ZT_PHY_MAX_SOCKETS)
			return (PhySocket *)0;

		ZT_PHY_SOCKFD_TYPE s = ::socket(remoteAddress->sa_family,SOCK_STREAM,0);
		if (!ZT_PHY_SOCKFD_VALID(s)) {
			connected = false;
			return (PhySocket *)0;
		}

#if defined(_WIN32) || defined(_WIN64)
		{
			BOOL f;
			if (remoteAddress->sa_family == AF_INET6) { f = TRUE; ::setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f)); }
			f = TRUE; ::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
			f = (_noDelay ? TRUE : FALSE); setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f));
			u_long iMode=1;
			ioctlsocket(s,FIONBIO,&iMode);
		}
#else
		{
			int f;
			if (remoteAddress->sa_family == AF_INET6) { f = 1; ::setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f)); }
			f = 1; ::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
			f = (_noDelay ? 1 : 0); setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f));
			fcntl(s,F_SETFL,O_NONBLOCK);
		}
#endif

		connected = true;
		if (::connect(s,remoteAddress,(remoteAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))) {
			connected = false;
#if defined(_WIN32) || defined(_WIN64)
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
			if (errno != EINPROGRESS) {
#endif
				ZT_PHY_CLOSE_SOCKET(s);
				return (PhySocket *)0;
			} // else connection is proceeding asynchronously...
		}

		try {
			_socks.push_back(PhySocketImpl());
		} catch ( ... ) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}
		PhySocketImpl &sws = _socks.back();

		if ((long)s > _nfds)
			_nfds = (long)s;
		if (connected) {
			FD_SET(s,&_readfds);
			sws.type = ZT_PHY_SOCKET_TCP_OUT_CONNECTED;
		} else {
			FD_SET(s,&_writefds);
#if defined(_WIN32) || defined(_WIN64)
			FD_SET(s,&_exceptfds);
#endif
			sws.type = ZT_PHY_SOCKET_TCP_OUT_PENDING;
		}
		sws.sock = s;
		sws.uptr = uptr;
		memset(&(sws.saddr),0,sizeof(struct sockaddr_storage));
		memcpy(&(sws.saddr),remoteAddress,(remoteAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));

		if ((callConnectHandler)&&(connected)) {
			try {
				_handler->phyOnTcpConnect((PhySocket *)&sws,&(sws.uptr),true);
			} catch ( ... ) {}
		}

		return (PhySocket *)&sws;
	}

	/**
	 * Attempt to send data to a TCP connection (non-blocking)
	 *
	 * If -1 is returned, the socket should no longer be used as it is now
	 * destroyed. If callCloseHandler is true, the close handler will be
	 * called before the function returns.
	 *
	 * @param sock An open TCP socket (other socket types will fail)
	 * @param data Data to send
	 * @param len Length of data
	 * @param callCloseHandler If true, call close handler on socket closing failure condition (default: true)
	 * @return Number of bytes actually sent or -1 on fatal error (socket closure)
	 */
	inline long tcpSend(PhySocket *sock,const void *data,unsigned long len,bool callCloseHandler = true)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
#if defined(_WIN32) || defined(_WIN64)
		long n = (long)::send(sws.sock,reinterpret_cast<const char *>(data),len,0);
		if (n == SOCKET_ERROR) {
				switch(WSAGetLastError()) {
					case WSAEINTR:
					case WSAEWOULDBLOCK:
						return 0;
					default:
						this->close(sock,callCloseHandler);
						return -1;
				}
		}
#else // not Windows
		long n = (long)::send(sws.sock,data,len,0);
		if (n < 0) {
			switch(errno) {
#ifdef EAGAIN
				case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && ( !defined(EAGAIN) || (EWOULDBLOCK != EAGAIN) )
				case EWOULDBLOCK:
#endif
#ifdef EINTR
				case EINTR:
#endif
					return 0;
				default:
					this->close(sock,callCloseHandler);
					return -1;
			}
		}
#endif // Windows or not
		return n;
	}

	/**
	 * Set whether we want to be notified via the TCP writability handler when a socket is writable
	 *
	 * Call whack() if this is being done from another thread and you want
	 * it to take effect immediately. Otherwise it is only guaranteed to
	 * take effect on the next poll().
	 *
	 * @param sock TCP connection socket (other types are not valid)
	 * @param notifyWritable Want writable notifications?
	 */
	inline const void tcpSetNotifyWritable(PhySocket *sock,bool notifyWritable)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
		if (notifyWritable) {
			FD_SET(sws.sock,&_writefds);
		} else {
			FD_CLR(sws.sock,&_writefds);
		}
	}

	/**
	 * Wait for activity and handle one or more events
	 *
	 * Note that this is not guaranteed to wait up to 'timeout' even
	 * if nothing happens, as whack() or other events such as signals
	 * may cause premature termination.
	 *
	 * @param timeout Timeout in milliseconds or 0 for none (forever)
	 */
	inline void poll(unsigned long timeout)
	{
		char buf[131072];
		struct sockaddr_storage ss;
		struct timeval tv;
		fd_set rfds,wfds,efds;

		memcpy(&rfds,&_readfds,sizeof(rfds));
		memcpy(&wfds,&_writefds,sizeof(wfds));
#if defined(_WIN32) || defined(_WIN64)
		memcpy(&efds,&_exceptfds,sizeof(efds));
#else
		FD_ZERO(&efds);
#endif

		tv.tv_sec = (long)(timeout / 1000);
		tv.tv_usec = (long)((timeout % 1000) * 1000);
		if (::select((int)_nfds + 1,&rfds,&wfds,&efds,(timeout > 0) ? &tv : (struct timeval *)0) <= 0)
			return;

		if (FD_ISSET(_whackReceiveSocket,&rfds)) {
			char tmp[16];
#if defined(_WIN32) || defined(_WIN64)
			::recv(_whackReceiveSocket,tmp,16,0);
#else
			::read(_whackReceiveSocket,tmp,16);
#endif
		}

		for(typename std::list<PhySocketImpl>::iterator s(_socks.begin());s!=_socks.end();) {
			switch (s->type) {

				case ZT_PHY_SOCKET_TCP_OUT_PENDING:
#if defined(_WIN32) || defined(_WIN64)
					if (FD_ISSET(s->sock,&efds)) {
						this->close((PhySocket *)&(*s),true);
					} else // ... if
#endif
					if (FD_ISSET(s->sock,&wfds)) {
						socklen_t slen = sizeof(ss);
						if (::getpeername(s->sock,(struct sockaddr *)&ss,&slen) != 0) {
							this->close((PhySocket *)&(*s),true);
						} else {
							s->type = ZT_PHY_SOCKET_TCP_OUT_CONNECTED;
							FD_SET(s->sock,&_readfds);
							FD_CLR(s->sock,&_writefds);
#if defined(_WIN32) || defined(_WIN64)
							FD_CLR(s->sock,&_exceptfds);
#endif
							try {
								_handler->phyOnTcpConnect((PhySocket *)&(*s),&(s->uptr),true);
							} catch ( ... ) {}
						}
					}
					break;

				case ZT_PHY_SOCKET_TCP_OUT_CONNECTED:
				case ZT_PHY_SOCKET_TCP_IN: {
					ZT_PHY_SOCKFD_TYPE sock = s->sock; // if closed, s->sock becomes invalid as s is no longer dereferencable
					if (FD_ISSET(sock,&rfds)) {
						long n = (long)::recv(sock,buf,sizeof(buf),0);
						if (n <= 0) {
							this->close((PhySocket *)&(*s),true);
						} else {
							try {
								_handler->phyOnTcpData((PhySocket *)&(*s),&(s->uptr),(void *)buf,(unsigned long)n);
							} catch ( ... ) {}
						}
					}
					if ((FD_ISSET(sock,&wfds))&&(FD_ISSET(sock,&_writefds))) {
						try {
							_handler->phyOnTcpWritable((PhySocket *)&(*s),&(s->uptr));
						} catch ( ... ) {}
					}
				}	break;

				case ZT_PHY_SOCKET_TCP_LISTEN:
					if (FD_ISSET(s->sock,&rfds)) {
						memset(&ss,0,sizeof(ss));
						socklen_t slen = sizeof(ss);
						ZT_PHY_SOCKFD_TYPE newSock = ::accept(s->sock,(struct sockaddr *)&ss,&slen);
						if (ZT_PHY_SOCKFD_VALID(newSock)) {
							if (_socks.size() >= ZT_PHY_MAX_SOCKETS) {
								ZT_PHY_CLOSE_SOCKET(newSock);
							} else {
#if defined(_WIN32) || defined(_WIN64)
								{ BOOL f = (_noDelay ? TRUE : FALSE); setsockopt(newSock,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f)); }
								{ u_long iMode=1; ioctlsocket(newSock,FIONBIO,&iMode); }
#else
								{ int f = (_noDelay ? 1 : 0); setsockopt(newSock,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f)); }
								fcntl(newSock,F_SETFL,O_NONBLOCK);
#endif
								_socks.push_back(PhySocketImpl());
								PhySocketImpl &sws = _socks.back();
								FD_SET(newSock,&_readfds);
								if ((long)newSock > _nfds)
									_nfds = (long)newSock;
								sws.type = ZT_PHY_SOCKET_TCP_IN;
								sws.sock = newSock;
								sws.uptr = (void *)0;
								memcpy(&(sws.saddr),&ss,sizeof(struct sockaddr_storage));
								try {
									_handler->phyOnTcpAccept((PhySocket *)&(*s),(PhySocket *)&(_socks.back()),&(s->uptr),&(sws.uptr),(const struct sockaddr *)&(sws.saddr));
								} catch ( ... ) {}
							}
						}
					}
					break;

				case ZT_PHY_SOCKET_UDP:
					if (FD_ISSET(s->sock,&rfds)) {
						for(;;) {
							memset(&ss,0,sizeof(ss));
							socklen_t slen = sizeof(ss);
							long n = (long)::recvfrom(s->sock,buf,sizeof(buf),0,(struct sockaddr *)&ss,&slen);
							if (n > 0) {
								try {
									_handler->phyOnDatagram((PhySocket *)&(*s),&(s->uptr),(const struct sockaddr *)&ss,(void *)buf,(unsigned long)n);
								} catch ( ... ) {}
							} else if (n < 0)
								break;
						}
					}
					break;

				default:
					break;

			}

			if (s->type == ZT_PHY_SOCKET_CLOSED)
				_socks.erase(s++);
			else ++s;
		}
	}

	/**
	 * @param sock Socket to close
	 * @param callHandlers If true, call handlers for TCP connect (success: false) or close (default: true)
	 */
	inline void close(PhySocket *sock,bool callHandlers = true)
	{
		if (!sock)
			return;
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
		if (sws.type == ZT_PHY_SOCKET_CLOSED)
			return;

		FD_CLR(sws.sock,&_readfds);
		FD_CLR(sws.sock,&_writefds);
#if defined(_WIN32) || defined(_WIN64)
		FD_CLR(sws.sock,&_exceptfds);
#endif

		ZT_PHY_CLOSE_SOCKET(sws.sock);

		switch(sws.type) {
			case ZT_PHY_SOCKET_TCP_OUT_PENDING:
				if (callHandlers) {
					try {
						_handler->phyOnTcpConnect(sock,&(sws.uptr),false);
					} catch ( ... ) {}
				}
				break;
			case ZT_PHY_SOCKET_TCP_OUT_CONNECTED:
			case ZT_PHY_SOCKET_TCP_IN:
				if (callHandlers) {
					try {
						_handler->phyOnTcpClose(sock,&(sws.uptr));
					} catch ( ... ) {}
				}
				break;
			default:
				break;
		}

		// Causes entry to be deleted from list in poll(), ignored elsewhere
		sws.type = ZT_PHY_SOCKET_CLOSED;

		if (sws.sock >= _nfds) {
			long nfds = (long)_whackSendSocket;
			if ((long)_whackReceiveSocket > nfds)
				nfds = (long)_whackReceiveSocket;
			for(typename std::list<PhySocketImpl>::iterator s(_socks.begin());s!=_socks.end();++s) {
				if ((s->type != ZT_PHY_SOCKET_CLOSED)&&((long)s->sock > nfds))
					nfds = (long)s->sock;
			}
			_nfds = nfds;
		}
	}
};

} // namespace ZeroTier

#endif
