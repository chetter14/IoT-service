#include <spdlog/spdlog.h>
#include <spdlog/sinks/tcp_sink.h> 	// TCP sink for Logstash
#include <memory>
#include <string>
#include <mutex>

class Logger {
private:
	static std::shared_ptr<spdlog::logger> logger_;
	
public: 
	static void Initialize(const std::string& address, int port);
	static void Info(const std::string& message);
	static void Warn(const std::string& message);
	static void Error(const std::string& message);
};
