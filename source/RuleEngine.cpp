#include "MyTcpHandler.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <string>
#include <cmath>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "Prometheus.hpp"
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <sstream>
#include <iomanip>
#include <ctime>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/tcp_sink.h> // TCP sink for Logstash

using namespace iot_service;

// Locally defined functions and variables
namespace {	
	mongocxx::v_noabi::collection GetTempRulesCollection(mongocxx::client& client) {
		// Initialize db and collection (or just access them)
		auto iot_db = client[DatabaseName];
		auto temp_rules_collection = iot_db[mqbroker::RuleEngineQueue];
		// Clear the content of collection
		temp_rules_collection.delete_many({});
		
		return temp_rules_collection;
	}
	
	std::string GetRuleMessage(int prev_last_temp, int last_temp, int cur_temp) {
		// Define temperature values for business logic
		constexpr int HUGE_DIFF = 3;
		constexpr int BOTTOM_TEMP = 22;
		constexpr int TOP_TEMP = 24;
		
		// Handle possible cases
		std::stringstream rule_message;
		if (std::abs(cur_temp - last_temp) > HUGE_DIFF) {
			rule_message << "Sharp change in temperature measurements! From " << last_temp 
				<< " to " << cur_temp;
		} else if (prev_last_temp < BOTTOM_TEMP && last_temp < BOTTOM_TEMP && cur_temp < BOTTOM_TEMP) {
			rule_message << "Room temperature has decreased!";
		} else if (prev_last_temp > TOP_TEMP && last_temp > TOP_TEMP && cur_temp > TOP_TEMP) {
			rule_message << "Room temperature has increased!";
		}
		return rule_message.str();
	}
}

int main() {
		// Initialize Logger
	// Logger::Initialize("logstash", 5000);
	
	// std::cout << "Sleeping..." << std::endl;
	
	// using namespace std::chrono_literals;
	// std::this_thread::sleep_for(50s);
	
	std::cout << "Launched main()" << std::endl;
	
	// Configure the TCP sink for Logstash
    spdlog::sinks::tcp_sink_config cfg("logstash", 5044);
	// cfg.username = "logstash_user";
    // cfg.password = "root";
	
	std::cout << "Created config" << std::endl;
	
	auto tcp_sink = std::make_shared<spdlog::sinks::tcp_sink_mt>(cfg);
	
	std::cout << "Created tcp sink" << std::endl;
	
	auto logger = std::make_shared<spdlog::logger>("tcp_logger", tcp_sink);
	
	std::cout << "Created logger" << std::endl;
	
	spdlog::register_logger(logger);
	
	std::cout << "Registered logger" << std::endl;

	logger->info(R"({"service":"Rule Engine", "level":"info", "message":"Service started"})");
	
	std::cout << "Logged info" << std::endl;
	
	
    // Initialize handler, connection, and channel	
    MyTcpHandler handler;
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@rabbitmq/"));
    AMQP::TcpChannel channel(&connection);

	// 23 (in Celsius degrees) - default room temperature 
	int prev_last_temp = 23;
	int last_temp = 23;
	
	// Initialize MongoDB driver instance
	mongocxx::instance instance{};
	// Get client connected to MongoDB
	mongocxx::client client{mongocxx::uri{"mongodb://root:example@mongodb:27017"}};
	// Get collection of temperature rules
	auto temp_rules_collection = GetTempRulesCollection(client);

	// Initialize Prometheus:
	
	using namespace prometheus;
	MetricsManager manager("0.0.0.0:8081");
	// Get a registry for collecting metrics
    auto registry = manager.GetRegistry();
	// Create a counter and register it
    auto& counter_family = BuildCounter()
                           .Name("rule_engine_counter")
                           .Help("Rule engine counter")
                           .Register(*registry);
	auto& message_counter = counter_family.Add({{"message_counter", "value"}});

    // Declare the queue and consume messages from it
    channel.declareQueue(mqbroker::RuleEngineQueue);
    channel.consume(mqbroker::RuleEngineQueue).onReceived([&](const AMQP::Message &message,
                                              uint64_t deliveryTag,
                                              bool redelivered) {
		// Extract message body (temperature value)
		std::string received_message(message.body(), message.bodySize());
		int cur_temp = std::stoi(received_message);
		
		// Update messages counter value
		message_counter.Increment();
		
		// Get rule message according to temperature values
		std::string rule_message = GetRuleMessage(prev_last_temp, last_temp, cur_temp);
		
		// Insert the current time point and rule message into collection (if any rule is met)
		if (rule_message.size() != 0) {
			// Get the current time
			auto now = std::chrono::system_clock::now();
			auto bson_date = bsoncxx::types::b_date{ now };		// Convert to BSON format
			
			temp_rules_collection.insert_one(bsoncxx::builder::basic::make_document(
				bsoncxx::builder::basic::kvp("Rule message", rule_message),
				bsoncxx::builder::basic::kvp("Time", bson_date)
			));
		}
		// Update the last and previous of the last values
		prev_last_temp = last_temp;
		last_temp = cur_temp;
    });

	while (true) {
		handler.processEvents(&connection);
	}

	std::cout << "RuleEngine is to be closed!" << std::endl;
    return 0;
}
