#pragma once
#include "spdlog/spdlog.h"
// #include "spdlog/common.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

class SpdLogw
{
public:
	static int InitLogger();
};