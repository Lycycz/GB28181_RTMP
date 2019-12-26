#include "srs_push.h"

#define MAX_RETRY_TIME 3
CameraPush::CameraPush()
    : need_keyframe_(true), rtmp_(nullptr), rtmp_status_(RS_STM_Init),
      campar_(nullptr), push_type_(SRS), retrys_(0) {}

CameraPush::CameraPush(CameraParam &campar)
    : need_keyframe_(true), rtmp_(nullptr), rtmp_status_(RS_STM_Init),
      push_type_(SRS), retrys_(0) {
  campar_ = &campar;
}

CameraPush::CameraPush(const CameraPush &camp)
    : need_keyframe_(camp.need_keyframe_), rtmp_(camp.rtmp_),
      rtmp_status_(camp.rtmp_status_), push_type_(camp.push_type_), retrys_(0) {
  campar_ = camp.campar_;
}

CameraPush::~CameraPush() {
  rtmp_status_ = RS_STM_Closed;
  if (rtmp_) {
    srs_rtmp_disconnect_server(rtmp_);
  }
  if (rtmp_) {
    srs_rtmp_destroy(rtmp_);
    rtmp_ = nullptr;
  }
}

CameraParam *CameraPush::GetParam() { return campar_; }

void CameraPush::SetFault(PUSH_TYPE push_type) {
  need_keyframe_ = true;
  rtmp_ = nullptr;
  rtmp_status_ = RS_STM_Init;
  SetPushType(push_type);
}

void CameraPush::SetParam(CameraParam &campar) { campar_ = &campar; }

void CameraPush::Run() {
  std::thread t1 = std::thread(std::bind(&CameraPush::OnH264Data, this));
  std::thread t2;
  switch (push_type_) {
  case FFMPEG: {
    t2 = std::thread(std::bind(&CameraPush::push_stream_ffmpeg, this));
    // push_stream_ffmpeg();
  } break;
  case SRS: {
    t2 = std::thread(std::bind(&CameraPush::push_stream_srs, this));
    // push_stream_srs();
  } break;
  }
  t1.join();
  t2.join();
}

void CameraPush::push_stream_srs() {
  // rtmp_ = srs_rtmp_create(campar_->StreamIp.c_str());
  rtmp_ = srs_rtmp_create("rtmp://192.168.44.91/live/home");
  while (campar_->played && campar_->alive) {
    if (rtmp_ != nullptr) {
      switch (rtmp_status_) {
      case RS_STM_Init: {
        if (srs_rtmp_handshake(rtmp_) == 0) {
          srs_human_trace("SRS: simple handshake ok.");
          rtmp_status_ = RS_STM_Handshaked;
        } else {
          CallDisconnect();
        }
      } break;
      case RS_STM_Handshaked: {
        if (srs_rtmp_connect_app(rtmp_) == 0) {
          srs_human_trace("SRS: connect vhost/app ok.");
          rtmp_status_ = RS_STM_Connected;
        } else {
          CallDisconnect();
        }
      } break;
      case RS_STM_Connected: {
        if (srs_rtmp_publish_stream(rtmp_) == 0) {
          srs_human_trace("SRS: publish stream ok.");
          rtmp_status_ = RS_STM_Published;
          CallConnect();
        } else {
          CallDisconnect();
        }
      } break;
      case RS_STM_Published: {
        srs_send_data();
      } break;
      }
    }
  }
}

void CameraPush::push_stream_ffmpeg() {
  AVFormatContext *ofmt_ctx = Init_ofmt_ctx();

  while (true) {
    EncData *dataPtr = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (lst_enc_data_.size() > 0) {
        dataPtr = lst_enc_data_.front();
        lst_enc_data_.pop_front();
      }
    }

    if (dataPtr != NULL) {
      if (dataPtr->_type == VIDEO_DATA) {
        char *ptr = (char *)dataPtr->_data;
        int len = dataPtr->_dataLen;
        int ret = 0;

        AVPacket packet;

        packet.buf = nullptr;
        packet.data = reinterpret_cast<uint8_t *>(ptr);
        packet.size = len;
        // out_stream timebase default is {1, 1000}
        packet.dts = dataPtr->_dts;
        packet.pts = dataPtr->_dts;
        packet.flags = dataPtr->flag;
        packet.stream_index = 0;
        packet.duration = 40; // 3600/90;
        packet.pos = -1;
        packet.side_data = nullptr;
        packet.side_data_elems = 0;

        ret = av_interleaved_write_frame(ofmt_ctx, &packet);
      }
    }
  }
}

void CameraPush::SetPushType(PUSH_TYPE push_type) { push_type_ = push_type; }

AVFormatContext *CameraPush::Init_ofmt_ctx() {
  int ret;
  AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
  AVOutputFormat *ofmt = NULL;

  const char *out_filename = campar_->StreamIp.c_str();
  // const char *out_filename = "rtmp://192.168.44.91/live/home";

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

void CameraPush::GotH264Nal(uint8_t *pData, int nLen, uint32_t ts, bool flag) {
  EncData *pdata = new EncData();
  pdata->_data = new uint8_t[nLen];
  memcpy(pdata->_data, pData, nLen);
  pdata->_dataLen = nLen;
  pdata->_bVideo = true;
  pdata->_type = VIDEO_DATA;
  pdata->_dts = ts;
  pdata->flag = flag;
  std::unique_lock<std::mutex> lock(mutex_);
  lst_enc_data_.push_back(pdata);
  has_data_ = true;
  lock.unlock();
  cv_.notify_all();
}

void CameraPush::CallConnect() {
  need_keyframe_ = true;
  retrys_ = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::list<EncData *>::iterator iter = lst_enc_data_.begin();
    while (iter != lst_enc_data_.end()) {
      EncData *ptr = *iter;
      lst_enc_data_.erase(iter++);
      delete[] ptr->data;
      delete ptr;
    }
  }
}

void CameraPush::CallDisconnect() {
  need_keyframe_ = true;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (rtmp_) {
      srs_rtmp_destroy(rtmp_);
      rtmp_ = nullptr;
    }
    if (rtmp_status_ != RS_STM_CLOSED) {
      rtmp_status_ = RS_STM_INIT;
      retrys_++;
      if (retrys <= MAX_RETRY_TIME) {
        rtmp_ = srs_rtmp_create(campar_->StreamIp.c_str());
      } else {
        std::cout << "retry " << MAX_RETRY_TIME << " times connect error!"
                  << std::endl;
        // exit(1);
      }
    }
  }
}

void CameraPush::OnH264Data() {
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

  portbase = this->campar_->PlayPort;

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
  int64_t start_time = av_gettime();
  while (campar_->played == 1 && campar_->alive == 1) {
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
              int pts = 0;
              int dts = 0;
              int flag;

              // TODO:: flag 可能判断有误
              GetH246FromPs(frame, frame_len, &returnps, &iPsLength, pts, flag);

              /*
              pts = pts * av_q2d(AVRational{1, 90000}) /
                     av_q2d(AVRational{1, 1000});
              */
              pts = pts * av_q2d(AVRational{1, 90000}) /
                    av_q2d(AVRational{1, 1000});
              dts = pts;

              if (returnps[3] == '\x67' || returnps[3] == '\x61') {
                char *p = returnps;
                for (; p < returnps + iPsLength;) {
                  char *data = NULL;
                  int size = 0;
                  int nb_start_code = 0;
                  read_h264_frame(returnps, (int)iPsLength, &p, &nb_start_code,
                                  25, &data, &size);
                  u_int8_t nut = (char)data[nb_start_code] & 0x1f;
                  srs_human_trace(
                      "recieve packet: type=%s, time=%d, size=%d, fps=%.2f, "
                      "b[%d]=%#x(%s)",
                      srs_human_flv_tag_type2string(SRS_RTMP_TYPE_VIDEO), pts,
                      size, 25.0, nb_start_code, (char)data[nb_start_code],
                      (nut == 7
                           ? "SPS"
                           : (nut == 8
                                  ? "PPS"
                                  : (nut == 5
                                         ? "I"
                                         : (nut == 1
                                                ? "P"
                                                : (nut == 9
                                                       ? "AUD"
                                                       : (nut == 6 ? "SEI"
                                                                   : "Unknow"
                                                                     "n")))))));

                  // 首帧是i帧开始推送, sps pps i
                  if (nut == 7)
                    need_keyframe_ = false;
                  if (need_keyframe_)
                    return;
                  // pps sps I P
                  if (nut == 7 || nut == 8 || nut == 5 || nut == 1)
                    GotH264Nal(reinterpret_cast<uint8_t *>(data), size, pts,
                               flag);
                }
              }

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
              if (pack->GetPacketData()[12] == 0x00 &&
              pack->GetPacketData()[13]
              == 0x00 && pack->GetPacketData()[14] == 0x01 &&
              pack->GetPacketData()[15] == 0xc0)
              {
                              }
                              else if (pack->GetPacketData()[12] == 0x00 &&
              pack->GetPacketData()[13] == 0x00 && pack->GetPacketData()[14]
              == 0x01 && pack->GetPacketData()[15] == 0xbd)
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

void CameraPush::srs_send_data() {
  EncData *dataPtr = nullptr;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [=] { return has_data_; });
    // if (lst_enc_data_.size() > 0) {
    dataPtr = lst_enc_data_.front();
    lst_enc_data_.pop_front();
    //}
  }
  if (dataPtr != NULL) {
    if (dataPtr->_type == VIDEO_DATA) {
      char *ptr = (char *)dataPtr->_data;
      int len = dataPtr->_dataLen;
      int ret = 0;
      ret = srs_h264_write_raw_frames(rtmp_, ptr, len, dataPtr->_dts,
                                      dataPtr->_dts);
#ifdef _DEBUG
      LOG(INFO) << av_gettime();
#endif
      if (ret != 0) {
        if (srs_h264_is_dvbsp_error(ret)) {
          srs_human_trace("ignore drop video error, code=%d", ret);
        } else if (srs_h264_is_duplicated_sps_error(ret)) {
          srs_human_trace("ignore duplicated sps, code=%d", ret);
        } else if (srs_h264_is_duplicated_pps_error(ret)) {
          srs_human_trace("ignore duplicated pps, code=%d", ret);
        } else {
          srs_human_trace("send h264 raw data failed. ret=%d", ret);
          return;
        }
      }
    }
    delete[] dataPtr->_data;
    delete dataPtr;
  }
}
