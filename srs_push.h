#include "demo.h"
#include "rtp.h"
#include "include/srs_librtmp/srs_librtmp.h"

enum RTMP_STATUS {
  RS_STM_Init,       // 初始化状态
  RS_STM_Handshaked, // 与服务器协商过程中
  RS_STM_Connected,  // 与服务器连接成功
  RS_STM_Published,  // 开始推流
  RS_STM_Closed      // 推流关闭
};

enum ENC_DATA_TYPE { VIDEO_DATA, AUDIO_DATA, META_DATA };

typedef struct EncData {
EncData(void) : _data(NULL), _dataLen(0), _bVideo(false), _dts(0), flag(false) {}
  uint8_t *_data;
  int _dataLen;
  bool _bVideo;
  uint32_t _dts;
  bool flag;
  ENC_DATA_TYPE _type;
} EncData;

enum PUSH_TYPE { FFMPEG, SRS };

class CameraPush {
public:
  explicit CameraPush();
  explicit CameraPush(const CameraPush& camp);
  CameraPush(CameraParam &campar);
  ~CameraPush();

public:
  void SetParam(CameraParam& campar);
  CameraParam* GetParam();
  //unused
  void SetFault(PUSH_TYPE push_type_);
  void Run();
  void OnH264Data();
  void SetPushType(PUSH_TYPE push_type);


private:
  void push_stream_ffmpeg();
  void push_stream_srs();
  void GotH264Nal(uint8_t *pData, int nLen, uint32_t ts, bool flag);
  void srs_send_data();

  AVFormatContext *Init_ofmt_ctx();

  CameraParam* campar_;
  PUSH_TYPE push_type_;
  bool need_keyframe_;
  void *rtmp_;
  RTMP_STATUS rtmp_status_;
  std::list<EncData *> lst_enc_data_;
  std::mutex mutex_;
};

inline int read_h264_frame(char *data, int size, char **pp, int *pnb_start_code,
                           int fps, char **frame, int *frame_size) {
  char *p = *pp;

  // @remark, for this demo, to publish h264 raw file to SRS,
  // we search the h264 frame from the buffer which cached the h264 data.
  // please get h264 raw data from device, it always a encoded frame.
  if (!srs_h264_startswith_annexb(p, size - (p - data), pnb_start_code)) {
    srs_human_trace("h264 raw data invalid.");
    return -1;
  }

  // @see srs_write_h264_raw_frames
  // each frame prefixed h.264 annexb header, by N[00] 00 00 01, where N>=0,
  // for instance, frame = header(00 00 00 01) + payload(67 42 80 29 95 A0 14 01
  // 6E 40)
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

  return 0;
}
