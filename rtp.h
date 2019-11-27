#pragma once
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include "libavutil/error.h"
};

#include "demo.h"
#include "PTZ.h"

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

struct pack_start_code
{
	unsigned char start_code[3];
	unsigned char stream_id[1];
};

struct program_stream_pack_header
{
	pack_start_code PackStart;// 4
	unsigned char Buf[9];
	unsigned char stuffinglen;
};

struct program_stream_pack_bb_header
{
	unsigned char head[4];
	unsigned char num1;
	unsigned char num2;
};

struct Frame {
	int size;
	char* data;
};

union littel_endian_size
{
	unsigned short int  length;
	unsigned char       byte[2];
};

struct program_stream_e
{
	pack_start_code     PackStart;
	littel_endian_size  PackLength;//we mast do exchange
	char                PackInfo1[2];
	unsigned char       stuffing_length;
};

struct program_stream_map
{
	pack_start_code PackStart;
	littel_endian_size PackLength;//we mast do exchange
	//program_stream_info_length
	//info
	//elementary_stream_map_length
	//elem
};

unsigned long parse_time_stamp(const unsigned char* p);

inline int ProgramStreamPackHeader(char* Pack, int length, char **NextPack, int *leftlength)
{
    printf("%02x %02x %02x %02x\n",Pack[0],Pack[1],Pack[2],Pack[3]);
    //通过 00 00 01 ba头的第14个字节的最后3位来确定头部填充了多少字节
    program_stream_pack_header *PsHead = (program_stream_pack_header *)Pack;
    unsigned char pack_stuffing_length = PsHead->stuffinglen & '\x07';
 
    *leftlength = length - sizeof(program_stream_pack_header) - pack_stuffing_length;//减去头和填充的字节
    *NextPack = Pack+sizeof(program_stream_pack_header) + pack_stuffing_length;
 
    //如果开头含有bb 则去掉bb
    if(*NextPack && (*NextPack)[0]=='\x00' && (*NextPack)[1]=='\x00' && (*NextPack)[2]=='\x01' && (*NextPack)[3]=='\xBB')
    {
        program_stream_pack_bb_header *pbbHeader=(program_stream_pack_bb_header *)(*NextPack);
        unsigned char bbheaderlen=pbbHeader->num2;
        (*NextPack) = (*NextPack) + sizeof(program_stream_pack_bb_header)+bbheaderlen;
        *leftlength = length - sizeof(program_stream_pack_bb_header) - bbheaderlen;
        int a=0;
        a++;
    }
 
    if(*leftlength<4) return 0;
 
    //printf("[%s]2 %x %x %x %x\n", __FUNCTION__, (*NextPack)[0], (*NextPack)[1], (*NextPack)[2], (*NextPack)[3]);
 
    return *leftlength;
}

inline int Pes(char* Pack, int length, char** NextPack, int* leftlength, char** PayloadData, int* PayloadDataLen)
{
	printf("[%s]%x %x %x %x\n", __FUNCTION__, Pack[0], Pack[1], Pack[2], Pack[3]);
	program_stream_e* PSEPack = (program_stream_e*)Pack;

	*PayloadData = 0;
	*PayloadDataLen = 0;

	if (length < sizeof(program_stream_e)) return 0;

	littel_endian_size pse_length;
	printf("%x %x", PSEPack->PackLength.byte[0], PSEPack->PackLength.byte[1]);
	pse_length.byte[0] = PSEPack->PackLength.byte[1];
	pse_length.byte[1] = PSEPack->PackLength.byte[0];

	*PayloadDataLen = pse_length.length - 2 - 1 - PSEPack->stuffing_length;
	
	// first pes contains pts
	//int pts = parse_time_stamp((unsigned char*)Pack + 9);

	if (*PayloadDataLen > 0)
		* PayloadData = Pack + sizeof(program_stream_e) - 1 + PSEPack->stuffing_length;

	*leftlength = length - pse_length.length - sizeof(pack_start_code) - sizeof(littel_endian_size);

	//printf("[%s]leftlength %d\n", __FUNCTION__, *leftlength);

	if (*leftlength <= 0) return 0;

	*NextPack = Pack + sizeof(pack_start_code) + sizeof(littel_endian_size) + pse_length.length;

	return *leftlength;
}

inline int ProgramStreamMap(char* Pack, int length, char** NextPack, int* leftlength, char** PayloadData, int* PayloadDataLen)
{
	printf("[%s]%x %x %x %x\n", __FUNCTION__, Pack[0], Pack[1], Pack[2], Pack[3]);

	program_stream_map* PSMPack = (program_stream_map*)Pack;

	//no payload
	*PayloadData = 0;
	*PayloadDataLen = 0;

	if (length < sizeof(program_stream_map)) return 0;

	littel_endian_size psm_length;
	psm_length.byte[0] = PSMPack->PackLength.byte[1];
	psm_length.byte[1] = PSMPack->PackLength.byte[0];

	*leftlength = length - psm_length.length - sizeof(program_stream_map);

	//printf("[%s]leftlength %d\n", __FUNCTION__, *leftlength);

	if (*leftlength <= 0) return 0;

	*NextPack = Pack + psm_length.length + sizeof(program_stream_map);

	return *leftlength;
}

inline int GetH246FromPs(char* buffer,int length, char **h264Buffer, int *h264length, int& pts, int& flag)
{
    int leftlength = 0;
    char *NextPack = 0;
 
    *h264Buffer = buffer;
    *h264length = 0;
 
    if(ProgramStreamPackHeader(buffer, length, &NextPack, &leftlength)==0)
        return 0;
 
    char *PayloadData = NULL;
    int PayloadDataLen=0;
 
	int i = 0;
    while(leftlength >= sizeof(pack_start_code))
    {
        PayloadData = NULL;
        PayloadDataLen=0;
 
        if(NextPack
            && NextPack[0]=='\x00'
            && NextPack[1]=='\x00'
            && NextPack[2]=='\x01'
            && NextPack[3]=='\xE0')
        {
            //接着就是流包，说明是非i帧
			if (i == 2) {
				// 测试好h264 码流中，第2个包是00 00 01 ba重复
				// 第3个是pes包，是i帧，取出时间戳
				pts = parse_time_stamp((unsigned char*)NextPack + 9);
			}
            if(Pes(NextPack, leftlength, &NextPack, &leftlength, &PayloadData, &PayloadDataLen))
            {
                if(PayloadDataLen)
                {
                    memcpy(buffer, PayloadData, PayloadDataLen);
                    buffer += PayloadDataLen;
                    *h264length += PayloadDataLen;
                }
            }
            else
            {
                if(PayloadDataLen)
                {
                    memcpy(buffer, PayloadData, PayloadDataLen);
                    buffer += PayloadDataLen;
                    *h264length += PayloadDataLen;
                }
                break;
            }
        }
        else if(NextPack
            && NextPack[0]=='\x00'
            && NextPack[1]=='\x00'
            && NextPack[2]=='\x01'
            && NextPack[3]=='\xBC')
        {
			// 第一个包是 00 00 01 BC i帧关键帧
			if (i == 0) flag = 1;
			else flag = 0;
            if(ProgramStreamMap(NextPack, leftlength, &NextPack, &leftlength, &PayloadData, &PayloadDataLen)==0)
                break;
        }
		else if (NextPack
			&& NextPack[0] == '\x00'
			&& NextPack[1] == '\x00'
			&& NextPack[2] == '\x01'
			&& NextPack[3] == '\xBA') {
			// 测试ps流中，可能存在00 00 01 BA重复，跳过 
			ProgramStreamPackHeader(NextPack, leftlength, &NextPack, &leftlength);
		}
        else
        {
			// 测试码流中最后14位无法识别
            //printf("[%s]no konw %02x %02x %02x %02x\n", __FUNCTION__, NextPack[0], NextPack[1], NextPack[2], NextPack[3]);
            break;
        }
		i++;
    }
    return *h264length;
}

void jrtplib_rtp_recv_thread(CameraParam* camerapar);
