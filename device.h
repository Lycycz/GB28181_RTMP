#pragma once
#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <cassert>

#include "281_Mutex.h"
#ifdef _DEBUG
	#include "glog/logging.h"
	#pragma comment(lib, "glog.lib")
#endif

extern std::vector<std::string> ReqString;

enum ReqType {
	R_DeviceControl,
	R_DeviceInfo,
	R_DeviceStatus,
	R_Catalog,
};

template <class T>
int VecIndex(T ele, std::vector<T> vec);

bool ReqValid(std::string req);

class Device {
public:
	Device(std::string sip = "None") :sip_(sip) {}
	virtual std::string GetSip();
protected:
	std::string sip_;
};


class ReqCam :public Device {
public:
	ReqCam(std::string sip) :Device(sip) {}
	ReqCam(std::string sip, std::string req);
	bool PushReq(std::string req);
	bool PopReq(std::string req);
	bool HasReq(std::string req);
	int ReqIndex(std::string req);
	void Print();
private:
	std::vector<std::string> Req;
};

#define ReqCamList std::vector<ReqCam>

class Client : public Device {
public:
	Client(std::string sip, std::string ip = "None", std::string port = "None") :Device(sip), ip_(ip), port_(port) {}
	bool AddReq(std::string sip, std::string req);
	bool DelReq(std::string sip, std::string req);

	int CamIndex(std::string CamSip);
	ReqCamList GetReqCamList();
	std::string GetIp();
	std::string GetPort();
private:
	std::string ip_;
	std::string port_;
	ReqCamList reqcamlist;
};

class ClientList {
public:
	bool AddClientReq(std::string clientsip, std::string clientip, std::string clientport, std::string camsip, std::string req);
	std::vector<Client>  DelClientReq(std::string camsip, std::string req);
	int ClientIndex(std::string sip);
	// 只在构造删除时的结果时使用，不会计算重复，reqcamlist为空，只使用sip,ip_,port_字段
	// unused
	void AddClient(Client client);

	std::vector<Client> Getclientlist();
private:
	std::vector<Client> clientlist;
	Mutex mutex_;
};

void PrintClientList(ClientList list);

std::string Req2str(ReqType req);
