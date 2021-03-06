/*
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
#ifndef _ZSUMMERX_TCPSOCKET_IMPL_H_
#define _ZSUMMERX_TCPSOCKET_IMPL_H_

#include "common_impl.h"
#include "iocp_impl.h"
namespace zsummer
{
	namespace network
	{
		class CTcpSocket : public std::enable_shared_from_this<CTcpSocket>
		{
		public:

			CTcpSocket();
			~CTcpSocket();
			//! Initialize an attach socket to zsummer pump.
			//if the socket is used to connect,  It's need initialize before call DoConnect 
			// if the socket is used to accept new socket, It's need initialize after OnAccept. 
			bool Initialize(ZSummerPtr summer);
			inline bool GetPeerInfo(std::string& remoteIP, unsigned short &remotePort)
			{
				remoteIP = m_remoteIP;
				remotePort = m_remotePort;
				return true;
			}
			//! handle: void(zsummer::network::ErrorCode);
			//! handle: ErrorCode: 0 success. other code is failed  and can see error code in enum ErrorCode 
			bool DoConnect(std::string remoteIP, unsigned short remotePort, const _OnConnectHandler & handler);
			//!handle:  void(ErrorCode, int)
			//!handle:  ErrorCode: 0 success. other code is failed  and can see error code in enum ErrorCode 
			//!handle:  int: is transfer length. if not all data already transfer that you need call DoSend transfer the remnant data.
			//! warning: when  handler is not callback ,  the function can not call repeat. if you have some question maybe you need read the test or implementation .
			//!          so,  when you want  repeat send data without care the callback , you need encapsulate the send operate via a send queue like the StressTest/FrameTest source code
			bool DoSend(char * buf, unsigned int len, const _OnSendHandler &handler);
			//!handle:  void(ErrorCode, int)
			//!handle:  ErrorCode: 0 success. other code is failed  and can see error code in enum ErrorCode 
			//!handle:  int: is received data  length. it maybe short than you want received data (len).
			//! buf: you recv buffer memory address . you would block the buffer still the handler callback .
			//! len: you want recv data for max bytes .
			//! warning: when  handler is not callback ,  the function can not call repeat. if you have some question maybe you need read the test or implementation .
			bool DoRecv(char * buf, unsigned int len, const _OnRecvHandler & handler);
			//close this socket.
			//warning : at a safe close method , if you have DoConnect/DoRecv/DoSend request and not all callback. you need wait callback .  the callback will return with a error code.
			//         if you have not the operate and when you DoClose the socket and immediate destroy this class object . in next do zsummerx's RunOnce(), callback may be return and call operate in the bad memory . 
			bool DoClose();
		public:
			bool AttachSocket(SOCKET s, std::string remoteIP, unsigned short remotePort);
			void OnIOCPMessage(BOOL bSuccess, DWORD dwTranceCount, unsigned char cType);
			std::string GetTcpSocketImplStatus();
		public:
			//private
			ZSummerPtr  m_summer;
			SOCKET		m_socket = INVALID_SOCKET;
			std::string m_remoteIP;
			unsigned short m_remotePort = 0;

			//recv
			tagReqHandle m_recvHandle;
			WSABUF		 m_recvWSABuf;
			_OnRecvHandler m_onRecvHandler;


			//send
			tagReqHandle m_sendHandle;
			WSABUF		 m_sendWsaBuf;
			_OnSendHandler m_onSendHandler;


			//connect
			tagReqHandle m_connectHandle;
			_OnConnectHandler m_onConnectHandler;
			//status
			int m_nLinkStatus = LS_UNINITIALIZE;
		};
		typedef std::shared_ptr<CTcpSocket> CTcpSocketPtr;

	}
}
























#endif











