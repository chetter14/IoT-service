// #include <spdlog/spdlog.h>
// #include <spdlog/sinks/tcp_sink.h> 	// TCP sink for Logstash
#include <memory>
#include <string>
#include <iostream>

// class Logger {
// public: 
	// static void Initialize(const std::string& address, int port) {
		// if (logger_) {
			// std::cout << "The logger is already instantiated!\n";
			// return;
		// }
		// Logger(address, port);
	// }
	
	// static void Info(const std::string& message) {
		// logger_->info(message);
	// }
	
	// static void Warn(const std::string& message) {
		// logger_->warn(message);
	// }
	
	// static void Error(const std::string& message) {
		// logger_->error(message);
	// }
	
// private:
	// Logger(const std::string& address, int port) { 
		// auto tcp_sink = std::make_shared<spdlog::sinks::tcp_sink_mt>(address, port);
		// logger_ = std::make_shared<spdlog::logger>("iot_service_logger", tcp_sink);
		
		// spdlog::register_logger(logger_);
	// }
	
	// static std::shared_ptr<spdlog::logger> logger_;
// };