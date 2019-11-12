#pragma once
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


inline int Pes(char* Pack, int length, char** NextPack, int* leftlength, char** PayloadData, int* PayloadDataLen)
{
	printf("[%s]%x %x %x %x\n", __FUNCTION__, Pack[0], Pack[1], Pack[2], Pack[3]);
	program_stream_e* PSEPack = (program_stream_e*)Pack;

	*PayloadData = 0;
	*PayloadDataLen = 0;

	if (length < sizeof(program_stream_e)) return 0;

	littel_endian_size pse_length;
	pse_length.byte[0] = PSEPack->PackLength.byte[1];
	pse_length.byte[1] = PSEPack->PackLength.byte[0];

	*PayloadDataLen = pse_length.length - 2 - 1 - PSEPack->stuffing_length;
	int pts = parse_time_stamp((unsigned char*)Pack + 9);

	if (*PayloadDataLen > 0)
		* PayloadData = Pack + sizeof(program_stream_e) + PSEPack->stuffing_length;

	*leftlength = length - pse_length.length - sizeof(pack_start_code) - sizeof(littel_endian_size);

	//printf("[%s]leftlength %d\n", __FUNCTION__, *leftlength);

	if (*leftlength <= 0) return 0;

	*NextPack = Pack + sizeof(pack_start_code) + sizeof(littel_endian_size) + pse_length.length;

	return *leftlength;
}

inline int ProgramStreamMap(char* Pack, int length, char** NextPack, int* leftlength, char** PayloadData, int* PayloadDataLen)
{
	//printf("[%s]%x %x %x %x\n", __FUNCTION__, Pack[0], Pack[1], Pack[2], Pack[3]);

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

