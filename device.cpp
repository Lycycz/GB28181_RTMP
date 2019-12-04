#include "device.h"

std::vector<std::string> ReqString = { "DeviceControl", "DeviceInfo", "DeviceStatus", "Catalog" };

bool ReqValid(std::string req) {
	return VecIndex(req, ReqString) >= 0;
}

std::string Req2str(ReqType req) {
	return ReqString[req];
}

template <class T>
int VecIndex(T ele, std::vector<T> vec) {
	for (size_t i = 0; i < vec.size(); i++)
		if (vec[i] == ele) return i;
	return -1;
}

std::string Device::GetSip() {
	return sip_;
}

ReqCam::ReqCam(std::string sip, std::string req) :Device(sip) {
	assert(ReqValid(req));
#ifdef _DEBUG
	LOG(INFO) << "新增请求:" << req;
#endif
	Req.push_back(req);
}

bool ReqCam::HasReq(std::string req) {
	return VecIndex(req, Req) >= 0;
}

int ReqCam::ReqIndex(std::string req) {
	return VecIndex(req, Req);
}

bool ReqCam::PushReq(std::string req) {
	assert(ReqValid(req));
	if (!HasReq(req)) {
#ifdef _DEBUG
		LOG(INFO) << "新增请求:" << req;
#endif
		Req.push_back(req);
	}
	else
#ifdef _DEBUG
		LOG(INFO) << "重复请求，已存在";
#endif
	return true;
}

bool ReqCam::PopReq(std::string req) {
	assert(ReqValid(req));
	int index = ReqIndex(req);
	if (index >= 0)
		Req.erase(Req.begin() + index);
	return true;
}

void ReqCam::Print() {
	for (auto i : Req)
		std::cout << i << std::endl;
}

int Client::CamIndex(std::string CamSip) {
	std::vector<std::string> CamSipList;
	for (auto i : reqcamlist)
		CamSipList.push_back(i.GetSip());
	return VecIndex(CamSip, CamSipList);
}

bool Client::AddReq(std::string sip, std::string req) {
	int index = CamIndex(sip);
	if (index >= 0) {
		reqcamlist[index].PushReq(req);
	}
	else {
#ifdef _DEBUG
		LOG(INFO) << "新增摄像机:" << sip;
#endif
		reqcamlist.push_back(ReqCam(sip, req));
	}
	return true;
}

bool Client::DelReq(std::string sip, std::string req) {
	int index = CamIndex(sip);
	if (index >= 0) {
		reqcamlist[index].PopReq(req);
	}
	else {
#ifdef _DEBUG
		LOG(INFO) << "不存在摄像机:" << sip;
#endif
	}
	return true;
}

std::string Client::GetIp() {
	return this->ip_;
}

std::string Client::GetPort() {
	return this->port_;
}

ReqCamList Client::GetReqCamList() { return reqcamlist; }

int ClientList::ClientIndex(std::string clientsip) {
	std::vector<std::string> SipList;
	for (auto i : clientlist)
		SipList.push_back(i.GetSip());
	return VecIndex(clientsip, SipList);
}

bool ClientList::AddClientReq(std::string clientsip, std::string clientip, std::string clientport, std::string camsip, std::string req) {
	this->mutex_.Lock();
	int index = ClientIndex(clientsip);
	if (index >= 0) {
		assert(clientlist[index].GetIp() == clientip && clientlist[index].GetPort() == clientport);
		clientlist[index].AddReq(camsip, req);
	}
	else {
#ifdef _DEBUG
		LOG(INFO) << "新增客户端: " << clientsip;
#endif
		clientlist.push_back(Client(clientsip,clientip,clientport));
		clientlist.back().AddReq(camsip, req);
	}
	this->mutex_.Unlock();
	return true;
}

std::vector<Client> ClientList::DelClientReq(std::string camsip, std::string req) {
	this->mutex_.Lock();
	std::vector<Client>  result;
	for (auto& client : this->clientlist) {
		result.push_back(client);
		client.DelReq(camsip, req);
	}
	this->mutex_.Unlock();
	return result;
}

void ClientList::AddClient(Client client) {
	this->clientlist.push_back(client);
}

std::vector<Client> ClientList::Getclientlist() { return clientlist; }

void PrintClientList(ClientList list) {
	for (auto i : list.Getclientlist())
	{
		std::cout << "ClientSip: " << i.GetSip() << std::endl;
		for (auto j : i.GetReqCamList()) {
			std::cout << "CamSip: " << j.GetSip() << std::endl;
			//std::cout <<"ReqType: "<< std::endl;
			j.Print();
		}
		std::cout << std::endl;
	}
}