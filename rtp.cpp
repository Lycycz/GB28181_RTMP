#define _CRT_SECURE_NO_WARNINGS
#include "rtp.h"

unsigned long parse_time_stamp(const unsigned char* p)
{
	unsigned long b;
	//共33位，溢出后从0开始
	unsigned long val;

	//第1个字节的第5、6、7位
	b = *p++;
	val = (b & 0x0e) << 29;

	//第2个字节的8位和第3个字节的前7位
	b = (*(p++)) << 8;
	b += *(p++);
	val += ((b & 0xfffe) << 14);

	//第4个字节的8位和第5个字节的前7位
	b = (*(p++)) << 8;
	b += *(p++);
	val += ((b & 0xfffe) >> 1);

	return val;
}

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << jrtplib::RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

int read_data(void* opaque, uint8_t* buf, int buf_size)
{
	memcpy(buf, static_cast<Frame*>(opaque)->data, static_cast<Frame*>(opaque)->size);
	buf_size = static_cast<Frame*>(opaque)->size;
	//int tmp = buf_size;
	return buf_size;
}

AVFormatContext* Init_ofmt_ctx(CameraParam* camerapar) {
	int ret;
	AVFormatContext* ifmt_ctx = NULL, * ofmt_ctx = NULL;
	AVOutputFormat* ofmt = NULL;
	//const char* out_filename = camerapar->StreamIp.c_str();
	const char* out_filename = "rtmp://192.168.44.91/live/home";

	const char* in_filename = "1.h264";

	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		printf("Could not open input file '%s'", in_filename);
	}

	// 1.2 解码一段数据，获取流相关信息
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf("Failed to retrieve input stream information");
	}
	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename);
	ofmt = ofmt_ctx->oformat;
	if (!ofmt_ctx) {
		printf("Could not create output context\n");
		ret = AVERROR_UNKNOWN;
	}

	AVStream* in_stream = ifmt_ctx->streams[0];
	AVCodecParameters* in_codecpar = in_stream->codecpar;

	AVStream* out_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!out_stream) {
		printf("Failed allocating output stream\n");
		ret = AVERROR_UNKNOWN;
	}
	ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
			av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, ret);
			printf("Could not open output URL '%s', %s", out_filename, av_error);
		}
	}
	return ofmt_ctx;
}

void jrtplib_rtp_recv_thread(CameraParam* camerapar) {
	//int ret;
	//AVFormatContext* ifmt_ctx = NULL, * ofmt_ctx = NULL;
	//AVOutputFormat* ofmt = NULL;
	//// const char* out_filename = camerapar->StreamIp.c_str();
	//const char* out_filename = "rtmp://192.168.44.91/live/home";
	//
	//const char* in_filename = "1.h264";
	////const char* in_filename = "rtp://192.168.44.91/live/home";

	//if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
	//	printf("Could not open input file '%s'", in_filename);
	//}

	//// 1.2 解码一段数据，获取流相关信息
	//if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
	//	printf("Failed to retrieve input stream information");
	//}
	//av_dump_format(ifmt_ctx, 0, in_filename, 0);
	//
	//avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename);
	//ofmt = ofmt_ctx->oformat;
	//if (!ofmt_ctx) {
	//	printf("Could not create output context\n");
	//	ret = AVERROR_UNKNOWN;
	//}
	//
	//AVStream* in_stream = ifmt_ctx->streams[0];
	//AVCodecParameters* in_codecpar = in_stream->codecpar;

	//AVStream* out_stream = avformat_new_stream(ofmt_ctx, NULL);
	//if (!out_stream) {
	//	printf("Failed allocating output stream\n");
	//	ret = AVERROR_UNKNOWN;
	//}
	//ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
	//av_dump_format(ofmt_ctx, 0, out_filename, 1);

	//if (!(ofmt->flags & AVFMT_NOFILE)) {
	//	ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
	//	if (ret < 0) {
	//		char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	//		av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, ret);
	//		printf("Could not open output URL '%s', %s", out_filename, av_error);
	//	}
	//}

	int ret;
	AVFormatContext* ofmt_ctx = Init_ofmt_ctx(camerapar);

	/* 计算 duration 和 framerate，暂时指定framerate为25帧，duration为40
	AVRational frame_rate;
	double duration;
	if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
		frame_rate = av_guess_frame_rate(ifmt_ctx, in_stream, NULL);
		duration = ((frame_rate.num && frame_rate.den) ? av_q2d(AVRational{ frame_rate.den, frame_rate.num }) : 0);
		// auto pts = av_rescale_q(duration, AVRational{ 1000, 1 }, in_stream->time_base);
		// duration = duration / av_q2d(in_stream->time_base);
		duration = duration / av_q2d(AVRational{ 1, 1000 });
	}
	*/

	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		printf("Error occurred when opening output URL\n");
	}

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);
#endif // RTP_SOCKETTYPE_WINSOCK

	jrtplib::RTPSession sess;
	uint16_t portbase;
	std::string ipstr;
	int status, i, num;

	jrtplib::RTPUDPv4TransmissionParams transparams;
	jrtplib::RTPSessionParams sessparams;

	sessparams.SetOwnTimestampUnit(1.0 / 25.0);

	portbase = camerapar->PlayPort;

	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(portbase);
	status = sess.Create(sessparams, &transparams);
	sess.SetDefaultPayloadType(96);
	checkerror(status);

	std::vector<Frame> FrameVector;

	char* frame = new char[1024 * 100];
	memset(frame, 0, sizeof(frame));
	char* returnps = new char[1024 * 100];
	memset(returnps, 0, sizeof(returnps));

	i = 0;

	while (camerapar->played && camerapar->alive)
	{
		sess.BeginDataAccess();

		// check incoming packets
		if (sess.GotoFirstSourceWithData())
		{
			do
			{
				jrtplib::RTPPacket* pack;
				while ((pack = sess.GetNextPacket()) != NULL)
				{
					// You can examine the data here
					//printf("Got packet!\n");
					if (pack->GetPayloadType() == 96) {
						// maker ps tail
						if (pack->HasMarker())
						{
							//caculate frame len 
							//add ps tail data
							int frame_len = std::accumulate(FrameVector.begin(), FrameVector.end(), 0,
								[](int sum, Frame a) {
									return sum + a.size;
								}) + pack->GetPayloadLength();
								std::cout << frame_len << std::endl;

								int size = 0;

								for (auto i : FrameVector) {
									memcpy(frame + size, i.data, i.size);
									size += i.size;
								}

								memcpy(frame + size, pack->GetPayloadData(), pack->GetPayloadLength());
								/*std::cout << pack->GetPayloadData() << std::endl;
								std::cout << frame + size << std::endl;*/
								int iPsLength;
								int pts;
								int flag;

								GetH246FromPs(frame, frame_len, &returnps, &iPsLength, pts, flag);

								AVPacket packet;
								packet.buf = nullptr;
								packet.data = reinterpret_cast<uint8_t*>(returnps);
								packet.size = iPsLength;
								packet.dts = pts * av_q2d(AVRational{ 1, 90000 }) / av_q2d(AVRational{ 1, 1000 });
								packet.pts = packet.dts;
								packet.flags = flag;
								packet.stream_index = 0;
								packet.duration = 40;	//3600/90;
								packet.pos = -1;
								packet.side_data = nullptr;
								packet.side_data_elems = 0;

								int64_t now_time = av_gettime() - start_time;
								LOG(INFO) << now_time;
								if (packet.pts > now_time) {
									av_usleep(packet.pts - now_time);

									if (flag) firstflag = 1;
									// AVFormatContext* ic = nullptr;
									// AVIOContext* pb = NULL;
									// AVInputFormat* piFmt = NULL;

									/*
									ic = avformat_alloc_context();
									int BUF_SIZE = 4096*20;
									Frame tmpbuffer{ iPsLength, h264buffer };
									uint8_t* buf = (uint8_t*)av_mallocz(sizeof(uint8_t) * BUF_SIZE);
									pb = avio_alloc_context(buf, BUF_SIZE, 0, &tmpbuffer, read_data, NULL, NULL);
									ic->pb = pb;

									if (av_probe_input_buffer(pb, &piFmt, "", NULL, 0, 0) < 0)
									{
										fprintf(stderr, "probe format failed\n");
									}
									ret = avformat_open_input(&ic, "", piFmt, NULL);
									*/

									// AVPacket packet;
									//av_read_frame(ic, &packet);

									// out_stream = ofmt_ctx->streams[packet.stream_index];
									if (firstflag)
										ret = av_interleaved_write_frame(ofmt_ctx, &packet);

									//av_packet_from_data(packet, reinterpret_cast<uint8_t*>(h264buffer), iPsLength);

									/*
									AVFrame* avframe = av_frame_alloc();
									int ret;
									*/

									//ret = avcodec_send_packet(codecCtx, packet);
									//ret = avcodec_receive_frame(codecCtx, avframe);

									for (int i = 0; i < FrameVector.size(); i++) {
										if (FrameVector[i].data != nullptr) {
											delete[] FrameVector[i].data;
											FrameVector[i].data = nullptr;
										}
									}
									FrameVector.clear();
								}
								else {
									/*
									if (pack->GetPacketData()[12] == 0x00 && pack->GetPacketData()[13] == 0x00 &&
										pack->GetPacketData()[14] == 0x01 && pack->GetPacketData()[15] == 0xc0)
									{

									}
									else if (pack->GetPacketData()[12] == 0x00 && pack->GetPacketData()[13] == 0x00 &&
										pack->GetPacketData()[14] == 0x01 && pack->GetPacketData()[15] == 0xbd)
									{

									}

									else {
									*/
									char* tmp = new char[pack->GetPayloadLength()];
									memcpy(tmp, pack->GetPayloadData(), pack->GetPayloadLength());
									FrameVector.push_back(Frame{ static_cast<int>(pack->GetPayloadLength()), tmp });
									//delete[] tmp;
								//}
								}
						}
						else {
							std::cout << "no" << std::endl;
						}
						//写入文件
						std::cout << int(pack->GetPayloadLength()) << std::endl;
						if (pack->HasMarker())
							std::cout << "maker" << std::endl;
						// fwrite(pack->GetPayloadData(), 1, pack->GetPayloadLength(), fpH264);

					// we don't longer need the packet, so
					// we'll delete it
						sess.DeletePacket(pack);
					}
				}

			} while (sess.GotoNextSourceWithData());

			sess.EndDataAccess();

#ifndef RTP_SUPPORT_THREAD
			status = sess.Poll();
			checkerror(status);
#endif // RTP_SUPPORT_THREAD

			//jrtplib::RTPTime::Wait(jrtplib::RTPTime(10, 0));
		}

		delete[] frame;
		delete[] returnps;

#ifdef RTP_SOCKETTYPE_WINSOCK
		WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	}

}
