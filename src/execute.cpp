#include "demo.h"
#include "PTZ.h"
#include "stdafx.h"
#include "rtp.h"
#include "srs_push.h"

int main(int argc, char** argv)
{
#ifdef _DEBUG
	FLAGS_alsologtostderr = 1;
	google::InitGoogleLogging(argv[0]);
	google::SetLogDestination(google::GLOG_INFO, "./log/");
#endif
	int ret = 0;
	osip_message_t* reg = NULL;
	osip_message_t* answer = NULL;
	eXosip_t* ex = NULL;
	char from[100] = { 0 };		/*sip:主叫用户名@被叫IP地址*/
	char proxy[100] = { 0 };	/*sip:被叫IP地址:被叫IP端口*/
	char contact[100] = { 0 };
	int  reg_id = 0;
	int reg_status = 0;
	int g_call_id = 0;

	ex = eXosip_malloc();
	ret = eXosip_init(ex);
	ret = eXosip_listen_addr(ex, IPPROTO_UDP, NULL, 5060, AF_INET, 0);

	LiveVideoParams::ReadCfg("E:/tmp_project/gb28281_demo/GB28181.txt", Singleton<LiveVideoParams>::Instance());

	std::vector<CameraPush> push_vec;
	for (auto& i : Singleton<LiveVideoParams>::Instance().CameraParams.camparlist) {
		CameraPush cam(i);
		push_vec.push_back(cam);
	}
	//sprintf(from, "sip:%s@%s", DEV_ID, DEV_IP);
	//sprintf(proxy, "sip:%s@%s:%d", SERVER_ID, SERVER_IP, SERVER_PORT);
	//sprintf(contact, "sip:%s@%s:%d", DEV_ID, DEV_IP, DEV_PORT);
	///*构建一个register*/
	//eXosip_lock(ex);
	//reg_id = eXosip_register_build_initial_register(ex, from, proxy, contact, 3600, &reg);
	//if (reg_id < 0)
	//{
	//	eXosip_unlock(ex);
	//	printf("eXosip_register_build_initial_register error!\r\n");
	//	return -1;
	//}
	////osip_message_set_authorization(reg, "Capability algorithm=\"H:MD5\"");
	///*发送register*/
	//ret = eXosip_register_send_register(ex, reg_id, reg);
	//eXosip_unlock(ex);
	//if (0 != ret)
	//{
	//	printf("eXosip_register_send_register no authorization error!\r\n");
	//	return -1;
	//}
	//printf("send reg msg \n");

	eXosip_lock(ex);
	eXosip_automatic_action(ex);
	eXosip_unlock(ex);

	//Send_DeviceStatus_Single(ex, &Singleton<LiveVideoParams>::Instance().CameraParams[0], 
	//	&Singleton<LiveVideoParams>::Instance());

	std::thread msgprocess(MsgProcess, ex, &Singleton<LiveVideoParams>::Instance());
	msgprocess.detach();

	// std::thread sendprocess(SendThread, ex, &livevideoparams);
	// Send_Catalog_Single(ex, livevideoparams.CameraParams[0], livevideoparams.gb28181params);
	// sendprocess.join();

	
	// Send_Catalog_Single(ex, &livevideoparams.CameraParams[0], livevideoparams.gb28181params);

	Sleep(5000);
	Send_Invite_Play(ex, &Singleton<LiveVideoParams>::Instance());
	Sleep(1000);

	while (1)
	{
		/*
		for (auto& i : push_vec) {
			auto param = i.GetParam();
			if (param->alive && param->played && !param->pushed) {
				//std::thread t1(std::mem_fn(&CameraPush::Run), i);
				//i.GetParam()->pushed = 1;
                std::thread t1(std::mem_fn(&CameraPush::Run), i);
				param->pushed = 1;
				t1.detach();
			}
		}
		*/

		for (auto& i : Singleton<LiveVideoParams>::Instance().CameraParams.camparlist) {

			if (i.alive && i.played && !i.pushed)
			{
				std::thread t1(std::mem_fn(&CameraParam::push_stream), i);
				i.pushed = 1;
				t1.detach();
			}
		}
		Sleep(1000);
	}

	//Send_Catalog_Single(ex, &livevideoparams.CameraParams[0], livevideoparams.gb28181params);
	return 0;
}
