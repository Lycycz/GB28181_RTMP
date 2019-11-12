#include "demo.h"
#include "rtp.h"

#define _CRT_SECURE_NO_WARNINGS

#pragma once
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
};

#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtppacket.h"

#include <vector>
#include <deque>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include <numeric> 

struct Frame {
	int size;
	char* data;
};
const char * out_filename = "rtmp://localhost/live/home";
AVFormatContext* ifmt_ctx = NULL, * ofmt_ctx = NULL;
AVOutputFormat* ofmt = NULL;

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
	return buf_size;
}

void jrtplib_rtp_recv_thread(CameraParam* camerapar) {
	int ret;

	
	//avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename);
	//ofmt = ofmt_ctx->oformat;

	
	AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);

	AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
	codecCtx->height = 1280;
	codecCtx->width = 720;
	codecCtx->time_base.num = 1; //这两行：一秒钟25帧
	codecCtx->time_base.den = 25;
	codecCtx->bit_rate = 0; //初始化为0
	codecCtx->frame_number = 1; //每包一个视频帧
	codecCtx->codec_type = AVMEDIA_TYPE_VIDEO;

	/*
	AVStream* out_stream = avformat_new_stream(ofmt_ctx, codec);
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		out_stream->codecpar->codec_tag |= AV_CODEC_FLAG_GLOBAL_HEADER;
	
	avcodec_open2(codecCtx, codec, nullptr);

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf("Could not open output URL '%s'", out_filename);
		}
	}
	*/

	//ret = avformat_write_header(ofmt_ctx, NULL);

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

	FILE* fpH264;
	errno_t err = fopen_s(&fpH264, "1.h264", "wb");
	
	
	std::deque <Frame> cache;
	std::vector<Frame> FrameVector;

	i = 0;



while (camerapar->played&& camerapar->alive)
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


				// ps head
				if (pack->GetPayloadType() == 96) {
					if (pack->HasMarker())
					{
						int frame_len = std::accumulate(FrameVector.begin(), FrameVector.end(), 0,
							[](int sum, Frame a) {
								return sum + a.size;
							}) + pack->GetPayloadLength();
							std::cout << frame_len << std::endl;

							char* frame = new char[frame_len];
							memset(frame, 0, frame_len);
							int size = 0;

							for (auto i : FrameVector) {
								memcpy(frame + size, i.data, i.size);
								size += i.size;
							}
							memcpy(frame + size, pack->GetPayloadData(), pack->GetPayloadLength());

							Frame* tmpframe = new Frame();
							tmpframe->data = new char[frame_len];
							tmpframe->size = 0;


							int h264length = 0;
							int pts;
							int flag;

							if (frame[0] == '\x00' && frame[1] == '\x00' &&
								frame[2] == '\x01' && frame[3] == '\xba') {

								program_stream_pack_header* PsHead = (program_stream_pack_header*)frame;
								unsigned char pack_stuffing_length = PsHead->stuffinglen & '\x07';

								int leftlength = frame_len - sizeof(program_stream_pack_header) - pack_stuffing_length;//减去头和填充的字节
								char* NextPack = frame + sizeof(program_stream_pack_header) + pack_stuffing_length;


								if (NextPack && (NextPack)[0] == '\x00' && (NextPack)[1] == '\x00' &&
									(NextPack)[2] == '\x01' && (NextPack)[3] == '\xBB') {
									program_stream_pack_bb_header* pbbHeader = (program_stream_pack_bb_header*)(NextPack);
									unsigned char bbheaderlen = pbbHeader->num2;
									NextPack = NextPack + sizeof(program_stream_pack_bb_header) + bbheaderlen;
									leftlength = leftlength - sizeof(program_stream_pack_bb_header) - bbheaderlen;
								}

								char* PayloadData = NULL;
								int PayloadDataLen = 0;
					
								while (leftlength > sizeof(pack_start_code)) {
									PayloadData = NULL;
									PayloadDataLen = 0;

									if (NextPack && NextPack[0] == '\x00' && NextPack[1] == '\x00'
										&& NextPack[2] == '\x01' && NextPack[3] == '\xE0') {
										if (Pes(NextPack, leftlength, &NextPack, &leftlength, &PayloadData, &PayloadDataLen)) {
											if (PayloadDataLen)
											{
												memcpy(tmpframe->data + tmpframe->size, PayloadData, PayloadDataLen);
												tmpframe->size += PayloadDataLen;
												h264length += PayloadDataLen;
											}
										}
										else
										{
											if (PayloadDataLen)
											{
												memcpy(tmpframe->data + tmpframe->size, PayloadData, PayloadDataLen);
												tmpframe->size += PayloadDataLen;
												h264length += PayloadDataLen;
											}

											break;
										}
									}
									else if (NextPack && NextPack[0] == '\x00' && NextPack[1] == '\x00'
										&& NextPack[2] == '\x01' && NextPack[3] == '\xBC') {
										if (ProgramStreamMap(NextPack, leftlength, &NextPack, &leftlength, &PayloadData, &PayloadDataLen) == 0)
											break;
										else
											flag = 1;
									}
									else if (NextPack && NextPack[0] == '\x00' && NextPack[1] == '\x00'
										&& NextPack[2] == '\x01' && NextPack[3] == '\xBA') {
										program_stream_pack_header* PsHead = (program_stream_pack_header*)frame;
										unsigned char pack_stuffing_length = PsHead->stuffinglen & '\x07';

										leftlength = leftlength - sizeof(program_stream_pack_header) - pack_stuffing_length;//减去头和填充的字节
										NextPack = NextPack + sizeof(program_stream_pack_header) + pack_stuffing_length;
									}
									else {
										printf("[%s]no konw %x %x %x %x\n", __FUNCTION__, NextPack[0], NextPack[1], NextPack[2], NextPack[3]);
										break;
									}

								}

							}
							

							AVPacket packet;

							/*						
							AVFormatContext* ic = nullptr;
							AVIOContext* pb = NULL;
							AVInputFormat* piFmt = NULL;

							ic = avformat_alloc_context();
							int BUF_SIZE = 4096 * 10;
							uint8_t* buf = (uint8_t*)av_mallocz(sizeof(uint8_t) * BUF_SIZE);
							pb = avio_alloc_context(buf, BUF_SIZE, 0, tmpframe, read_data, NULL, NULL);
							ic->pb = pb;
							
							

							if (av_probe_input_buffer(pb, &piFmt, "", NULL, 0, 0) < 0)
							{
								fprintf(stderr, "probe format failed\n");
							}
	
							err = avformat_open_input(&ic, "", piFmt, NULL);
							
							AVPacket packet;
							av_read_frame(ic, &packet);
							

							out_stream = ofmt_ctx->streams[packet.stream_index];
							*/
							av_new_packet(&packet, h264length);
							memcpy(packet.data, tmpframe->data, tmpframe->size);
							packet.size = h264length;
							packet.pts = 0;
							packet.dts = 0;
							packet.flags = 1;
							packet.pos = 0;

							//ret = av_interleaved_write_frame(ofmt_ctx, &packet);

							AVFrame* avframe = av_frame_alloc();
							int ret;

							ret = avcodec_send_packet(codecCtx, &packet);
							ret = avcodec_receive_frame(codecCtx, avframe);
								
	
							/*avformat_close_input(&ic);
							av_frame_free(&avframe);
							av_freep(&pb->buffer);
							
							av_freep(&pb);
							avformat_free_context(ic);
							for (auto i : FrameVector)
								delete[] i.data;
							delete[] frame;
							delete tmpframe;
							av_packet_unref(&packet);*/
							

							FrameVector.clear();
						}
						else {
						/*	if (pack->GetPacketData()[12] == 0x00 && pack->GetPacketData()[13] == 0x00 &&
								pack->GetPacketData()[14] == 0x01 && pack->GetPacketData()[15] == 0xc0)
							{

							}
							else if (pack->GetPacketData()[12] == 0x00 && pack->GetPacketData()[13] == 0x00 &&
								pack->GetPacketData()[14] == 0x01 && pack->GetPacketData()[15] == 0xbd)
							{

							}
							
							else {
							*/
								//jrtplib::RTPPacket tmppack = *pack;
								char* tmp = new char[pack->GetPayloadLength()];
								memcpy(tmp, pack->GetPayloadData(), pack->GetPayloadLength());
								//std::cout << tmp << std::endl;
								FrameVector.push_back(Frame{ static_cast<int>(pack->GetPayloadLength()), tmp });
							//}
						}
				}
				else {
					std::cout << "no" << std::endl;
				}
					
					//std::cout << pack->GetPayloadData() << std::endl;
						//写入文件
					/*	std::cout << int(pack->GetPayloadLength()) << std::endl;
					if (pack->HasMarker())
						std::cout << "maker" << std::endl;
						fwrite(pack->GetPayloadData(), 1, pack->GetPayloadLength(), fpH264);*/
					
					// we don't longer need the packet, so
					// we'll delete it
					sess.DeletePacket(pack);
					
				}

			} while (sess.GotoNextSourceWithData());
		}

		sess.EndDataAccess();

#ifndef RTP_SUPPORT_THREAD
		status = sess.Poll();
		checkerror(status);
#endif // RTP_SUPPORT_THREAD

		//jrtplib::RTPTime::Wait(jrtplib::RTPTime(10, 0));
	}
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK

}


int main() {
	CameraParam tmp;
	tmp.played = 1;
	tmp.PlayPort = 6000;
	tmp.alive = 1;

	jrtplib_rtp_recv_thread(&tmp);
	return 0;
}
