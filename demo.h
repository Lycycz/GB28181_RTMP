#pragma once
#include "281_Mutex.h"
#include "281_Message.h"
#include "PTZ.h"

#include <stdio.h>
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


#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>
#include <cassert>
#include <map>

#include <libconfig.h++>
#include <mxml.h>
#include <time.h>
#include <process.h>

//#pragma comment(lib, "jrtplib_d.lib")
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

#define SERVER_ID       ("34020000001320000001")
#define SERVER_IP       ("192.168.44.91")
#define SERVER_PORT     (5080)
#define DEV_ID          ("34020000002000000001")
#define DEV_IP          ("192.168.44.91")
#define DEV_PORT        (5060)
#define DEV_NAME        ("34020000")
#define DEV_PWD         ("12345678")

struct CameraParam {
	int alive;
	std::string SipId;
	std::string IpAddr;
	std::string UserName;
	std::string UserPwd;
	int port;

	std::string PlayIpAddr;
	int PlayPort;

	int registered;
	int cateloged;
	int played;
};

struct gb28181Params {
	std::string localSipId;	//服务器sip
	std::string localIpAddr;	//服务器ip
	int localSipPort;	//服务器发送端口	
	int SN;
};

struct ReqCamInfo {
	std::string Sip;
	std::vector<std::string> req;
};

struct ClientInfo {
	std::string Ip;
	std::string port;
	std::vector<ReqCamInfo> ReqCam;
};


class LiveVideoParams {
public:
	LiveVideoParams() : LVPcondvar(&mutex_), CameraNum(0), StreamType(0) {};

	int CameraNum;
	std::vector<CameraParam> CameraParams;
	gb28181Params gb28181params;
	// sip clientinfo
	std::map<std::string, ClientInfo> clients;
	int StreamType;
	
	Mutex mutex_;
	CondVar LVPcondvar; // unused

	// according sip search CameraParam
	// return: index of CameraParams:vector<CameraParam>
	//			-1 if not in CameraParams
	int FindCameraparam(std::string sipid);
};


const char* whitespace_cb(mxml_node_t* node, int where);

static int Msg_is_message_fun(struct eXosip_t* peCtx, eXosip_event_t* je, LiveVideoParams* livevideoparams);

void Send_Catalogs(eXosip_t* ex, LiveVideoParams* livedioparams);

void Send_Catalog_Single(eXosip_t* ex, CameraParam *camerapar, gb28181Params gb281281par);

void Send_Invite_Play(eXosip_t* ex, LiveVideoParams* livevideoparams);

void Send_Invite_Play_Single(eXosip_t* ex, CameraParam* camerapar, gb28181Params gb281281par);

void Send_PtzControl_Single(eXosip_t* ex, CameraParam* camerapar, gb28181Params gb28181par, PTZ Ptz);
