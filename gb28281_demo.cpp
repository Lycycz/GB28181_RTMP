// gb28281_demo.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define _CRT_SECURE_NO_WARNINGS
#include "demo.h"
#include "PTZ.h"
#include "stdafx.h"

LiveVideoParams livevideoparams;

int LiveVideoParams::FindCameraparam(std::string sipid) {
	auto it = std::find_if(CameraParams.begin(), CameraParams.end(),
		[&](CameraParam par)
		{
			return par.SipId == sipid;
		});
	std::vector<CameraParam>::iterator begin = CameraParams.begin();
	return std::distance(begin, it) < CameraParams.size() ? std::distance(begin, it) : -1;
}

const char* whitespace_cb(mxml_node_t* node, int where)
{
	return NULL;
}

void ReadCfg(std::string cfgpath, LiveVideoParams&livevidoparams)
{
	libconfig::Config config;
	config.readFile(cfgpath.c_str());
	livevideoparams.gb28181params.localSipId = config.lookup("server_id").c_str();
	livevideoparams.gb28181params.localIpAddr = config.lookup("server_ipaddr").c_str();
	livevideoparams.gb28181params.localSipPort = config.lookup("server_port");
	livevideoparams.gb28181params.SN = 1;

	libconfig::Setting& root = config.getRoot();
	libconfig::Setting& cameras = root["cameraParam"];
	livevideoparams.CameraNum = cameras.getLength();
	
	for (auto i = 0; i < cameras.getLength(); i++) {
		CameraParam campar;
		campar.SipId = static_cast<const char*>(cameras[i]["sipid"]);
		campar.IpAddr = static_cast<const char*>(cameras[i]["ipaddr"]);
		campar.UserName = static_cast<const char*>(cameras[i]["username"]);
		campar.UserPwd = static_cast<const char*>(cameras[i]["userpwd"]);
		campar.port = cameras[i]["port"];
		campar.PlayPort = cameras[i]["recvport"];
		campar.alive = 0;
		campar.registered = 0;
		campar.cateloged = 0;
		campar.played = 0;

		livevideoparams.CameraParams.push_back(campar);
	}


}

//TODO:get cmdtype unused
static void GetCmdType(char* xmlcontent, std::string& CmdValue, std::string& CmdType) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(xmlcontent);
	//doc.child("Notify").child("CmdType").text().as_string();
	CmdValue = doc.first_child().name();
	CmdType = doc.child(CmdValue.c_str()).child("CmdType").text().as_string();
	printf("%s \n", CmdType);
}

static void GetDeviceId(char* xmlcontent, std::string& DeviceID) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(xmlcontent);
	//doc.child("Notify").child("CmdType").text().as_string();
	std::string CmdValue = doc.first_child().name();
	DeviceID = doc.child(CmdValue.c_str()).child("DeviceID").text().as_string();
	printf("%s \n", DeviceID);
}

static void RegisterSuccess(struct eXosip_t* peCtx, eXosip_event_t* je, LiveVideoParams* livevideoparams)
{
	int iReturnCode = 0;
	osip_message_t* pSRegister = NULL;
	iReturnCode = eXosip_message_build_answer(peCtx, je->tid, 200, &pSRegister);
	if (iReturnCode == 0 && pSRegister != NULL)
	{
		eXosip_lock(peCtx);
		eXosip_message_send_answer(peCtx, je->tid, 200, pSRegister);
		eXosip_unlock(peCtx);
		//osip_message_free(pSRegister);
	}


	std::string username = je->request->from->url->username;
	auto it = livevideoparams->FindCameraparam(username);
	if (it >= 0 && (livevideoparams->CameraParams[it].alive != 1 ||
		livevideoparams->CameraParams[it].registered != 1))
	{
		livevideoparams->mutex_.Lock();
		//livevideoparams->LVPcondvar.Wait();
		livevideoparams->CameraParams[it].alive = 1;
		livevideoparams->CameraParams[it].registered = 1;
		livevideoparams->mutex_.Unlock();
	}

}

static void Msg_Forward(struct eXosip_t* peCtx, eXosip_event_t* je, char* buf, CameraParam* campar) {
	osip_message_t* message = NULL;
	char dest_call[256], source_call[256];
	_snprintf(dest_call, 256, "sip:%s@%s:%d", campar->SipId.c_str(), campar->IpAddr.c_str(),
		campar->port);
	_snprintf(source_call, 256, "sip:%s@%s:%d", livevideoparams.gb28181params.localSipId.c_str(),
		livevideoparams.gb28181params.localIpAddr.c_str(), livevideoparams.gb28181params.localSipPort);
	int ret = eXosip_message_build_request(peCtx, &message, "MESSAGE", dest_call, source_call, NULL);
	if (ret == 0 && message != NULL)
	{
		osip_message_set_body(message, buf, strlen(buf));
		osip_message_set_content_type(message, "Application/MANSCDP+xml");
		eXosip_lock(peCtx);
		eXosip_message_send_request(peCtx, message);
		eXosip_unlock(peCtx);
	}
}

/*  easy control
<EasyControl>
<CmdType>Zoomin</CmdType>
<DeviceID>3200000000000000000001</DeviceID>
</EasyControl>
*/
static void Msg_EasyControl(struct eXosip_t* peCtx, eXosip_event_t* je, CameraParam* campar, std::string easycontrolcmd) {
	PTZ ptz = ptzconvert(easycontrolcmd);
	Send_PtzControl_Single(peCtx, campar, livevideoparams.gb28181params, ptz);
}

static int Msg_is_message_fun(struct eXosip_t* peCtx, eXosip_event_t* je, LiveVideoParams* livevideoparams) {
	int ret;
	std::string CmdValue;
	std::string CmdType;
	std::string DeviceID;
	osip_body* body = nullptr;

	osip_message_get_body(je->request, 0, &body);
	if (body != nullptr) {
		GetCmdType(body->body, CmdValue, CmdType);
		GetDeviceId(body->body, DeviceID);
		int cam_index = livevideoparams->FindCameraparam(DeviceID);
		CameraParam* campar;
		if (cam_index >= 0)
			campar = &livevideoparams->CameraParams[cam_index];
		else
			return -1;

		if (strstr(CmdValue.c_str(), "Control") != nullptr) {
			Msg_Forward(peCtx, je, body->body, campar);
		}
		// TODO:简化逻辑
		else if (strstr(CmdValue.c_str(), "Query") != nullptr) {
			auto clients = livevideoparams->clients;
			auto it = clients[je->request->from->url->username];
			if (clients.find(je->request->from->url->username) != clients.end()) {
				std::vector <std::string> tmpcmd{ CmdType };
				ReqCamInfo tmpReqCamIndo{ DeviceID, std::move(tmpcmd) };
				std::vector<ReqCamInfo> ReqCamInfoVec{std::move(tmpReqCamIndo)};
				livevideoparams->clients.insert(
					std::map<std::string, ClientInfo>::value_type(je->request->from->url->username,
						ClientInfo{ je->request->from->url->host,
					je->request->from->url->port, std::move(ReqCamInfoVec) }));
			}
			else {
				auto Camit = std::find_if(it.ReqCam.begin(), it.ReqCam.end(),
					[&](ReqCamInfo caminfo)
					{
						return	caminfo.Sip == DeviceID;
					});
				if (Camit == it.ReqCam.end()) {
					std::vector<std::string> tmpcmd{ CmdType };
					it.ReqCam.push_back(ReqCamInfo{ DeviceID, std::move(tmpcmd) });
				}
				else {
					(*Camit).req.push_back(CmdType);
				}
			}

			Msg_Forward(peCtx, je, body->body, campar);
			//je->request->from->url->host
			//je->request->from->url->port
		}
		else if (strstr(CmdValue.c_str(), "Notify") != nullptr) {
			if (strstr(CmdType.c_str(), "Keeplive") != nullptr) {
				livevideoparams->mutex_.Lock();
				livevideoparams->CameraParams[cam_index].alive = 1;
				livevideoparams->CameraParams[cam_index].registered = 1;
				livevideoparams->mutex_.Unlock();
			}
		}
		else if (strstr(CmdValue.c_str(), "EasyControl") != nullptr) {
			Msg_EasyControl(peCtx, je, campar, CmdType);
		}
		// TODO:将请求从列表中删除
		else if (strstr(CmdValue.c_str(), "Response") != nullptr) {

			std::cout << "response" << std::endl;
		}
		else {
			printf("msg body: %s \n", body->body);
		}
	}
	else {
		printf("get body error!");
	}
	RegisterSuccess(peCtx, je, livevideoparams);
	return 0;
}

// sip receive thread
void MsgProcess(eXosip_t* ex, LiveVideoParams* livevideoparams)
{
	eXosip_event_t* je = NULL;
	while (1)
	{
		je = eXosip_event_wait(ex, 0, 50);
		if (je == NULL)
		{
			osip_usleep(100000);
			continue;
		}
		printf("type: %d \n", je->type);

		switch (je->type)
		{
			/*注册成功*/
		case EXOSIP_REGISTRATION_SUCCESS:
		{
			printf("reg sussess \n");
			//reg_status = 1;
		}
		break;
		/*注册失败*/
		case EXOSIP_REGISTRATION_FAILURE:
		{
			printf("status_code = %d \n", je->response->status_code);

			/*收到服务器返回的注册失败/401未认证状态*/
			if ((NULL != je->response) && (401 == je->response->status_code))
			{
				int ret;
				osip_message* reg = NULL;
				/*发送携带认证信息的注册请求*/
				eXosip_lock(ex);
				eXosip_clear_authentication_info(ex);/*清除认证信息*/
				/*添加主叫用户的认证信息*/
				eXosip_add_authentication_info(ex, "zjf", "zjf", "12345678", "MD5", NULL);
				eXosip_register_build_register(ex, je->rid, 3600, &reg);
				ret = eXosip_register_send_register(ex, je->rid, reg);
				eXosip_unlock(ex);
				if (0 != ret)
				{
					printf("eXosip_register_send_register authorization error!\r\n");
					//return -1;
				}
				osip_message_free(reg);
			}
			else
			{
				printf("reg failed \n");
			}
		}
		break;

		case EXOSIP_MESSAGE_NEW:
		{
			printf("EXOSIP_MESSAGE_NEW!\n");
			if (MSG_IS_MESSAGE(je->request))/*使用MESSAGE方法的请求*/
			{
				//按照规则，需要回复200 OK信息
				printf("recv message \n");
				Msg_is_message_fun(ex, je, livevideoparams);
			}
			else if (MSG_IS_REGISTER(je->request))
			{
				printf("recv registor \n");
				RegisterSuccess(ex, je, livevideoparams);
			}
			else if (MSG_IS_BYE(je->request)) {
				printf("recv bye \n");
				std::string username = je->request->from->url->username;
				auto it = livevideoparams->FindCameraparam(username);
				if (it >= 0 && (livevideoparams->CameraParams[it].alive != 0 ||
					livevideoparams->CameraParams[it].registered != 0))
				{
					livevideoparams->mutex_.Lock();
					//livevideoparams->LVPcondvar.Wait();
					livevideoparams->CameraParams[it].alive = 0;
					livevideoparams->CameraParams[it].played = 0;
					livevideoparams->mutex_.Unlock();
				}
			}
			else if (je->response!=nullptr) {
				std::cout << "a";
			}
			else
			{
				printf("unspported methode: %s \n", je->request->sip_method);
			}
		}
		break;
		case EXOSIP_MESSAGE_REQUESTFAILURE:
		{
			printf("通用应答:对方收到消息格式错误\n");
		}
		break;

		case EXOSIP_CALL_PROCEEDING:
		{
			printf("\nEXOSIP_CALL_PROCEEDING\n");
			RegisterSuccess(ex, je, livevideoparams);
		}
		break;
		case EXOSIP_CALL_INVITE:
		{
			printf("\nEXOSIP_CALL_INVITE\n");
			osip_body* body = nullptr;
			osip_message_get_body(je->request, 0, &body);
		/*	if (g_call_id > 0)
			{
				printf("the call is exist ! \n");
				break;
			}*/
		}
		break;

		case EXOSIP_CALL_ANSWERED:
		{
			osip_message_t* ack = NULL;
			osip_body* body = nullptr;
			osip_message_get_body(je->response, 0, &body);

			eXosip_call_build_ack(ex, je->did, &ack);
			eXosip_call_send_ack(ex, je->did, ack);
		}
		break;
		
		case EXOSIP_CALL_MESSAGE_NEW:
		{
			osip_body* body = nullptr;
			osip_message_get_body(je->request, 0, &body);
			if (strncmp(je->request->sip_method, "BYE", 4) == 0) {
				printf("methode: %s \n", je->request->sip_method);
			}
			// printf("msg body : %s \n", body->body);
		}
		break;
		case EXOSIP_CALL_CLOSED:
		{
			osip_body* body = nullptr;
			osip_message_get_body(je->request, 0, &body);
		}
		default:
			break;
		}
	}
}

//TODO:添加catalog和inviteplay
void SendThread(eXosip_t* ex, LiveVideoParams* livevideoparams) {
	while (1) {
		Send_Catalogs(ex, livevideoparams);
		Send_Invite_Play(ex, livevideoparams);
		Sleep(1);
	}
}

void Send_Catalogs(eXosip_t* ex, LiveVideoParams* livevideoparams) {
	int num = livevideoparams->CameraNum;
	for (auto i = 0; i < num; i++) {
		CameraParam* camerpar = &livevideoparams->CameraParams[i];
		if(camerpar->registered && !camerpar->cateloged)
			Send_Catalog_Single(ex, camerpar, livevideoparams->gb28181params);
	}
}

// 验证通过
void Send_Catalog_Single(eXosip_t* ex, CameraParam *camerapar, gb28181Params gb28181par) {
	int ret;
	std::string deviceId = gb28181par.localSipId;

	mxml_node_t* tree, * query, * node;
	tree = mxmlNewXML("1.0");
	char sn[32];
	if (tree != NULL)
	{
		query = mxmlNewElement(tree, "Query");
		if (query != NULL)
		{
			char buf[256] = { 0 };
			char dest_call[256], source_call[256];
			node = mxmlNewElement(query, "CmdType");
			mxmlNewText(node, 0, "Catalog");
			node = mxmlNewElement(query, "SN");
			_snprintf(sn, 32, "%d", livevideoparams.gb28181params.SN++);
			mxmlNewText(node, 0, sn);
			node = mxmlNewElement(query, "DeviceID");
			mxmlNewText(node, 0, camerapar->SipId.c_str());
			mxmlSaveString(tree, buf, 256, whitespace_cb);

			osip_message_t* message = NULL;
			_snprintf(dest_call, 256, "sip:%s@%s:%d", camerapar->SipId.c_str(), camerapar->IpAddr.c_str(),
				camerapar->port);
			_snprintf(source_call, 256, "sip:%s@%s:%d", livevideoparams.gb28181params.localSipId.c_str(),
				livevideoparams.gb28181params.localIpAddr.c_str(), livevideoparams.gb28181params.localSipPort);
			ret = eXosip_message_build_request(ex, &message, "MESSAGE", dest_call, source_call, NULL);
			if (ret == 0 && message != NULL)
			{
				osip_message_set_body(message, buf, strlen(buf));
				osip_message_set_content_type(message, "Application/MANSCDP+xml");
				eXosip_lock(ex);
				eXosip_message_send_request(ex, message);
				eXosip_unlock(ex);

				camerapar->cateloged = 1;
			}
			else {
				printf("eXosip_message_build_request failed \n");
			}
		}
		else {
			printf("mxmlNewElement Query failed \n");
		}
		mxmlDelete(tree);
	}
	else {
		printf("mxmlNewXml failed\n");
	}
}

void Send_DeviceStatus_Single(eXosip_t* ex, CameraParam* camerapar, gb28181Params gb28181par) {
	int ret;
	char sn[32];

	mxml_node_t* tree, * query, * node;
	tree = mxmlNewXML("1.0");
	if (tree != NULL) {
		query = mxmlNewElement(tree, "Query");
		if (query != NULL) {
			char buf[256] = { 0 };
			char dest_call[256], source_call[256];
			node = mxmlNewElement(query, "CmdType");
			mxmlNewText(node, 0, "DeviceStatus");
			node = mxmlNewElement(query, "SN");
			_snprintf(sn, 32, "%d", livevideoparams.gb28181params.SN++);
			mxmlNewText(node, 0, sn);
			node = mxmlNewElement(query, "DeviceID");
			mxmlNewText(node, 0, camerapar->SipId.c_str());
			mxmlSaveString(tree, buf, 256, whitespace_cb);

			osip_message_t* message = NULL;
			_snprintf(dest_call, 256, "sip:%s@%s:%d", camerapar->SipId.c_str(), camerapar->IpAddr.c_str(),
				camerapar->port);
			_snprintf(source_call, 256, "sip:%s@%s:%d", livevideoparams.gb28181params.localSipId.c_str(),
				livevideoparams.gb28181params.localIpAddr.c_str(), livevideoparams.gb28181params.localSipPort);
			ret = eXosip_message_build_request(ex, &message, "MESSAGE", dest_call, source_call, NULL);
			if (ret == 0 && message != NULL)
			{
				osip_message_set_body(message, buf, strlen(buf));
				osip_message_set_content_type(message, "Application/MANSCDP+xml");
				eXosip_lock(ex);
				eXosip_message_send_request(ex, message);
				eXosip_unlock(ex);
			}
		}
		mxmlDelete(tree);
	}
}

void Send_DeviceInfo_Single(eXosip_t* ex, CameraParam* camerapar, gb28181Params gb28181par) {
	int ret;
	char sn[32];

	mxml_node_t* tree, * query, * node;
	tree = mxmlNewXML("1.0");
	if (tree != NULL) {
		query = mxmlNewElement(tree, "Query");
		if (query != NULL) {
			char buf[256] = { 0 };
			char dest_call[256], source_call[256];
			node = mxmlNewElement(query, "CmdType");
			mxmlNewText(node, 0, "DeviceInfo");
			node = mxmlNewElement(query, "SN");
			_snprintf(sn, 32, "%d", livevideoparams.gb28181params.SN++);
			mxmlNewText(node, 0, sn);
			node = mxmlNewElement(query, "DeviceID");
			mxmlNewText(node, 0, camerapar->SipId.c_str());
			mxmlSaveString(tree, buf, 256, whitespace_cb);

			osip_message_t* message = NULL;
			_snprintf(dest_call, 256, "sip:%s@%s:%d", camerapar->SipId.c_str(), camerapar->IpAddr.c_str(),
				camerapar->port);
			_snprintf(source_call, 256, "sip:%s@%s:%d", livevideoparams.gb28181params.localSipId.c_str(),
				livevideoparams.gb28181params.localIpAddr.c_str(), livevideoparams.gb28181params.localSipPort);
			ret = eXosip_message_build_request(ex, &message, "MESSAGE", dest_call, source_call, NULL);
			if (ret == 0 && message != NULL)
			{
				osip_message_set_body(message, buf, strlen(buf));
				osip_message_set_content_type(message, "Application/MANSCDP+xml");
				eXosip_lock(ex);
				eXosip_message_send_request(ex, message);
				eXosip_unlock(ex);
			}
		}
		mxmlDelete(tree);
	}
}

void Send_Invite_Play(eXosip_t* ex, LiveVideoParams* livevideoparams) {
	int num = livevideoparams->CameraNum;
	for (auto i = 0; i < num; i++) {
		CameraParam* camerpar = &livevideoparams->CameraParams[i];
		if (camerpar->registered && !camerpar->played) {
			Send_Invite_Play_Single(ex, camerpar, livevideoparams->gb28181params);

			livevideoparams->mutex_.Lock();
			camerpar->played = 1;
			livevideoparams->mutex_.Unlock();
		}
	}
}

void Send_Invite_Play_Single(eXosip_t* ex, CameraParam* camerapar, gb28181Params gb28181par) {
	int ret;

	char dest_call[256], source_call[256], subject[128];
	osip_message_t* invite = NULL;

	_snprintf(dest_call, 256, "sip:%s@%s:%d", camerapar->SipId.c_str(), camerapar->IpAddr.c_str(),
		camerapar->port);
	_snprintf(source_call, 256, "sip:%s@%s:%d", livevideoparams.gb28181params.localSipId.c_str(),
		livevideoparams.gb28181params.localIpAddr.c_str(), livevideoparams.gb28181params.localSipPort);
	_snprintf(subject, 128, "%s:0,%s:0", camerapar->SipId.c_str(), gb28181par.localSipId.c_str());
	ret = eXosip_call_build_initial_invite(ex, &invite, dest_call, source_call, NULL, subject);
	if (ret != 0) {
		printf("eXosip_call_build_initial_invite failed, %s,%s,%s", dest_call, source_call, subject);
	}

	char body[500];
	int bodyLen = _snprintf(body, 500,
		"v=0\r\n"
		"o=%s 0 0 IN IP4 %s\r\n"
		"s=Play\r\n"
		"c=IN IP4 %s\r\n"
		"t=0 0\r\n"
		"m=video %d RTP/AVP 96 97 98\r\n"
		//"m=video %d RTP/AVP 96\r\n"
		"a=rtpmap:96 PS/90000\r\n"
		"a=rtpmap:97 MPEG4/90000\r\n"
		"a=rtpmap:98 H264/90000\r\n"
		"a=recvonly\r\n"
		"y=0000001024\r\n"
		"f=\r\n", gb28181par.localSipId.c_str(), gb28181par.localIpAddr.c_str(),
		gb28181par.localIpAddr.c_str(), camerapar->PlayPort);
	osip_message_set_body(invite, body, bodyLen);
	osip_message_set_content_type(invite, "APPLICATION/SDP");
	eXosip_lock(ex);
	eXosip_call_send_initial_invite(ex, invite);
	eXosip_unlock(ex);
}

int PtzCmd_Build(PTZ ptz, char* ptzcmd, int Horizontal_speed=0, int Vertical_speed=0, int Zoom_speed=0) {
	int ptz_1 = 10*16 + 5;
	int ptz_2 = (10 + 5 + VERSION) % 16;
	int ptz_3 = 0;
	int ptz_4 = 1 << (5 - ptz);
	int ptz_5;
	int ptz_6;
	int ptz_7;

	if (ptz == PTZ::DOWN || ptz == PTZ::UP) {
		ptz_5 = Horizontal_speed;
		ptz_6 = 16 * 12 + 8;
		ptz_7 = Zoom_speed;
	}
	else if(ptz == PTZ::LEFT || ptz == PTZ::RIGHT){
		ptz_5 = 16 * 12 + 8;
		ptz_6 = Vertical_speed;
		ptz_7 = Zoom_speed;
	}
	else {
		ptz_5 = Horizontal_speed;
		ptz_6 = Vertical_speed;
		ptz_7 = 16 * 8 + 0;
	}
	int ptz_8 = (ptz_1 + ptz_2 + ptz_3 + ptz_4 + ptz_5 + ptz_6 + ptz_7) % 256;

	sprintf(ptzcmd, "%02x%02x%02x%02x%02x%02x%02x%02x",
		ptz_1, ptz_2, ptz_3, ptz_4, ptz_5, ptz_6, ptz_7, ptz_8);
	_strupr(ptzcmd);
	// printf("%s \n", ptzcmd);
	return 0;
}

// 验证通过 
void Send_PtzControl_Single(eXosip_t* ex, CameraParam* camerapar, gb28181Params gb28181par, PTZ Ptz) {
	char sn[32];
	char PtzCmd[20];
	int ret;
	ret = PtzCmd_Build(Ptz, PtzCmd);

	mxml_node_t* tree, * control, * node;
	tree = mxmlNewXML("1.0");
	if (tree != NULL)
	{
		control = mxmlNewElement(tree, "Control");
		if (control != NULL) {
			char buf[256] = { 0 };
			char dest_call[256], source_call[256];

			node = mxmlNewElement(control, "CmdType");
			mxmlNewText(node, 0, "DeviceControl");
			node = mxmlNewElement(control, "SN");
			_snprintf(sn, 32, "%d", gb28181par.SN++);
			mxmlNewText(node, 0, sn);
			node = mxmlNewElement(control, "DeviceID");
			mxmlNewText(node, 0, camerapar->SipId.c_str());
			node = mxmlNewElement(control, "PTZCmd");
			mxmlNewText(node, 0, PtzCmd);
			mxmlSaveString(tree, buf, 256, whitespace_cb);

			osip_message_t* message = NULL;
			_snprintf(dest_call, 256, "sip:%s@%s:%d", camerapar->SipId.c_str(), camerapar->IpAddr.c_str(),
				camerapar->port);
			_snprintf(source_call, 256, "sip:%s@%s:%d", livevideoparams.gb28181params.localSipId.c_str(),
				livevideoparams.gb28181params.localIpAddr.c_str(), livevideoparams.gb28181params.localSipPort);
			ret = eXosip_message_build_request(ex, &message, "MESSAGE", dest_call, source_call, NULL);

			if (ret == 0 && message != NULL)
			{
				osip_message_set_body(message, buf, strlen(buf));
				osip_message_set_content_type(message, "Application/MANSCDP+xml");
				eXosip_lock(ex);
				eXosip_message_send_request(ex, message);
				eXosip_unlock(ex);
			}
		}
		mxmlDelete(tree);
	}
}

//int main()
//{
//	int ret = 0;
//	osip_message_t* reg = NULL;
//	osip_message_t* answer = NULL;
//	eXosip_t* ex = NULL;
//	char from[100] = { 0 };/*sip:主叫用户名@被叫IP地址*/
//	char proxy[100] = { 0 };/*sip:被叫IP地址:被叫IP端口*/
//	char contact[100] = { 0 };
//	int  reg_id = 0;
//	int reg_status = 0;
//	int g_call_id = 0;
//
//	ex = eXosip_malloc();
//	ret = eXosip_init(ex);
//	ret = eXosip_listen_addr(ex, IPPROTO_UDP, NULL, 5060, AF_INET, 0);
//	
//	ReadCfg("E:/tmp_project/gb28281_demo/GB28181.txt", livevideoparams);
//	
//	//sprintf(from, "sip:%s@%s", DEV_ID, DEV_IP);
//	//sprintf(proxy, "sip:%s@%s:%d", SERVER_ID, SERVER_IP, SERVER_PORT);
//	//sprintf(contact, "sip:%s@%s:%d", DEV_ID, DEV_IP, DEV_PORT);
//	///*构建一个register*/
//	//eXosip_lock(ex);
//	//reg_id = eXosip_register_build_initial_register(ex, from, proxy, contact, 3600, &reg);
//	//if (reg_id < 0)
//	//{
//	//	eXosip_unlock(ex);
//	//	printf("eXosip_register_build_initial_register error!\r\n");
//	//	return -1;
//	//}
//	////osip_message_set_authorization(reg, "Capability algorithm=\"H:MD5\"");
//	///*发送register*/
//	//ret = eXosip_register_send_register(ex, reg_id, reg);
//	//eXosip_unlock(ex);
//	//if (0 != ret)
//	//{
//	//	printf("eXosip_register_send_register no authorization error!\r\n");
//	//	return -1;
//	//}
//	//printf("send reg msg \n");
//	
//	eXosip_lock(ex);
//	eXosip_automatic_action(ex);
//	eXosip_unlock(ex);
//	
//	
//	std::thread msgprocess(MsgProcess, ex, &livevideoparams);
//	//std::thread sendprocess(SendThread, ex, &livevideoparams);
//	//Send_Catalog_Single(ex, livevideoparams.CameraParams[0], livevideoparams.gb28181params);
//	//sendprocess.join();
//	
//	Sleep(5000);
//	Send_Catalog_Single(ex, &livevideoparams.CameraParams[0], livevideoparams.gb28181params);
//	//Send_DeviceStatus_Single(ex, &livevideoparams.CameraParams[0], livevideoparams.gb28181params);
//	//Send_Catalog_Single(ex, &livevideoparams.CameraParams[0], livevideoparams.gb28181params);
//	msgprocess.join();
//	return 0;
//}

