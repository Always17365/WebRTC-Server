/*
 * Socket.h
 *
 *  Created on: 2016年9月23日
 *      Author: max
 */

#ifndef SOCKET_H_
#define SOCKET_H_

#include <errno.h>

#include <sys/socket.h>

#include <string>
using namespace std;

#include <common/LogManager.h>

typedef enum SocketStatus {
	SocketStatusSuccess,
	SocketStatusFail,
	SocketStatusTimeout
}SocketStatus;

typedef struct ev_io;

#define INVALID_SOCKET -1

class Socket {
public:
	static Socket* Create() {
		Socket* socket = new Socket();
		return socket;
	}

	static void Destroy(Socket* socket) {
		if( socket ) {
			delete socket;
		}
	}

public:
	Socket() {
		fd = INVALID_SOCKET;
		ip = "";
		port = 0;
		data = NULL;
		w = NULL;
	};

	~Socket() {

	}

	void SetAddress(int fd, const string ip, unsigned int port) {
		this->fd = fd;
		this->ip = ip;
		this->port = port;
	}

	SocketStatus Read(const char *data, int &len) {
		SocketStatus status = SocketStatusFail;
		int ret = recv(fd, (void *)data, len, 0);
		if( ret > 0 ) {
			status = SocketStatusSuccess;

		} else if( ret == 0 ) {
//			LogAync(
//					LOG_STAT,
//					"Socket::Read( "
//					"[Normal Closed], "
//					"fd : '%d' "
//					")",
//					fd
//					);
			status = SocketStatusFail;

		} else {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
//				LogAync(
//						LOG_STAT,
//						"Socket::Read( "
//						"[errno == EAGAIN || errno == EWOULDBLOCK continue], "
//						"fd : '%d' "
//						")",
//						fd
//						);
				status = SocketStatusTimeout;

			} else {
//				LogAync(
//						LOG_STAT,
//						"Socket::Read( "
//						"[Error Closed], "
//						"fd : '%d' "
//						")",
//						fd
//						);
				status = SocketStatusFail;
			}
		}

		len = ret;
		return status;
	}

	bool Send(const char *data, int &len) {
		bool bFlag = false;
		int index = 0;

		if( len <= 0 ) {
			return false;
		}

		do {
			int ret = send(fd, data + index, len - index, 0);
			if( ret > 0 ) {
				index += ret;
				if( index == len ) {
//					LogAync(
//							LOG_STAT,
//							"Socket::Send( "
//							"[Send Finish], "
//							"fd : '%d' "
//							")",
//							fd
//							);
					bFlag = true;
				}
			} else {
				if(errno == EAGAIN || errno == EWOULDBLOCK) {
//					LogAync(
//							LOG_STAT,
//							"Socket::Send( "
//							"[errno == EAGAIN || errno == EWOULDBLOCK continue], "
//							"fd : % '%d' "
//							")",
//							fd
//							);
					continue;
				} else {
//					LogAync(
//							LOG_STAT,
//							"Socket::Send( "
//							"[Error Closed], "
//							"fd : '%d' "
//							")",
//							fd
//							);
					break;
				}
			}

		} while(true);

		len = index;

		return bFlag;
	}

	void Disconnect() {
//		LogAync(
//				LOG_STAT,
//				"Socket::Disconnect( "
//				"fd : '%d' "
//				")",
//				fd
//				);
		shutdown(fd, SHUT_RD);
	}

	void Close() {
//		LogAync(
//				LOG_STAT,
//				"Socket::Close( "
//				"fd : '%d' "
//				")",
//				fd
//				);
		close(fd);
	}

	int fd;
	string ip;
	unsigned int port;

	/**
	 * 自定义数据
	 */
	void* data;

	ev_io *w;
};

#endif /* SOCKET_H_ */
