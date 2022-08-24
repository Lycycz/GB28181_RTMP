#include "spdlogw.h"

int SpdLogw::InitLogger()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("spdlog_test.log", true);
    file_sink->set_level(spdlog::level::warn);

    spdlog::sinks_init_list sink_list = { file_sink, console_sink };

    spdlog::set_default_logger(std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list({ console_sink, file_sink })));
    spdlog::flush_on(spdlog::level::info);

    return 0;
}
