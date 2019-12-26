#define _CRT_SECURE_NO_WARNINGS
#include "rtp.h"
#include "include/srs_librtmp/srs_librtmp.h"

unsigned long parse_time_stamp(const unsigned char *p) {
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

void checkerror(int rtperr) {
  if (rtperr < 0) {
    std::cout << "ERROR: " << jrtplib::RTPGetErrorString(rtperr) << std::endl;
    exit(-1);
  }
}

int read_data(void *opaque, uint8_t *buf, int buf_size) {
  memcpy(buf, static_cast<Frame *>(opaque)->data,
         static_cast<Frame *>(opaque)->size);
  buf_size = static_cast<Frame *>(opaque)->size;
  // int tmp = buf_size;
  return buf_size;
}

AVFormatContext *Init_ofmt_ctx(CameraParam *camerapar) {
  int ret;
  AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
  AVOutputFormat *ofmt = NULL;
  // const char* out_filename = camerapar->StreamIp.c_str();
  const char *out_filename = "rtmp://192.168.44.91/live/home";

  const char *in_filename = "1.h264";

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

  AVStream *in_stream = ifmt_ctx->streams[0];
  AVCodecParameters *in_codecpar = in_stream->codecpar;

  AVPacket packet;
  av_read_frame(ifmt_ctx, &packet);

  AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
  if (!out_stream) {
    printf("Failed allocating output stream\n");
    ret = AVERROR_UNKNOWN;
  }
  ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
  av_dump_format(ofmt_ctx, 0, out_filename, 1);

  // out_stream->time_base = in_stream->time_base;

  if (!(ofmt->flags & AVFMT_NOFILE)) {
    ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      char av_error[AV_ERROR_MAX_STRING_SIZE] = {0};
      av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, ret);
      printf("Could not open output URL '%s', %s", out_filename, av_error);
    }
  }
  return ofmt_ctx;
}

int read_h264_frame(char* data, int size, char** pp, int* pnb_start_code, int fps,
    char** frame, int* frame_size, int* dts, int* pts)
{
    char* p = *pp;

    // @remark, for this demo, to publish h264 raw file to SRS,
    // we search the h264 frame from the buffer which cached the h264 data.
    // please get h264 raw data from device, it always a encoded frame.
    if (!srs_h264_startswith_annexb(p, size - (p - data), pnb_start_code)) {
        srs_human_trace("h264 raw data invalid.");
        return -1;
    }

    // @see srs_write_h264_raw_frames
    // each frame prefixed h.264 annexb header, by N[00] 00 00 01, where N>=0, 
    // for instance, frame = header(00 00 00 01) + payload(67 42 80 29 95 A0 14 01 6E 40)
    *frame = p;
    p += *pnb_start_code;

    for (; p < data + size; p++) {
        if (srs_h264_startswith_annexb(p, size - (p - data), NULL)) {
            break;
        }
    }

    *pp = p;
    *frame_size = p - *frame;
    if (*frame_size <= 0) {
        srs_human_trace("h264 raw data invalid.");
        return -1;
    }

    // @remark, please get the dts and pts from device,
    // we assume there is no B frame, and the fps can guess the fps and dts,
    // while the dts and pts must read from encode lib or device.
    *dts += 1000 / fps;
    *pts = *dts;

    return 0;
}

void jrtplib_rtp_recv_thread(CameraParam *camerapar) {
  int ret;
  // AVFormatContext *ofmt_ctx = Init_ofmt_ctx(camerapar);

  /* 计算 duration 和 framerate，暂时指定framerate为25帧，duration为40
  AVRational frame_rate;
  double duration;
  if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
          frame_rate = av_guess_frame_rate(ifmt_ctx, in_stream, NULL);
          duration = ((frame_rate.num && frame_rate.den) ? av_q2d(AVRational{
  frame_rate.den, frame_rate.num }) : 0);
          // auto pts = av_rescale_q(duration, AVRational{ 1000, 1 },
  in_stream->time_base);
          // duration = duration / av_q2d(in_stream->time_base);
          duration = duration / av_q2d(AVRational{ 1, 1000 });
  }
  */

  // ret = avformat_write_header(ofmt_ctx, NULL);

  
  const char* rtmp_url = "rtmp://192.168.44.91/live/home";

  srs_rtmp_t rtmp = srs_rtmp_create(rtmp_url);

  if (srs_rtmp_handshake(rtmp) != 0) {
      srs_human_trace("simple handshake failed.");
  }
  srs_human_trace("simple handshake success");

  if (srs_rtmp_connect_app(rtmp) != 0) {
      srs_human_trace("connect vhost/app failed.");
  }
  srs_human_trace("connect vhost/app success");

  if (srs_rtmp_publish_stream(rtmp) != 0) {
      srs_human_trace("publish stream failed.");
  }
  srs_human_trace("publish stream success");
   

  /*if (ret < 0) {
    printf("Error occurred when opening output URL\n");
  }*/

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

  char *frame = new char[1024 * 100];
  memset(frame, 0, sizeof(frame));
  char *returnps = new char[1024 * 100];
  memset(returnps, 0, sizeof(returnps));

  i = 0;
  // 关键帧标志位，开始接收到i帧置为1
  int firstflag = 0;

  int64_t start_time = av_gettime();
  while (camerapar->played && camerapar->alive) {
    sess.BeginDataAccess();
    // check incoming packets
    if (sess.GotoFirstSourceWithData()) {
      do {
        jrtplib::RTPPacket *pack;
        while ((pack = sess.GetNextPacket()) != NULL) {
          // You can examine the data here
          // printf("Got packet!\n");
          if (pack->GetPayloadType() == 96) {
            // maker ps tail
            if (pack->HasMarker()) {
              // caculate frame len
              // add ps tail data
              int frame_len =
                  std::accumulate(
                      FrameVector.begin(), FrameVector.end(), 0,
                      [](int sum, Frame a) { return sum + a.size; }) +
                  pack->GetPayloadLength();
              std::cout << frame_len << std::endl;

              int size = 0;

              for (auto i : FrameVector) {
                memcpy(frame + size, i.data, i.size);
                size += i.size;
              }

              memcpy(frame + size, pack->GetPayloadData(),
                     pack->GetPayloadLength());

              int iPsLength;
              int pts_;
              int flag;

              GetH246FromPs(frame, frame_len, &returnps, &iPsLength, pts_, flag);
              printf("[%s]%x %x %x %x\n", __FUNCTION__, returnps[0], returnps[1], returnps[2], returnps[3]);

              // TODO: read_h264_frame 从数组中读取
              // 顺序读取出sps pps 和 i,p 数据
              // srs_h264_write_raw_frames 推流
              // 从srs_push.cpp 中写
              
              int pts = 0;
              int dts = 0;
              
              int all_len = 0;

              int sps_flag = 0;
              if (returnps[3] == '\x67' || returnps[3] == '\x61')
              {
                  char* p = returnps;
                  for (; p < returnps + iPsLength;)
                  {
                      char* data = NULL;
                      int size = 0;
                      int nb_start_code = 0;
                      read_h264_frame(returnps, (int)iPsLength, &p, &nb_start_code, 25, &data, &size, &dts, &pts);
                      pts = pts_ / 90;
                      dts = pts;
                      u_int8_t nut = (char)data[nb_start_code] & 0x1f;
                      if (nut == 7 || nut == 8 || nut == 1 || nut == 5) {
                          if (nut == 5) flag = 1;
                          if (nut == 7)
                          {
                              firstflag = 1;
                              sps_flag = 1;
                          }
                          if (sps_flag && nut == 1) {
                              dts = dts + 40;
                              pts = pts + 40;
                          }
                          all_len += size;
                          LOG(INFO) << "type :" << (nut == 7 ? "SPS" : (nut == 8 ? "PPS" : (nut == 5 ? "I" : (nut == 1 ? "P" : (nut == 9 ? "AUD" : (nut == 6 ? "SEI" : "Unknown")))))) <<
                            " time :" << pts;
                          srs_human_trace("sent packet: type=%s, time=%d, size=%d, fps=%.2f, b[%d]=%#x(%s)",
							  srs_human_flv_tag_type2string(SRS_RTMP_TYPE_VIDEO), pts, size, 25, nb_start_code, (char)data[nb_start_code],
							  (nut == 7 ? "SPS" : (nut == 8 ? "PPS" : (nut == 5 ? "I" : (nut == 1 ? "P" : (nut == 9 ? "AUD" : (nut == 6 ? "SEI" : "Unknown")))))));
                          if(firstflag)
                              ret = srs_h264_write_raw_frames(rtmp, data, size, dts, pts);
                      }
                      else
                          continue;
                      if (ret != 0) {
                          if (srs_h264_is_dvbsp_error(ret)) {
                              srs_human_trace("ignore drop video error, code=%d", ret);
                          }
                          else if (srs_h264_is_duplicated_sps_error(ret)) {
                              srs_human_trace("ignore duplicated sps, code=%d", ret);
                          }
                          else if (srs_h264_is_duplicated_pps_error(ret)) {
                              srs_human_trace("ignore duplicated pps, code=%d", ret);
                          }
                          else {
                              srs_human_trace("send h264 raw data failed. ret=%d", ret);
                              LOG(ERROR) << "send h264 raw data failed. ret=" << ret;
                          }
                      }
                  }
              }
              else {
                  printf("error!\n");
              }
              
              AVPacket packet;
              packet.buf = nullptr;
              packet.data = reinterpret_cast<uint8_t*>(returnps);
              packet.size = all_len;
              // out_stream timebase default is {1, 1000}
              packet.dts = pts_ * av_q2d(AVRational{ 1, 90000 }) /
                  av_q2d(AVRational{ 1, 1000 });
              packet.pts = packet.dts;
              packet.flags = flag;
              packet.stream_index = 0;
              packet.duration = 40; // 3600/90;
              packet.pos = -1;
              packet.side_data = nullptr;
              packet.side_data_elems = 0;
              
              /*
              int64_t now_time = av_gettime() - start_time;
              LOG(INFO) << now_time;
              if (packet.pts > now_time) {
                av_usleep(packet.pts - now_time);
              }
              */

              if(flag)
                firstflag = 1;
              /*
              if (firstflag)
                ret = av_interleaved_write_frame(ofmt_ctx, &packet);
              */

              /*
              AVFormatContext* ic = nullptr;
              AVIOContext* pb = NULL;
              AVInputFormat* piFmt = NULL;
              ic = avformat_alloc_context();
              int BUF_SIZE = 4096*20;
              Frame tmpbuffer{ iPsLength, h264buffer };
              uint8_t* buf = (uint8_t*)av_mallocz(sizeof(uint8_t) * BUF_SIZE);
              pb = avio_alloc_context(buf, BUF_SIZE, 0, &tmpbuffer, read_data,
              NULL, NULL); ic->pb = pb;

              if (av_probe_input_buffer(pb, &piFmt, "", NULL, 0, 0) < 0)
              {
                      fprintf(stderr, "probe format failed\n");
              }
              ret = avformat_open_input(&ic, "", piFmt, NULL);

              AVPacket packet;
              av_read_frame(ic, &packet);
              */
              // 首帧是i帧开始推流

              for (int i = 0; i < FrameVector.size(); i++) {
                if (FrameVector[i].data != nullptr) {
                  delete[] FrameVector[i].data;
                  FrameVector[i].data = nullptr;
                }
              }
              FrameVector.clear();
            } else {
              // audio
              /*
              if (pack->GetPacketData()[12] == 0x00 && pack->GetPacketData()[13]
              == 0x00 && pack->GetPacketData()[14] == 0x01 &&
              pack->GetPacketData()[15] == 0xc0)
              {
                              }
                              else if (pack->GetPacketData()[12] == 0x00 &&
              pack->GetPacketData()[13] == 0x00 && pack->GetPacketData()[14] ==
              0x01 && pack->GetPacketData()[15] == 0xbd)
                              {
              }
              else {
              */
              char *tmp = new char[pack->GetPayloadLength()];
              memcpy(tmp, pack->GetPayloadData(), pack->GetPayloadLength());
              FrameVector.push_back(
                  Frame{static_cast<int>(pack->GetPayloadLength()), tmp});
              //}
            }
          } else {
            std::cout << "Pack Payload is:" << pack->GetPayloadType()
                      << std::endl;
          }
          //写入文件
          std::cout << int(pack->GetPayloadLength()) << std::endl;
          if (pack->HasMarker())
            std::cout << "maker" << std::endl;
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
       // jrtplib::RTPTime::Wait(jrtplib::RTPTime(10, 0));
  }
  delete[] frame;
  delete[] returnps;
#ifdef RTP_SOCKETTYPE_WINSOCK
  WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
}
