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
 * Copyright (C) 2013 YaweiZhang <yawei_zhang@foxmail.com>.
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


#include <zsummerX/select/tcpsocket_impl.h>
#include <zsummerX/select/tcpaccept_impl.h>


using namespace zsummer::network;




CTcpAccept::CTcpAccept()
{
	m_register._type = tagRegister::REG_TCP_ACCEPT;
}

CTcpAccept::~CTcpAccept()
{
	if (m_register._fd != InvalideFD)
	{
		closesocket(m_register._fd);
		m_register._fd = InvalideFD;
	}
}
std::string CTcpAccept::AcceptSection()
{
	std::stringstream os;
	os << " CTcpAccept:Status m_summer=" << m_summer.use_count() << ", m_listenIP=" << m_listenIP
		<< ", m_listenPort=" << m_listenPort << ", m_onAcceptHandler=" << (bool)m_onAcceptHandler
		<< ", m_client=" << m_client.use_count() << "m_register=" << m_register;
	return os.str();
}
bool CTcpAccept::Initialize(ZSummerPtr summer)
{
	m_summer = summer;
	m_register._linkstat = LS_WAITLINK;
	return true;
}


bool CTcpAccept::OpenAccept(std::string listenIP, unsigned short listenPort)
{
	if (!m_summer)
	{
		LCE("CTcpAccept::OpenAccept[this0x" << this << "] m_summer not bind!" << AcceptSection());
		return false;
	}

	if (m_register._fd != InvalideFD)
	{
		LCF("CTcpAccept::OpenAccept[this0x" << this << "] accept fd is aready used!" << AcceptSection());
		return false;
	}

	m_register._fd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_register._fd == InvalideFD)
	{
		LCF("CTcpAccept::OpenAccept[this0x" << this << "] listen socket create err " << OSTREAM_GET_LASTERROR << ", " << AcceptSection());
		return false;
	}

	SetNonBlock(m_register._fd);



	int bReuse = 1;
	if (setsockopt(m_register._fd, SOL_SOCKET, SO_REUSEADDR,  (char *)&bReuse, sizeof(bReuse)) != 0)
	{
		LCW("CTcpAccept::OpenAccept[this0x" << this << "] listen socket set reuse fail! " << OSTREAM_GET_LASTERROR << ", " << AcceptSection());
	}


	m_addr.sin_family = AF_INET;
	m_addr.sin_addr.s_addr = inet_addr(listenIP.c_str());
	m_addr.sin_port = htons(listenPort);
	if (bind(m_register._fd, (sockaddr *) &m_addr, sizeof(m_addr)) != 0)
	{
		LCF("CTcpAccept::OpenAccept[this0x" << this << "] listen socket bind err, " << OSTREAM_GET_LASTERROR << ", " << AcceptSection());
		closesocket(m_register._fd);
		m_register._fd = InvalideFD;
		return false;
	}

	if (listen(m_register._fd, 200) != 0)
	{
		LCF("CTcpAccept::OpenAccept[this0x" << this << "] listen socket listen err, " << OSTREAM_GET_LASTERROR << ", " << AcceptSection());
		closesocket(m_register._fd);
		m_register._fd = InvalideFD;
		return false;
	}

	if (!m_summer->RegisterEvent(0, m_register))
	{
		LCF("CTcpAccept::OpenAccept[this0x" << this << "] listen socket register error." << AcceptSection());
		return false;
	}
	m_register._linkstat = LS_ESTABLISHED;
	return true;
}

bool CTcpAccept::DoAccept(CTcpSocketPtr &s, const _OnAcceptHandler &handle)
{
	if (m_onAcceptHandler)
	{
		LCF("CTcpAccept::DoAccept[this0x" << this << "] err, dumplicate DoAccept" << AcceptSection());
		return false;
	}
	if (m_register._linkstat != LS_ESTABLISHED)
	{
		LCF("CTcpAccept::DoAccept[this0x" << this << "] err, _linkstat not work state" << AcceptSection());
		return false;
	}
	
	m_register._rd = true;
	m_register._tcpacceptPtr = shared_from_this();
	if (!m_summer->RegisterEvent(1, m_register))
	{
		LCF("CTcpAccept::DoAccept[this0x" << this << "] err, _linkstat not work state" << AcceptSection());
		m_register._tcpacceptPtr.reset();
		return false;
	}

	m_onAcceptHandler = handle;
	m_client = s;
	
	return true;
}
void CTcpAccept::OnSelectMessage()
{
	std::shared_ptr<CTcpAccept> guard(m_register._tcpacceptPtr);
	m_register._tcpacceptPtr.reset();

	if (!m_onAcceptHandler)
	{
		LCF("CTcpAccept::OnSelectMessage[this0x" << this << "] err, dumplicate DoAccept" << AcceptSection());
		return ;
	}
	if (m_register._linkstat != LS_ESTABLISHED)
	{
		LCF("CTcpAccept::OnSelectMessage[this0x" << this << "] err, _linkstat not work state" << AcceptSection());
		return ;
	}

	_OnAcceptHandler onAccept;
	onAccept.swap(m_onAcceptHandler);
	CTcpSocketPtr ps(m_client);
	m_client.reset();
	m_register._rd = false;
	m_summer->RegisterEvent(1,m_register);

	sockaddr_in cltaddr;
	memset(&cltaddr, 0, sizeof(cltaddr));
	socklen_t len = sizeof(cltaddr);
	SOCKET s = ::accept(m_register._fd, (sockaddr *)&cltaddr, &len);
	if (s == -1)
	{
		if (!IS_WOULDBLOCK)
		{
			LCE("CTcpAccept::OnSelectMessage[this0x" << this << "] ERR: accept return -1, " OSTREAM_GET_LASTERROR << AcceptSection());
		}
		onAccept(EC_ERROR, ps);
		return ;
	}

	SetNonBlock(s);
	SetNoDelay(s);

	ps->AttachSocket(s,inet_ntoa(cltaddr.sin_addr), ntohs(cltaddr.sin_port));
	onAccept(EC_SUCCESS, ps);
	
	return ;
}

bool CTcpAccept::Close()
{
	m_onAcceptHandler = nullptr;
	m_summer->RegisterEvent(2, m_register);
	shutdown(m_register._fd, SHUT_RDWR);
	closesocket(m_register._fd);
	m_register._fd = InvalideFD;
	return true;
}

