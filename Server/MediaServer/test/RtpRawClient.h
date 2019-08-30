/*
 * RtpRawClient.h
 * 利用UDP作为传输的RTP客户端
 *  Created on: 2019/07/03
 *      Author: max
 *		Email: Kingsleyyau@gmail.com
 */

#ifndef RTP_RTPRAWCLIENT_H_
#define RTP_RTPRAWCLIENT_H_

#include <string.h>

#include <socket/UdpSender.h>
#include <socket/UdpReceiver.h>

#include "RtpSession.h"

using namespace std;

namespace mediaserver {

class RtpRawClient: public RtpSession {
public:
	RtpRawClient();
	virtual ~RtpRawClient();

public:
	bool Init(const string sendIp, int rtpSendPort, int rtpRecvPort);
	bool Start(char *localKey = NULL, int localSize = 0, char *remoteKey = NULL, int remoteSize = 0);
	void Stop();
	void Shutdown();

	/**
	 * 接收原始RTP包(网络字节序)
	 */
	bool RecvRtpPacket(void *pkt, unsigned int& pktSize);

private:
	UdpSender mRtpSender;
	UdpSender mRtcpSender;
	UdpReceiver mRtpReceiver;

	string mSendIp;
	int mRtpSendPort;
	int mRtpRecvPort;

};

} /* namespace mediaserver */

#endif /* RTP_RTPRAWCLIENT_H_ */
