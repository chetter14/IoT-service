#include <spdlog/spdlog.h>
#include <spdlog/sinks/tcp_sink.h> 	// TCP sink for Logstash
#include <memory>
#include <string>
#include <mutex>
#include <iostream>
#include "Logger.hpp"

// Static variables definitions
std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::Initialize(const std::string& address, int port) {
	if (Logger::logger_ != nullptr) {
		std::cout << "The logger is already instantiated!" << std::endl;
		return;
	} else {
		spdlog::sinks::tcp_sink_config cfg(address, port);
		auto tcp_sink = std::make_shared<spdlog::sinks::tcp_sink_mt>(cfg);
		
		Logger::logger_ = std::make_shared<spdlog::logger>("iot_service_logger", tcp_sink);
		spdlog::register_logger(Logger::logger_);
	}
}

void Logger::Info(const std::string& message) {
	if (Logger::logger_)
		Logger::logger_->info(message);
}

void Logger::Warn(const std::string& message) {
	if (Logger::logger_)
		Logger::logger_->warn(message);
}

void Logger::Error(const std::string& message) {
	if (Logger::logger_)
		Logger::logger_->error(message);
}

