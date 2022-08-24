#pragma once
#include "281_Mutex.h"
#include "281_Message.h"
#include "PTZ.h"
#include "Singleton.h"
#include "device.h"

#include <cstdio>
#include <winsock2.h>

#include "osip2/osip_mt.h"
#include "eXosip2/eXosip.h"
#include "pugixml.hpp"

#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"


#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>
#include <cassert>
#include <map>
#include <list>

#include <libconfig.h++>
#include <mxml.h>
#include <ctime>
#include <process.h>

#ifdef _DEBUG
#include "glog/logging.h"
#include "spdlogw.h"
#pragma comment(lib, "jrtplib_d.lib")
#pragma comment(lib, "jthread_d.lib")
#pragma comment(lib, "glog.lib")
#elif
#pragma comment(lib, "jrtplib.lib")
#pragma comment(lib, "jthread.lib")
#endif


#ifdef _MSC_VER
#pragma comment(lib, "legacy_stdio_definitions.lib")
#endif
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mxml1.lib")
#pragma comment(lib, "eXosip.lib")
#pragma comment(lib, "libcares.lib")
#pragma comment(lib, "osip2.lib")

#pragma comment(lib, "Dnsapi.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "osipparser2.lib")
#pragma comment(lib, "Qwave.lib")
#pragma comment(lib, "delayimp.lib")
#pragma comment(lib, "libconfig++.lib")

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "postproc.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")

class CameraParam {
public:
	// 摄像头是否在线
	// 注册时候自动置为1，心跳包保持置为1
	// 收到bye自动置为0
	int alive;
	std::string Sip;
	std::string Ip;
	std::string UserName;
	std::string UserPwd;
	// 摄像头接收信息的接口
	int port;

	// 发送视频的ip
	std::string PlayIpAddr;
	// 发送视频的port
	int PlayPort;
	
	// 推流的rtmp地址
	std::string StreamIp;

	// 摄像头是否已注册到sip服务器
	int registered;
	// 摄像头是否发送设备信息
	int cateloged;
	// 视频是否开始发送到视频服务器
	int played;
	// 视频服务器是否推流
	int pushed;

	// 推流函数
	void push_stream();
};

class CameraParamList {
public:
	std::vector<CameraParam> camparlist;
	Mutex mutex_;
};

struct gb28181Params {
	std::string localSipId;		//服务器sip
	std::string localIpAddr;	//服务器ip
	int localSipPort;			//服务器发送端口	
	int SN;						//服务器消息sn码，从1递增
};

class LiveVideoParams {
public:
	LiveVideoParams() : CameraNum(0), StreamType(0) {};

	int CameraNum;
	// contains mutex control
	CameraParamList CameraParams;
	// server pararms
	gb28181Params gb28181params;

	// contains mutex control
	ClientList clientlist;
	// 流类型
	int StreamType;

	// according sip search device(camera or client)
	// return: index of vec contains Sip
	//			-1 if not in CameraParams
	template<class C>
	int FindSipIndex(std::string sipid, C vec);
	static int ReadCfg(std::string cfgpath, LiveVideoParams& livevideoparams);
};

const char* whitespace_cb(mxml_node_t* node, int where);

static int Msg_is_message_fun(struct eXosip_t* peCtx, eXosip_event_t* je,
	LiveVideoParams* livevideoparams);

void Send_Catalogs(eXosip_t* ex, LiveVideoParams* livedioparams);

void Send_Catalog_Single(eXosip_t* ex, CameraParam *camerapar, LiveVideoParams* livevideoparams);

void Send_Invite_Play(eXosip_t* ex, LiveVideoParams* livevideoparams);

void Send_Invite_Play_Single(eXosip_t* ex, CameraParam* camerapar, LiveVideoParams* livevideoparams);

int PtzCmd_Build(PTZ ptz, char* ptzcmd, int Horizontal_speed = 0, int Vertical_speed = 0, int Zoom_speed = 0);

void Send_PtzControl_Single(eXosip_t* ex, CameraParam* camerapar, LiveVideoParams* livevideoparams, PTZ Ptz);

void MsgProcess(eXosip_t* ex, LiveVideoParams* livevideoparams);
