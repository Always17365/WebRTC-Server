/*
 * MediaServer.cpp
 *
 *  Created on: 2019-6-13
 *      Author: Max.Chiu
 *      Email: Kingsleyyau@gmail.com
 */

#include "MediaServer.h"
// System
#include <sys/syscall.h>
#include <algorithm>
// Include
#include <include/CommonHeader.h>
// Common
#include <common/CommonFunc.h>
#include <httpclient/HttpClient.h>
#include <crypto/Crypto.h>
#include <simulatorchecker/SimulatorProtocolTool.h>
// Request
// Respond
#include <respond/BaseRespond.h>
#include <respond/BaseRawRespond.h>
#include <respond/BaseResultRespond.h>

/***************************** 状态监视处理 **************************************/
class StateRunnable : public KRunnable {
public:
	StateRunnable(MediaServer *container) {
		mContainer = container;
	}
	virtual ~StateRunnable() {
		mContainer = NULL;
	}
protected:
	void onRun() {
		mContainer->StateHandle();
	}
private:
	MediaServer *mContainer;
};

/***************************** 状态监视处理 **************************************/

/***************************** 超时处理 **************************************/
class TimeoutCheckRunnable : public KRunnable {
public:
	TimeoutCheckRunnable(MediaServer *container) {
		mContainer = container;
	}
	virtual ~TimeoutCheckRunnable() {
		mContainer = NULL;
	}
protected:
	void onRun() {
		mContainer->TimeoutCheckHandle();
	}
private:
	MediaServer *mContainer;
};

/***************************** 超时处理 **************************************/

/***************************** 外部登录处理 **************************************/
class ExtRequestRunnable : public KRunnable {
public:
	ExtRequestRunnable(MediaServer *container) {
		mContainer = container;
	}
	virtual ~ExtRequestRunnable() {
		mContainer = NULL;
	}
protected:
	void onRun() {
		mContainer->ExtRequestHandle();
	}
private:
	MediaServer *mContainer;
};

/***************************** 外部登录处理 **************************************/


MediaServer::MediaServer()
:mServerMutex(KMutex::MutexType_Recursive) {
	// TODO Auto-generated constructor stub
	// Http服务
	mAsyncIOServer.SetAsyncIOServerCallback(this);
	// Websocket服务
	mWSServer.SetCallback(this);
	// 状态监视线程
	mpStateRunnable = new StateRunnable(this);
	// 超时处理线程
	mpTimeoutCheckRunnable = new TimeoutCheckRunnable(this);
	// 外部请求线程
	mpExtRequestRunnable = new ExtRequestRunnable(this);

	// 内部服务(HTTP)参数
	miPort = 0;
	miMaxClient = 0;
	miMaxHandleThread = 0;
	miMaxQueryPerThread = 0;
	miTimeout = 0;

	// 媒体流服务(WebRTC)参数
	mWebRTCPortStart = 10000;
	mWebRTCMaxClient = 10;
	mWebRTCRtp2RtmpShellFilePath = "/root/Max/webrtc/bin/rtp2rtmp.sh";
	mbTurnUseSecret = false;
	mTurnUserName = "";
	mTurnPassword = "";
	mTurnShareSecret = "";
	mTurnClientTTL = 600;

	// 日志参数
	miStateTime = 0;
	miDebugMode = 0;
	miLogLevel = 0;

	// 信令服务(Websocket)参数
	miWebsocketPort = 0;
	miWebsocketMaxClient = 0;
	mbForceExtSync = true;
	miExtSyncStatusMaxTime = 10;
	miExtSyncStatusTime = 1;

	// 统计参数
	mTotal = 0;
	// 其他
	mRunning = false;
}

MediaServer::~MediaServer() {
	// TODO Auto-generated destructor stub
	Stop();

	if ( mpStateRunnable ) {
		delete mpStateRunnable;
		mpStateRunnable = NULL;
	}

	if ( mpTimeoutCheckRunnable ) {
		delete mpTimeoutCheckRunnable;
		mpTimeoutCheckRunnable = NULL;
	}

	if ( mpExtRequestRunnable ) {
		delete mpExtRequestRunnable;
		mpExtRequestRunnable = NULL;
	}
}

bool MediaServer::Start(const string& config) {
	if( config.length() > 0 ) {
		mConfigFile = config;

		// LoadConfig config
		if( LoadConfig() ) {
			return Start();

		} else {
			printf("# MediaServer can not load config file exit. \n");
		}

	} else {
		printf("# No config file can be use exit. \n");
	}

	return false;
}

bool MediaServer::Start() {
	bool bFlag = true;

	pid_t pid = getpid();

	LogManager::GetLogManager()->Start(miLogLevel, mLogDir);
	LogManager::GetLogManager()->SetDebugMode(miDebugMode);
	LogManager::GetLogManager()->LogSetFlushBuffer(1 * BUFFER_SIZE_1K * BUFFER_SIZE_1K);

	LogAync(
			LOG_ERR_SYS,
			"MediaServer::Start( "
			"############## MediaServer ############## "
			")"
			);

	LogAync(
			LOG_ERR_SYS,
			"MediaServer::Start( "
			"Version : %s, "
			"Build date : %s %s "
			")",
			VERSION_STRING,
			__DATE__,
			__TIME__
			);

	LogAync(
			LOG_ERR_SYS,
			"MediaServer::Start( "
			"[运行参数], "
			"pid : %d, "
			"mPidFilePath : %s "
			")",
			pid,
			mPidFilePath.c_str()
			);
	if ( mPidFilePath.length() > 0 ) {
		remove(mPidFilePath.c_str());

		char cmd[1024];
		snprintf(cmd, sizeof(cmd) - 1, "echo %d > %s", pid, mPidFilePath.c_str());
		system(cmd);
	}

	// 内部服务(HTTP)参数
	LogAync(
			LOG_WARNING,
			"MediaServer::Start( "
			"[内部服务(HTTP)], "
			"miPort : %d, "
			"miMaxClient : %d, "
			"miMaxHandleThread : %d, "
			"miMaxQueryPerThread : %d, "
			"miTimeout : %d, "
			"miStateTime : %d "
			")",
			miPort,
			miMaxClient,
			miMaxHandleThread,
			miMaxQueryPerThread,
			miTimeout,
			miStateTime
			);

	// 媒体流服务(WebRTC)
	LogAync(
			LOG_WARNING,
			"MediaServer::Start( "
			"[媒体流服务(WebRTC)], "
			"mWebRTCPortStart : %u, "
			"mWebRTCMaxClient : %u, "
			"mWebRTCRtp2RtmpShellFilePath : %s, "
			"mWebRTCRtp2RtmpBaseUrl : %s, "
			"mWebRTCDtlsCertPath : %s, "
			"mWebRTCDtlsKeyPath : %s, "
			"mWebRTCLocalIp : %s, "
			"mStunServerIp : %s, "
			"mStunServerExtIp : %s, "
			"mbTurnUseSecret : %u, "
			"mTurnUserName : %s, "
			"mTurnPassword : %s, "
			"mTurnShareSecret : %s, "
			"mTurnClientTTL : %u "
			")",
			mWebRTCPortStart,
			mWebRTCMaxClient,
			mWebRTCRtp2RtmpShellFilePath.c_str(),
			mWebRTCRtp2RtmpBaseUrl.c_str(),
			mWebRTCDtlsCertPath.c_str(),
			mWebRTCDtlsKeyPath.c_str(),
			mWebRTCLocalIp.c_str(),
			mStunServerIp.c_str(),
			mStunServerExtIp.c_str(),
			mbTurnUseSecret,
			mTurnUserName.c_str(),
			mTurnPassword.c_str(),
			mTurnShareSecret.c_str(),
			mTurnClientTTL
			);

	// 信令服务(Websocket)
	LogAync(
			LOG_WARNING,
			"MediaServer::Start( "
			"[信令服务(Websocket)], "
			"miWebsocketPort : %u, "
			"miWebsocketMaxClient : %u "
			")",
			miWebsocketPort,
			miWebsocketMaxClient
			);

	// 日志参数
	LogAync(
			LOG_WARNING,
			"MediaServer::Start( "
			"[日志服务], "
			"miDebugMode : %d, "
			"miLogLevel : %d, "
			"mlogDir : %s "
			")",
			miDebugMode,
			miLogLevel,
			mLogDir.c_str()
			);

	// 初始化全局属性
	HttpClient::Init();
	if( !WebRTC::GobalInit(mWebRTCDtlsCertPath, mWebRTCDtlsKeyPath, mStunServerIp, mWebRTCLocalIp, mbTurnUseSecret, mTurnUserName, mTurnPassword, mTurnShareSecret) ) {
		printf("# MediaServer(WebRTC) start Fail. \n");
		return false;
	}

	mServerMutex.lock();
	if( mRunning ) {
		Stop();
	}
	mRunning = true;

	// 启动子进程监听
	if( bFlag ) {
		bFlag = MainLoop::GetMainLoop()->Start();
		if( bFlag ) {
			LogAync(
					LOG_WARNING, "MediaServer::Start( event : [启动监听子进程循环-OK] )"
					);

		} else {
			LogAync(
					LOG_ERR_SYS, "MediaServer::Start( event : [启动监听子进程循环-Fail] )"
					);
			printf("# MediaServer(Loop) start Fail. \n");
		}
	}

	// 启动HTTP服务
	if( bFlag ) {
		bFlag = mAsyncIOServer.Start(miPort, miMaxClient, miMaxHandleThread);
		if( bFlag ) {
			LogAync(
					LOG_WARNING, "MediaServer::Start( event : [创建内部服务(HTTP)-OK] )"
					);

		} else {
			LogAync(
					LOG_ERR_SYS, "MediaServer::Start( event : [创建内部服务(HTTP)-Fail] )"
					);
			printf("# MediaServer(HTTP) start Fail. \n");
		}
	}

	// 启动Websocket服务
	if( bFlag ) {
		bFlag = mWSServer.Start(miWebsocketPort);
		if( bFlag ) {
			LogAync(
					LOG_WARNING, "MediaServer::Start( event : [创建内部服务(Websocket)-OK] )"
					);

		} else {
			LogAync(
					LOG_ERR_SYS, "MediaServer::Start( event : [创建内部服务(Websocket)-Fail] )"
					);
			printf("# MediaServer(Websocket) start Fail. \n");
		}
	}

	// 启动WebRTC服务
	if( bFlag ) {
		// WebRTC最大转发数
		for(unsigned int i = 0, port = mWebRTCPortStart; i < mWebRTCMaxClient; i++, port +=4) {
			WebRTC *rtc = new WebRTC();
			rtc->SetCallback(this);
			rtc->Init(mWebRTCRtp2RtmpShellFilePath, "127.0.0.1", port, "127.0.0.1", port + 2);
			mWebRTCList.PushBack(rtc);
		}

		// Websocket最大连接数
		for(unsigned int i = 0; i < miWebsocketMaxClient; i++) {
			MediaClient *client = new MediaClient();
			mMediaClientList.PushBack(client);
		}
	}

	// 启动超时处理线程
	if( bFlag ) {
		if( mTimeoutCheckThread.Start(mpTimeoutCheckRunnable, "Timeout") != 0 ) {
			LogAync(
					LOG_WARNING,
					"MediaServer::Start( "
					"event : [启动超时处理线程-OK] "
					")");
		} else {
			bFlag = false;
			LogAync(
					LOG_ERR_SYS,
					"MediaServer::Start( "
					"event : [启动超时处理线程-Fail] "
					")"
					);
			printf("# MediaServer(Timeout Thread) start Fail. \n");
		}
	}

	// 启动外部请求线程
	if( bFlag ) {
		if( mExtRequestThread.Start(mpExtRequestRunnable, "ExtRequest") != 0 ) {
			LogAync(
					LOG_WARNING,
					"MediaServer::Start( "
					"event : [启动外部请求线程-OK] "
					")");
		} else {
			bFlag = false;
			LogAync(
					LOG_ERR_SYS,
					"MediaServer::Start( "
					"event : [启动外部请求线程-Fail] "
					")"
					);
			printf("# MediaServer(ExtRequest Thread) start Fail. \n");
		}
	}

	// 启动状态监视线程
	if( bFlag ) {
		if( mStateThread.Start(mpStateRunnable, "State") != 0 ) {
			LogAync(
					LOG_WARNING,
					"MediaServer::Start( "
					"event : [启动状态监视线程-OK] "
					")");
		} else {
			bFlag = false;
			LogAync(
					LOG_ERR_SYS,
					"MediaServer::Start( "
					"event : [启动状态监视线程-Fail] "
					")"
					);
			printf("# MediaServer(State) start Fail. \n");
		}
	}

	if( bFlag ) {
		// 服务启动成功
		LogAync(
				LOG_WARNING,
				"MediaServer::Start( "
				"event : [OK] "
				")"
				);
		printf("# MediaServer start OK. \n");

	} else {
		// 服务启动失败
		LogAync(
				LOG_ERR_SYS,
				"MediaServer::Start( "
				"event : [Fail] "
				")"
				);
		printf("# MediaServer start Fail. \n");
		Stop();
	}
	mServerMutex.unlock();

	return bFlag;
}

bool MediaServer::LoadConfig() {
	bool bFlag = false;
	mConfigMutex.lock();
	if( mConfigFile.length() > 0 ) {
		ConfFile conf;
		conf.InitConfFile(mConfigFile.c_str(), "");
		if ( conf.LoadConfFile() ) {
			// 基本参数
			miPort = atoi(conf.GetPrivate("BASE", "PORT", "9880").c_str());
			miMaxClient = atoi(conf.GetPrivate("BASE", "MAXCLIENT", "100000").c_str());
			miMaxHandleThread = atoi(conf.GetPrivate("BASE", "MAXHANDLETHREAD", "2").c_str());
			miMaxQueryPerThread = atoi(conf.GetPrivate("BASE", "MAXQUERYPERCOPY", "10").c_str());
			miTimeout = atoi(conf.GetPrivate("BASE", "TIMEOUT", "10").c_str());
			miStateTime = atoi(conf.GetPrivate("BASE", "STATETIME", "30").c_str());
			mPidFilePath = conf.GetPrivate("BASE", "PID", "").c_str();

			// 日志参数
			miLogLevel = atoi(conf.GetPrivate("LOG", "LOGLEVEL", "5").c_str());
			mLogDir = conf.GetPrivate("LOG", "LOGDIR", "log");
			miDebugMode = atoi(conf.GetPrivate("LOG", "DEBUGMODE", "0").c_str());

			// WebRTC参数
			mWebRTCPortStart = atoi(conf.GetPrivate("WEBRTC", "WEBRTCPORTSTART", "10000").c_str());
			mWebRTCMaxClient = atoi(conf.GetPrivate("WEBRTC", "WEBRTCMAXCLIENT", "10").c_str());
			mWebRTCRtp2RtmpShellFilePath = conf.GetPrivate("WEBRTC", "RTP2RTMPSHELL", "script/rtp2rtmp.sh");
			mWebRTCRtp2RtmpBaseUrl = conf.GetPrivate("WEBRTC", "RTP2RTMPBASEURL", "rtmp://127.0.0.1:4000/cdn_flash/");
			mWebRTCDtlsCertPath = conf.GetPrivate("WEBRTC", "DTLSCER", "etc/webrtc_dtls.crt");
			mWebRTCDtlsKeyPath = conf.GetPrivate("WEBRTC", "DTLSKEY", "etc/webrtc_dtls.key");
			mWebRTCLocalIp = conf.GetPrivate("WEBRTC", "ICELOCALIP", "");
			mStunServerIp = conf.GetPrivate("WEBRTC", "STUNSERVERIP", "127.0.0.1");
			mStunServerExtIp = conf.GetPrivate("WEBRTC", "STUNSERVEREXTIP", "127.0.0.1");
			mStunServerExtIp = (mStunServerExtIp.length()==0)?mStunServerIp:mStunServerExtIp;
			mbTurnUseSecret = atoi(conf.GetPrivate("WEBRTC", "TURNSTATIC", "0").c_str());
			mTurnUserName = conf.GetPrivate("WEBRTC", "TURNUSERNAME", "MaxServer");
			mTurnPassword = conf.GetPrivate("WEBRTC", "TURNPASSWORD", "123");
			mTurnShareSecret = conf.GetPrivate("WEBRTC", "TURNSHARESECRET", "");
			mTurnClientTTL = atoi(conf.GetPrivate("WEBRTC", "TURNCLIENTTTL", "600").c_str());

			// Websocket参数
			miWebsocketPort = atoi(conf.GetPrivate("WEBSOCKET", "PORT", "9881").c_str());
			miWebsocketMaxClient = atoi(conf.GetPrivate("WEBSOCKET", "MAXCLIENT", "1000").c_str());
			mExtSetStatusPath = conf.GetPrivate("WEBSOCKET", "EXTSETSTATUSPATH", "");
			mExtSyncStatusPath = conf.GetPrivate("WEBSOCKET", "EXTSYNCPATH", "");

			bFlag = true;
		}
	}
	mConfigMutex.unlock();
	return bFlag;
}

bool MediaServer::ReloadLogConfig() {
	bool bFlag = false;
	mConfigMutex.lock();
	if( mConfigFile.length() > 0 ) {
		ConfFile conf;
		conf.InitConfFile(mConfigFile.c_str(), "");
		if ( conf.LoadConfFile() ) {
			// 基本参数
			miStateTime = atoi(conf.GetPrivate("BASE", "STATETIME", "30").c_str());

			// 日志参数
			miLogLevel = atoi(conf.GetPrivate("LOG", "LOGLEVEL", "5").c_str());
			miDebugMode = atoi(conf.GetPrivate("LOG", "DEBUGMODE", "0").c_str());

			// WebRTC参数
			mWebRTCRtp2RtmpShellFilePath = conf.GetPrivate("WEBRTC", "RTP2RTMPSHELL", "script/rtp2rtmp.sh");
			mWebRTCRtp2RtmpBaseUrl = conf.GetPrivate("WEBRTC", "RTP2RTMPBASEURL", "rtmp://127.0.0.1:4000/cdn_flash/");

			LogManager::GetLogManager()->SetLogLevel(miLogLevel);
			LogManager::GetLogManager()->SetDebugMode(miDebugMode);
			LogManager::GetLogManager()->LogSetFlushBuffer(1 * BUFFER_SIZE_1K * BUFFER_SIZE_1K);

			bFlag = true;
		}
	}
	mConfigMutex.unlock();
	return bFlag;
}

bool MediaServer::IsRunning() {
	return mRunning;
}

bool MediaServer::Stop() {
	LogAync(
			LOG_WARNING,
			"MediaServer::Stop("
			")"
			);

	mServerMutex.lock();

	if( mRunning ) {
		mRunning = false;

		// 停止监听socket
		mAsyncIOServer.Stop();
		// 停止监听Websocket
		mWSServer.Stop();
		// 停止定时任务
		mStateThread.Stop();
		mTimeoutCheckThread.Stop();
		mExtRequestThread.Stop();
		// 停止子进程监听循环
		MainLoop::GetMainLoop()->Stop();

		if ( mPidFilePath.length() > 0 ) {
			int ret = remove(mPidFilePath.c_str());
			mPidFilePath = "";
		}
	}

	mServerMutex.unlock();

	LogAync(
			LOG_WARNING,
			"MediaServer::Stop( "
			"event : [OK] "
			")"
			);
	printf("# MediaServer stop OK. \n");

	LogManager::GetLogManager()->Stop();

	return true;
}

void MediaServer::Exit() {
	pid_t pid = getpid();

	LogAync(
			LOG_WARNING,
			"MediaServer::Exit( "
			"pid : %d, "
			"mPidFilePath : %s "
			")",
			pid,
			mPidFilePath.c_str()
			);

	if ( mPidFilePath.length() > 0 ) {
		int ret = remove(mPidFilePath.c_str());
		mPidFilePath = "";
	}
}

/***************************** 定时任务 **************************************/
void MediaServer::StateHandle() {
	unsigned int iCount = 1;
	unsigned int iStateTime = miStateTime;

	unsigned int iTotal = 0;
	double iSecondTotal = 0;

	while( IsRunning() ) {
		if ( iCount < iStateTime ) {
			iCount++;

		} else {
			iCount = 1;
			iSecondTotal = 0;

			mCountMutex.lock();
			iTotal = mTotal;

			mTotal = 0;
			mCountMutex.unlock();

			LogAync(
					LOG_ERR_USER,
					"MediaServer::StateHandle( "
					"event : [状态服务], "
					"过去%u秒共收到请求(Websocket) : %u个 "
					")",
					iStateTime,
					iTotal
					);

			iStateTime = miStateTime;

		}
		sleep(1);
	}
}

void MediaServer::TimeoutCheckHandle() {
	while( IsRunning() ) {
		mWebRTCMap.Lock();
		for (WebsocketMap::iterator itr = mWebsocketMap.Begin(); itr != mWebsocketMap.End(); itr++) {
			MediaClient *client = itr->second;
			if ( client->IsTimeout() ) {
				LogAync(
						LOG_WARNING,
						"MediaServer::TimeoutCheckHandle( "
						"event : [超时处理服务:断开超时连接], "
						"hdl : %p "
						")",
						client->hdl.lock().get()
						);

				if ( client->connected ) {
					mWSServer.Disconnect(client->hdl);
					client->connected = false;
				}
			}
		}
		mWebRTCMap.Unlock();

		sleep(1);
	}
}

void MediaServer::ExtRequestHandle() {
	HttpClient httpClient;
	bool bFlag = false;

	while( IsRunning() ) {
		bFlag = HandleExtForceSync(&httpClient);

		ExtRequestItem *item = (ExtRequestItem *)mExtRequestList.PopFront();
		if ( item ) {
			switch (item->type) {
			case ExtRequestTypeLogin:{
				HandleExtLogin(&httpClient, item);
			}break;
			case ExtRequestTypeLogout:{
				HandleExtLogout(&httpClient, item);
			}break;
			default:break;
			}

			delete item;
		} else {
			sleep(miExtSyncStatusTime);
			if ( !bFlag ) {
				miExtSyncStatusTime++;
				miExtSyncStatusTime = (miExtSyncStatusTime >= miExtSyncStatusMaxTime)?miExtSyncStatusMaxTime:miExtSyncStatusTime;
			} else {
				miExtSyncStatusTime = 1;
			}
		}
	}
}

/***************************** 定时任务 **************************************/



/***************************** 内部服务(HTTP)回调 **************************************/
bool MediaServer::OnAccept(Client *client) {
	HttpParser* parser = new HttpParser();
	parser->SetCallback(this);
	parser->custom = client;
	client->parser = parser;

	LogAync(
			LOG_MSG,
			"MediaServer::OnAccept( "
			"parser : %p, "
			"client : %p "
			")",
			parser,
			client
			);

	return true;
}

void MediaServer::OnDisconnect(Client* client) {
	HttpParser* parser = (HttpParser *)client->parser;

	LogAync(
			LOG_MSG,
			"MediaServer::OnDisconnect( "
			"parser : %p, "
			"client : %p "
			")",
			parser,
			client
			);

	if( parser ) {
		delete parser;
		client->parser = NULL;
	}
}

void MediaServer::OnHttpParserHeader(HttpParser* parser) {
	Client* client = (Client *)parser->custom;

	LogAync(
			LOG_MSG,
			"MediaServer::OnHttpParserHeader( "
			"parser : %p, "
			"client : %p, "
			"path : %s "
			")",
			parser,
			client,
			parser->GetPath().c_str()
			);

	// 可以在这里处理超时任务
//	mAsyncIOServer.GetHandleCount();

	bool bFlag = HttpParseRequestHeader(parser);
	// 已经处理则可以断开连接
	if( bFlag ) {
		mAsyncIOServer.Disconnect(client);
	}
}

void MediaServer::OnHttpParserBody(HttpParser* parser) {
	Client* client = (Client *)parser->custom;

	bool bFlag = HttpParseRequestBody(parser);
	if ( bFlag ) {
		mAsyncIOServer.Disconnect(client);
	}
}

void MediaServer::OnHttpParserError(HttpParser* parser) {
	Client* client = (Client *)parser->custom;

	LogAync(
			LOG_WARNING,
			"MediaServer::OnHttpParserError( "
			"parser : %p, "
			"client : %p, "
			"path : %s "
			")",
			parser,
			client,
			parser->GetPath().c_str()
			);

	mAsyncIOServer.Disconnect(client);
}
/***************************** 内部服务(HTTP)回调 End **************************************/


/***************************** 内部服务(HTTP)接口 **************************************/
bool MediaServer::HttpParseRequestHeader(HttpParser* parser) {
	bool bFlag = true;

	if( parser->GetPath() == "/reload" ) {
		// 重新加载日志配置
		OnRequestReloadLogConfig(parser);
	} else {
		bFlag = false;
	}

	return bFlag;
}

bool MediaServer::HttpParseRequestBody(HttpParser* parser) {
	bool bFlag = false;

	{
		// 未知命令
		bFlag = OnRequestUndefinedCommand(parser);
	}

	return bFlag;
}

bool MediaServer::HttpSendRespond(
		HttpParser* parser,
		IRespond* respond
		) {
	bool bFlag = false;
	Client* client = (Client *)parser->custom;

//	LogAync(
//			LOG_MSG,
//			"MediaServer::HttpSendRespond( "
//			"event : [内部服务(HTTP)-返回请求到客户端], "
//			"parser : %p, "
//			"client : %p, "
//			"respond : %p "
//			")",
//			parser,
//			client,
//			respond
//			);

	// 发送头部
	char buffer[MAX_BUFFER_LEN];
	snprintf(
			buffer,
			MAX_BUFFER_LEN - 1,
			"HTTP/1.1 200 OK\r\n"
			"Connection:Close\r\n"
			"Content-Type:text/html; charset=utf-8\r\n"
			"\r\n"
			);
	int len = strlen(buffer);

	mAsyncIOServer.Send(client, buffer, len);

	if( respond != NULL ) {
		// 发送内容
		bool more = false;
		while( true ) {
			len = respond->GetData(buffer, MAX_BUFFER_LEN, more);
//			LogAync(
//					LOG_WARNING,
//					"MediaServer::HttpSendRespond( "
//					"event : [内部服务(HTTP)-返回请求内容到客户端], "
//					"parser : %p, "
//					"client : %p, "
//					"respond : %p, "
//					"buffer : %s "
//					")",
//					parser,
//					client,
//					respond,
//					buffer
//					);

			mAsyncIOServer.Send(client, buffer, len);

			if( !more ) {
				// 全部发送完成
				bFlag = true;
				break;
			}
		}
	}

	if( !bFlag ) {
		LogAync(
				LOG_WARNING,
				"MediaServer::HttpSendRespond( "
				"event : [内部服务(HTTP)-返回请求到客户端-Fail], "
				"parser : %p, "
				"client : %p "
				")",
				parser,
				client
				);
	}

	return bFlag;
}
/***************************** 内部服务(HTTP)接口 end **************************************/

/***************************** 内部服务(HTTP) 回调处理 **************************************/
void MediaServer::OnRequestReloadLogConfig(HttpParser* parser) {
	LogAync(
			LOG_WARNING,
			"MediaServer::OnRequestReloadLogConfig( "
			"event : [内部服务(HTTP)-收到命令:重新加载日志配置], "
			"parser : %p "
			")",
			parser
			);
	// 重新加载日志配置
	ReloadLogConfig();

	// 马上返回数据
	BaseResultRespond respond;
	HttpSendRespond(parser, &respond);
}

bool MediaServer::OnRequestUndefinedCommand(HttpParser* parser) {
	LogAync(
			LOG_WARNING,
			"MediaServer::OnRequestUndefinedCommand( "
			"event : [内部服务(HTTP)-收到命令:未知命令], "
			"parser : %p "
			")",
			parser
			);
	Client* client = (Client *)parser->custom;

	// 马上返回数据
	BaseResultRespond respond;
	respond.SetParam(false, "Undefined Command.");
	HttpSendRespond(parser, &respond);

	return true;
}

void MediaServer::OnWebRTCServerSdp(WebRTC *rtc, const string& sdp, WebRTCMediaType type) {
	connection_hdl hdl;
	bool bFound = false;

	mWebRTCMap.Lock();
	WebRTCMap::iterator itr = mWebRTCMap.Find(rtc);
	if( itr != mWebRTCMap.End() ) {
		MediaClient *client = itr->second;
		hdl = client->hdl;
		bFound = true;

		if ( bFound ) {
			Json::Value resRoot;
			Json::Value resData;
			Json::FastWriter writer;

			resRoot["id"] = client->id++;
			resRoot["route"] = "imRTC/sendSdpAnswerNotice";
			resRoot["errno"] = 0;
			resRoot["errmsg"] = "";

			resData["sdp"] = sdp;
			resRoot["req_data"] = resData;

			string res = writer.write(resRoot);
			mWSServer.SendText(hdl, res);
		}
	}
	mWebRTCMap.Unlock();

	LogAync(
			LOG_WARNING,
			"MediaServer::OnWebRTCServerSdp( "
			"event : [WebRTC-返回SDP], "
			"hdl : %p, "
			"rtc : %p, "
			"rtmpUrl : %s, "
			"type : %s "
//			"\nsdp:\n%s"
			")",
			hdl.lock().get(),
			rtc,
			rtc->GetRtmpUrl().c_str(),
			WebRTCMediaTypeString[type].c_str()
//			sdp.c_str()
			);
}

void MediaServer::OnWebRTCStartMedia(WebRTC *rtc) {
	connection_hdl hdl;
	bool bFound = false;

	mWebRTCMap.Lock();
	WebRTCMap::iterator itr = mWebRTCMap.Find(rtc);
	if( itr != mWebRTCMap.End() ) {
		MediaClient *client = itr->second;
		client->startMediaTime  = getCurrentTime();
		hdl = client->hdl;
		bFound = true;

		if( bFound ) {
			Json::Value resRoot;
			Json::FastWriter writer;

			resRoot["id"] = client->id++;
			resRoot["route"] = "imRTC/sendStartMediaNotice";
			resRoot["errno"] = 0;
			resRoot["errmsg"] = "";

			string res = writer.write(resRoot);
			mWSServer.SendText(hdl, res);
		}
	}
	mWebRTCMap.Unlock();

	LogAync(
			LOG_WARNING,
			"MediaServer::OnWebRTCStartMedia( "
			"event : [WebRTC-开始媒体传输], "
			"hdl : %p, "
			"rtc : %p, "
			"rtmpUrl : %s "
			")",
			hdl.lock().get(),
			rtc,
			rtc->GetRtmpUrl().c_str()
			);
}

void MediaServer::OnWebRTCError(WebRTC *rtc, WebRTCErrorType errType, const string& errMsg) {
	connection_hdl hdl;
	bool bFound = false;

	mWebRTCMap.Lock();
	WebRTCMap::iterator itr = mWebRTCMap.Find(rtc);
	if( itr != mWebRTCMap.End() ) {
		MediaClient *client = itr->second;
		hdl = client->hdl;
		bFound = true;

		if( bFound ) {
			Json::Value resRoot;
			Json::FastWriter writer;

			resRoot["id"] = client->id++;
			resRoot["route"] = "imRTC/sendErrorNotice";
			resRoot["errno"] = 0;
			resRoot["errmsg"] = errMsg;

			string res = writer.write(resRoot);
			mWSServer.SendText(hdl, res);

			if ( client->connected ) {
				mWSServer.Disconnect(hdl);
				client->connected = false;
			}
		}
	}
	mWebRTCMap.Unlock();

	LogAync(
			LOG_WARNING,
			"MediaServer::OnWebRTCError( "
			"event : [WebRTC-出错], "
			"hdl : %p, "
			"rtc : %p, "
			"rtmpUrl : %s, "
			"errType : %u, "
			"errMsg : %s "
			")",
			hdl.lock().get(),
			rtc,
			rtc->GetRtmpUrl().c_str(),
			errType,
			errMsg.c_str()
			);
}

void MediaServer::OnWebRTCClose(WebRTC *rtc) {
	connection_hdl hdl;
	bool bFound = false;

	mWebRTCMap.Lock();
	WebRTCMap::iterator itr = mWebRTCMap.Find(rtc);
	if( itr != mWebRTCMap.End() ) {
		MediaClient *client = itr->second;
		hdl = client->hdl;
		bFound = true;

		if( bFound ) {
			if ( client->connected ) {
				mWSServer.Disconnect(hdl);
				client->connected = false;
			}
		}
	}
	mWebRTCMap.Unlock();

	LogAync(
			LOG_WARNING,
			"MediaServer::OnWebRTCClose( "
			"event : [WebRTC-断开], "
			"hdl : %p, "
			"rtc : %p, "
			"rtmpUrl : %s "
			")",
			hdl.lock().get(),
			rtc,
			rtc->GetRtmpUrl().c_str()
			);

//	if( bFound ) {
//		mWSServer.Disconnect(hdl);
//	}
}

void MediaServer::OnWSOpen(WSServer *server, connection_hdl hdl, const string& addr, const string& userAgent) {
	long long currentTime = getCurrentTime();
	mServerMutex.lock();
	if( !mRunning ) {
		LogAync(
				LOG_ERR_SYS,
				"MediaServer::OnWSOpen( "
				"event : [Websocket-新连接, 服务器启动中...], "
				"hdl : %p, "
				"addr : %s, "
				"connectTime : %lld, "
				"userAgent : %s "
				")",
				hdl.lock().get(),
				addr.c_str(),
				currentTime,
				userAgent.c_str()
				);
		mWSServer.Disconnect(hdl);
	}
	mServerMutex.unlock();

	// Create UUID
	uuid_t uuid;
	uuid_generate(uuid);
	char uuid_str[64];
	uuid_unparse_upper(uuid, uuid_str);

	LogAync(
			LOG_WARNING,
			"MediaServer::OnWSOpen( "
			"event : [Websocket-新连接], "
			"hdl : %p, "
			"addr : %s, "
			"uuid : %s, "
			"connectTime : %lld, "
			"userAgent : %s "
			")",
			hdl.lock().get(),
			addr.c_str(),
			uuid_str,
			currentTime,
			userAgent.c_str()
			);

	MediaClient *client = mMediaClientList.PopFront();
	if ( client ) {
		client->hdl = hdl;
		client->connectTime = currentTime;
		client->connected = true;
		client->addr = addr;
		client->uuid = uuid_str;

		mWebRTCMap.Lock();
		mWebsocketMap.Insert(hdl, client);
		mMediaClientMap.Insert(uuid_str, client);
		mWebRTCMap.Unlock();

	} else {
		LogAync(
				LOG_ERR_SYS,
				"MediaServer::OnWSOpen( "
				"event : [超过最大连接数, 断开连接], "
				"hdl : %p, "
				"addr : %s, "
				"uuid : %s, "
				"connectTime : %lld, "
				"userAgent : %s "
				")",
				hdl.lock().get(),
				addr.c_str(),
				uuid_str,
				currentTime,
				userAgent.c_str()
				);
		mWSServer.Disconnect(hdl);
	}
}

void MediaServer::OnWSClose(WSServer *server, connection_hdl hdl) {
	long long connectTime = getCurrentTime();
	long long currentTime = connectTime;

	mServerMutex.lock();
	if( !mRunning ) {
		LogAync(
				LOG_WARNING,
				"MediaServer::OnWSClose( "
				"event : [Websocket-断开, 服务器启动中...], "
				"hdl : %p, "
				")",
				hdl.lock().get()
				);
	}
	mServerMutex.unlock();

	MediaClient *client = NULL;
	WebRTC *rtc = NULL;
	string addr = "";
	string uuid = "";

	mWebRTCMap.Lock();
	WebsocketMap::iterator itr = mWebsocketMap.Find(hdl);
	if ( itr != mWebsocketMap.End() ) {
		client = itr->second;

		if ( client->logined ) {
			// 插入外部注销通知
			ExtRequestItem *item = new ExtRequestItem();
			item->type = ExtRequestTypeLogout;
			item->uuid = client->uuid;
			item->extParam = client->extParam;
			mExtRequestList.PushBack(item);
		}

		// Remove rtc
		addr = client->addr;
		connectTime = client->connectTime;
		rtc = client->rtc;
		mWebRTCMap.Erase(rtc);

		// Remove uuid
		uuid = client->uuid;
		mMediaClientMap.Erase(uuid);

		client->Reset();
	}
	mWebsocketMap.Erase(hdl);
	mWebRTCMap.Unlock();

	LogAync(
			LOG_WARNING,
			"MediaServer::OnWSClose( "
			"event : [Websocket-断开], "
			"hdl : %p, "
			"rtc : %p, "
			"uuid : %s, "
			"addr : %s, "
			"aliveTime : %lldms "
			")",
			hdl.lock().get(),
			rtc,
			uuid.c_str(),
			addr.c_str(),
			currentTime - connectTime
			);

	if ( rtc ) {
		rtc->Stop();
		mWebRTCList.PushBack(rtc);
	}

	if ( client ) {
		mMediaClientList.PushBack(client);
	}
}

void MediaServer::OnWSMessage(WSServer *server, connection_hdl hdl, const string& str) {
	bool bFlag = false;
	bool bRespond = true;

	Json::Value reqRoot;
	Json::Reader reader;

	Json::Value resRoot;
	Json::Value resData = Json::Value::null;
	Json::FastWriter writer;

	mCountMutex.lock();
	mTotal++;
	mCountMutex.unlock();

	LogAync(
			LOG_STAT,
			"MediaServer::OnWSMessage( "
			"event : [Websocket-请求], "
			"hdl : %p, "
			"str : %s "
			")",
			hdl.lock().get(),
			str.c_str()
			);

	// Parse Json
	bool bParse = reader.parse(str, reqRoot, false);
	if ( bParse ) {
		if( reqRoot.isObject() ) {
			resRoot["id"] = reqRoot["id"];
			resRoot["route"] = reqRoot["route"];
			resRoot["errno"] = RequestErrorType_None;
			resRoot["errmsg"] = "";

			string route = "";
			if ( resRoot["route"].isString() ) {
				route = resRoot["route"].asString();
			}

			bParse = false;
			if ( reqRoot["route"].isString() ) {
				string route = reqRoot["route"].asString();
				if ( route == "imRTC/sendSdpCall" ) {
					if ( reqRoot["req_data"].isObject() ) {
						Json::Value reqData = reqRoot["req_data"];

						string stream = "";
						if( reqData["stream"].isString() ) {
							stream = reqData["stream"].asString();
						}

						string sdp = "";
						if( reqData["sdp"].isString() ) {
							sdp = reqData["sdp"].asString();
						}

						string rtmpUrl = mWebRTCRtp2RtmpBaseUrl;
						rtmpUrl += stream;

						LogAync(
								LOG_WARNING,
								"MediaServer::OnWSMessage( "
								"event : [Websocket-请求-拨号], "
								"hdl : %p, "
								"stream : %s "
								")",
								hdl.lock().get(),
								stream.c_str()
								);

						if( stream.length() > 0 && sdp.length() > 0 ) {
							WebRTC *rtc = NULL;

							mWebRTCMap.Lock();
							WebsocketMap::iterator itr = mWebsocketMap.Find(hdl);
							if ( itr != mWebsocketMap.End() ) {
								MediaClient *client = itr->second;
								client->callTime = getCurrentTime();

								if ( client->rtc ) {
									rtc = client->rtc;
								} else {
									rtc = mWebRTCList.PopFront();
									if ( rtc ) {
										client->rtc = rtc;
										mWebRTCMap.Insert(rtc, client);
									}
								}
							}
							mWebRTCMap.Unlock();

							if ( rtc ) {
								bFlag = rtc->Start(sdp, rtmpUrl);
								if ( bFlag ) {
									resData["rtmpUrl"] = rtmpUrl;
								} else {
									GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_WebRTC_Start_Fail);
								}

							} else {
								GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_WebRTC_No_More_WebRTC_Connection_Allow);
							}

						} else {
							GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_Request_Missing_Param);
						}
					}
				} else if ( route == "imRTC/sendSdpUpdate" ) {
					if ( reqRoot["req_data"].isObject() ) {
						Json::Value reqData = reqRoot["req_data"];

						string sdp = "";
						if( reqData["sdp"].isString() ) {
							sdp = reqData["sdp"].asString();
						}

						if( sdp.length() > 0 ) {
							WebRTC *rtc = NULL;

							mWebRTCMap.Lock();
							WebsocketMap::iterator itr = mWebsocketMap.Find(hdl);
							if ( itr != mWebsocketMap.End() ) {
								MediaClient *client = itr->second;
								rtc = client->rtc;
								if ( rtc ) {
									rtc->UpdateCandidate(sdp);
									bFlag = true;
								} else {
									GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_WebRTC_Update_Candidate_Before_Call);
								}
							} else {
								GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_Unknow_Error);
							}
							mWebRTCMap.Unlock();

						} else {
							GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_Request_Missing_Param);
						}
					}
				} else if ( mExtSetStatusPath.length() > 0 && route == "imRTC/login" ) {
					// Start External Login
					mWebRTCMap.Lock();
					WebsocketMap::iterator itr = mWebsocketMap.Find(hdl);
					if ( itr != mWebsocketMap.End() ) {
						MediaClient *client = itr->second;
						ExtRequestItem *item = new ExtRequestItem();
						item->type = ExtRequestTypeLogin;
						item->uuid = client->uuid;
						item->reqRoot = reqRoot;
						mExtRequestList.PushBack(item);
						bRespond = false;
						bFlag = true;
					} else {
						GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_Unknow_Error);
					}
					mWebRTCMap.Unlock();
				} else if ( route == "imRTC/sendGetToken" ) {
					// 这里只需要精确到秒
					char user[256] = {0};
					time_t timer = time(NULL);
					snprintf(user, sizeof(user) - 1, "%lu:client", timer + mTurnClientTTL);
					string password = Crypto::Sha1("mediaserver12345", user);
					Arithmetic art;
					string base64 = art.Base64Encode((const char *)password.c_str(), password.length());

					LogAync(
							LOG_WARNING,
							"MediaServer::OnWSMessage( "
							"event : [Websocket-请求-获取ICE配置], "
							"user : %s, "
							"base64 : %s "
							")",
							user,
							base64.c_str()
							);

					bFlag = true;

					Json::Value iceServers = Json::Value::null;
					Json::Value urls = Json::Value::null;

					char url[1024];
					snprintf(url, sizeof(url) - 1, "turn:%s?transport=tcp", mStunServerExtIp.c_str());
					urls.append(url);
					snprintf(url, sizeof(url) - 1, "turn:%s?transport=udp", mStunServerExtIp.c_str());
					urls.append(url);

					iceServers["urls"] = urls;
					iceServers["username"] = user;
					iceServers["credential"] = base64;

					resData["iceServers"].append(iceServers);

				} else {
					GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_Request_Unknow_Command);
				}
			} else {
				GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_Request_Unknow_Command);
			}
		}
	} else {
		GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_Request_Data_Format_Parse);
	}

	resRoot["data"] = resData;
	string res = writer.write(resRoot);

	if ( bRespond ) {
		mWSServer.SendText(hdl, res);
	}

	if ( !bFlag ) {
		LogAync(
				LOG_STAT,
				"MediaServer::OnWSMessage( "
				"event : [Websocket-请求出错], "
				"hdl : %p, "
				"str : %s, "
				"res : %s "
				")",
				hdl.lock().get(),
				str.c_str(),
				res.c_str()
				);

		mWSServer.Disconnect(hdl);
	}
}

void MediaServer::GetErrorObject(Json::Value &resErrorNo, Json::Value &resErrorMsg, RequestErrorType errType) {
	ErrObject obj = RequestErrObjects[errType];
	resErrorNo = obj.errNo;
	resErrorMsg = obj.errMsg;
}

bool MediaServer::HandleExtForceSync(HttpClient* httpClient) {
	bool bFlag = true;

	if ( mbForceExtSync && mExtSyncStatusPath.length() > 0 ) {
		bFlag = false;

		// Request HTTP
		long httpCode = 0;
		const char* res = NULL;
		int respondSize = 0;

		// Request JSON
		Json::Value reqRoot = Json::objectValue;
		Json::FastWriter writer;

		reqRoot["params"] = Json::arrayValue;
		mWebRTCMap.Lock();
		// Pop All Logout Request
		for(ExtRequestList::iterator itr =  mExtRequestList.Begin(); itr != mExtRequestList.End(); itr++) {
			ExtRequestItem *item = *itr;
			if ( item->type == ExtRequestTypeLogout ) {
				delete item;
				mExtRequestList.PopValueUnSafe(itr++);
			}
		}

		// Send Sync Online List
		for(WebsocketMap::iterator itr = mWebsocketMap.Begin(); itr != mWebsocketMap.End(); itr++) {
			MediaClient *client = itr->second;
			if ( client->logined ) {
				reqRoot["params"].append(client->extParam);
			}
		}
		mWebRTCMap.Unlock();

		string req = writer.write(reqRoot);

		HttpEntiy httpEntiy;
		httpEntiy.SetRawData(req.c_str());

		string url = mExtSyncStatusPath;
		if ( httpClient->Request(url.c_str(), &httpEntiy) ) {
			httpCode = httpClient->GetRespondCode();
			httpClient->GetBody(&res, respondSize);

			if( respondSize > 0 ) {
				// 发送成功
				Json::Value resRoot;
				Json::Reader reader;
				if( reader.parse(res, resRoot, false) ) {
					if( resRoot.isObject() ) {
						if( resRoot["ret"].isInt() ) {
							int errNo = resRoot["ret"].asInt();
							if ( errNo == 1 ) {
								bFlag = true;
								mbForceExtSync = false;
							}
						}
					}
				}
			}
		}

		LogAync(
				LOG_WARNING,
				"MediaServer::HandleExtForceSync( "
				"event : [外部同步在线状态-%s], "
				"url : %s, "
				"req : %s, "
				"res : %s "
				")",
				FLAG_2_STRING(bFlag),
				url.c_str(),
				req.c_str(),
				res
				);
	}

	return bFlag;
}

void MediaServer::HandleExtLogin(HttpClient* httpClient, ExtRequestItem *item) {
	bool bFlag = false;

	// Request Param
	Json::Value reqRoot = item->reqRoot;
	string param = "";

	// Respond
	Json::Value resRoot;
	Json::FastWriter writer;
	resRoot["id"] = reqRoot["id"];
	resRoot["route"] = reqRoot["route"];
	resRoot["errno"] = RequestErrorType_None;
	resRoot["errmsg"] = "";

	if ( reqRoot["req_data"].isObject() ) {
		Json::Value reqData = reqRoot["req_data"];

		if ( reqData["param"].isString() ) {
			param = reqData["param"].asString();
		}

		if( param.length() > 0 ) {
			bFlag = SendExtSetStatusRequest(httpClient, true, param);
			if ( !bFlag ) {
				resRoot["errno"] = RequestErrorType_Ext_Login_Error;
			}
		} else {
			GetErrorObject(resRoot["errno"], resRoot["errmsg"], RequestErrorType_Request_Missing_Param);
		}
	}

	// Callback to client
	mWebRTCMap.Lock();
	MediaClientMap::iterator itr = mMediaClientMap.Find(item->uuid);
	if ( itr != mMediaClientMap.End() ) {
		MediaClient *client = itr->second;
		if ( client->connected ) {
			string res = writer.write(resRoot);
			mWSServer.SendText(client->hdl, res);

			// 外部校验失败, 重复登录, 踢掉
			if ( bFlag && !client->logined ) {
				client->logined = true;
				client->extParam = param;
			} else {
				mWSServer.Disconnect(client->hdl);
				client->connected = false;
			}
		}
	} else {
		LogAync(
				LOG_WARNING,
				"MediaServer::HandleExtLogin( "
				"event : [外部登录, 客户端连接已经断开], "
				"uuid : %s, "
				"param : %s "
				")",
				item->uuid.c_str(),
				param.c_str()
				);
	}
	mWebRTCMap.Unlock();
}

void MediaServer::HandleExtLogout(HttpClient* httpClient, ExtRequestItem *item) {
	bool bFlag = false;
	bFlag = SendExtSetStatusRequest(httpClient, false, item->extParam);
}

bool MediaServer::SendExtSetStatusRequest(
		HttpClient* httpClient,
		bool isLogin,
		const string& param
		) {
	bool bFlag = false;

	long httpCode = 0;
	const char* res = NULL;
	int respondSize = 0;

	// Request
	Json::Value reqRoot;
	Json::FastWriter writer;
	reqRoot["param"] = param;
	reqRoot["status"] = (int)isLogin;
	string req = writer.write(reqRoot);

	HttpEntiy httpEntiy;
	httpEntiy.SetRawData(req);

	string url = mExtSetStatusPath;
	if ( httpClient->Request(url.c_str(), &httpEntiy) ) {
		httpCode = httpClient->GetRespondCode();
		httpClient->GetBody(&res, respondSize);

		if( respondSize > 0 ) {
			// 发送成功
			Json::Value resRoot;
			Json::Reader reader;
			if( reader.parse(res, resRoot, false) ) {
				if( resRoot.isObject() ) {
					if( resRoot["ret"].isInt() ) {
						int errNo = resRoot["ret"].asInt();
						if ( errNo == 1 ) {
							bFlag = true;
						}
					}
				}
			}
		}
	}

	if ( httpCode != 200 ) {
		mbForceExtSync = true;
	}

	LogAync(
			LOG_WARNING,
			"MediaServer::SendExtSetStatusRequest( "
			"event : [外部%s-%s], "
			"url : %s, "
			"req : %s, "
			"httpCode : %d, "
			"res : %s "
			")",
			isLogin?"登录":"注销",
			FLAG_2_STRING(bFlag),
			url.c_str(),
			req.c_str(),
			httpCode,
			res
			);

	return bFlag;
}
