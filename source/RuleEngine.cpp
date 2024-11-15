#include "MyTcpHandler.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <string>
#include <cmath>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>

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

    // Declare the queue and consume messages from it
    channel.declareQueue(mqbroker::RuleEngineQueue);
    channel.consume(mqbroker::RuleEngineQueue).onReceived([&](const AMQP::Message &message,
                                              uint64_t deliveryTag,
                                              bool redelivered) {
		// Extract message body (temperature value)
		std::string received_message(message.body(), message.bodySize());
		int cur_temp = std::stoi(received_message);
		
		// Get rule message according to temperature values
		std::string rule_message = GetRuleMessage(prev_last_temp, last_temp, cur_temp);
		
		// Get the current time (from date to milliseconds)
		std::string current_time = GetCurrentTime();
		
		// Insert the current time point and rule message into collection (if any rule is met)
		if (rule_message.size() != 0) {
			temp_rules_collection.insert_one(bsoncxx::builder::basic::make_document(
				bsoncxx::builder::basic::kvp(current_time, rule_message)
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
