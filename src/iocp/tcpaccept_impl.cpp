﻿/*
 * zsummerX License
 * -----------
 * 
 * zsummerX is licensed under the terms of the MIT license reproduced below.
 * This means that zsummerX is free software and can be used for both academic
 * and commercial purposes at absolutely no cost.
 * 
 * 
 * ===============================================================================
 * 
 * Copyright (C) 2013-2014 YaweiZhang <yawei_zhang@foxmail.com>.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * ===============================================================================
 * 
 * (end of COPYRIGHT)
 */


#include <zsummerX/iocp/iocp_impl.h>
#include <zsummerX/iocp/tcpaccept_impl.h>


using namespace zsummer::network;
CTcpAccept::CTcpAccept()
{
	//listen
	memset(&m_handle._overlapped, 0, sizeof(m_handle._overlapped));
	m_server = INVALID_SOCKET;
	memset(&m_addr, 0, sizeof(m_addr));
	m_handle._type = tagReqHandle::HANDLE_ACCEPT;

	//client
	m_socket = INVALID_SOCKET;
	memset(m_recvBuf, 0, sizeof(m_recvBuf));
	m_recvLen = 0;
	//status
	m_nLinkStatus = LS_UNINITIALIZE;
}
CTcpAccept::~CTcpAccept()
{
	if (m_server != INVALID_SOCKET)
	{
		closesocket(m_server);
		m_server = INVALID_SOCKET;
	}
	if (m_socket != INVALID_SOCKET)
	{
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
}

bool CTcpAccept::Initialize(ZSummerPtr summer)
{
	m_summer = summer;
	return true;
}

bool CTcpAccept::OpenAccept(const char * ip, unsigned short port)
{
	if (!m_summer)
	{
		LCF("CTcpAccept m_summer is nullptr!  ip=" << ip << ", port=" << port);
		assert(0);
		return false;
	}

	if (m_server != INVALID_SOCKET)
	{
		LCF("CTcpAccept socket is arealy used!  ip=" << ip << ", port=" << port);
		assert(0);
		return false;
	}

	m_server = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_server == INVALID_SOCKET)
	{
		LCF("CTcpAccept server can't create socket!   ip=" << ip << ", port=" << port);
		assert(0);
		return false;
	}

	BOOL bReUseAddr = TRUE;
	if (setsockopt(m_server, SOL_SOCKET,SO_REUSEADDR, (char*)&bReUseAddr, sizeof(BOOL)) != 0)
	{
		LCW("setsockopt  SO_REUSEADDR ERROR! ERRCODE=" << WSAGetLastError() << "    ip=" << ip << ", port=" << port);
	}

	m_addr.sin_family = AF_INET;
	m_addr.sin_addr.s_addr = inet_addr(ip);
	m_addr.sin_port = htons(port);
	if (bind(m_server, (sockaddr *) &m_addr, sizeof(m_addr)) != 0)
	{
		LCF("server bind err, ERRCODE=" << WSAGetLastError() << "   ip=" << ip << ", port=" << port);
		closesocket(m_server);
		m_server = INVALID_SOCKET;
		return false;
	}

	if (listen(m_server, SOMAXCONN) != 0)
	{
		LCF("server listen err, ERRCODE=" << WSAGetLastError() << "   ip=" << ip << ", port=" << port);
		closesocket(m_server);
		m_server = INVALID_SOCKET;
		return false;
	}

	if (CreateIoCompletionPort((HANDLE)m_server, m_summer->m_io, (ULONG_PTR)this, 1) == NULL)
	{
		LCF("server bind iocp err, ERRCODE=" << WSAGetLastError() << "   ip=" << ip << ", port=" << port);
		closesocket(m_server);
		m_server = INVALID_SOCKET;
		return false;
	}
	m_nLinkStatus = LS_ESTABLISHED;
	return true;
}

bool CTcpAccept::DoAccept(CTcpSocketPtr & s, const _OnAcceptHandler& handler)
{
	if (m_onAcceptHandler)
	{
		LCF("DoAccept err, aready DoAccept  ip=" << m_ip << ", port=" << m_port);
		return false;
	}
	if (m_nLinkStatus != LS_ESTABLISHED)
	{
		LCF("DoAccept err, link is unestablished.  ip=" << m_ip << ", port=" << m_port);
		return false;
	}
	
	m_client = s;
	m_socket = INVALID_SOCKET;
	memset(m_recvBuf, 0, sizeof(m_recvBuf));
	m_recvLen = 0;
	m_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,  NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_socket == INVALID_SOCKET)
	{
		LCF("create client socket err! ERRCODE=" << WSAGetLastError() << " ip=" << m_ip << ", port=" << m_port);
		return false;
	}

	if (!AcceptEx(m_server, m_socket, m_recvBuf, 0, sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &m_recvLen, &m_handle._overlapped))
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			LCE("do AcceptEx err, ERRCODE=" << WSAGetLastError() << " ip=" << m_ip << ", port=" << m_port);
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
			return false;
		}
	}
	m_onAcceptHandler = handler;
	m_handle._tcpAccept = shared_from_this();
	return true;
}
bool CTcpAccept::OnIOCPMessage(BOOL bSuccess)
{
	std::shared_ptr<CTcpAccept> guad(m_handle._tcpAccept);
	m_handle._tcpAccept.reset();

	_OnAcceptHandler onAccept;
	onAccept.swap(m_onAcceptHandler);
	if (bSuccess)
	{
		{
			if (setsockopt(m_socket,SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&m_server, sizeof(m_server)) != 0)
			{
				LCW("SO_UPDATE_ACCEPT_CONTEXT fail!  last err=" << WSAGetLastError()  << " ip=" << m_ip << ", port=" << m_port);
			}
			BOOL bTrue = TRUE;
			if (setsockopt(m_socket,IPPROTO_TCP, TCP_NODELAY, (char*)&bTrue, sizeof(bTrue)) != 0)
			{
				LCW("setsockopt TCP_NODELAY fail!  last err=" << WSAGetLastError()  << " ip=" << m_ip << ", port=" << m_port);
			}
		}

		sockaddr * paddr1 = NULL;
		sockaddr * paddr2 = NULL;
		int tmp1 = 0;
		int tmp2 = 0;
		GetAcceptExSockaddrs(m_recvBuf, m_recvLen, sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &paddr1, &tmp1, &paddr2, &tmp2);
		m_client->AttachSocket(m_socket, inet_ntoa(((sockaddr_in*)paddr2)->sin_addr), ntohs(((sockaddr_in*)paddr2)->sin_port));
		onAccept(EC_SUCCESS, m_client);
	}
	else
	{
		LCW("Accept Fail,  retry doAccept ... ip=" << m_ip << ", port=" << m_port << ", lastError=" << GetLastError());
		onAccept(EC_ERROR, m_client);
	}
	return true;
}
