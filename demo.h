#pragma once
#include "281_Mutex.h"
#include "281_Message.h"
#include "PTZ.h"
#include "Singleton.h"

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

class CameraParam {
public:
	// ����ͷ�Ƿ�����
	// ע��ʱ���Զ���Ϊ1��������������Ϊ1
	// �յ�bye�Զ���Ϊ0
	int alive;
	std::string Sip;
	std::string Ip;
	std::string UserName;
	std::string UserPwd;
	// ����ͷ������Ϣ�Ľӿ�
	int port;

	// ������Ƶ��ip
	std::string PlayIpAddr;
	// ������Ƶ��port
	int PlayPort;
	
	// ������rtmp��ַ
	std::string StreamIp;

	// ����ͷ�Ƿ���ע�ᵽsip������
	int registered;
	// ����ͷ�Ƿ����豸��Ϣ
	int cateloged;
	// ��Ƶ�Ƿ�ʼ���͵���Ƶ������
	int played;
	// ��Ƶ�������Ƿ�����
	int pushed;

	// ��������
	void push_stream();
};

struct gb28181Params {
	std::string localSipId;		//������sip
	std::string localIpAddr;	//������ip
	int localSipPort;			//���������Ͷ˿�	
	int SN;						//��������Ϣsn�룬��1����
};

struct ReqCamInfo {
	std::string Sip;
	std::vector<std::string> req;
};

struct ClientInfo {
	std::string Sip;
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
	std::vector<ClientInfo> clients;
	// ������
	int StreamType;
	
	Mutex mutex_;
	CondVar LVPcondvar; // unused

	// according sip search device(camera or client)
	// return: index of vec contains Sip
	//			-1 if not in CameraParams
	template<class C>
	int FindSipIndex(std::string sipid, C vec);
};

const char* whitespace_cb(mxml_node_t* node, int where);

static int Msg_is_message_fun(struct eXosip_t* peCtx, eXosip_event_t* je, LiveVideoParams* livevideoparams);

void Send_Catalogs(eXosip_t* ex, LiveVideoParams* livedioparams);

void Send_Catalog_Single(eXosip_t* ex, CameraParam *camerapar, LiveVideoParams* livevideoparams);

void Send_Invite_Play(eXosip_t* ex, LiveVideoParams* livevideoparams);

void Send_Invite_Play_Single(eXosip_t* ex, CameraParam* camerapar, LiveVideoParams* livevideoparams);

void Send_PtzControl_Single(eXosip_t* ex, CameraParam* camerapar, LiveVideoParams* livevideoparams, PTZ Ptz);
