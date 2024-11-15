#include "MyTcpHandler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>

using namespace iot_service;

// Locally defined functions
namespace {	
	mongocxx::v_noabi::collection GetTempValuesCollection(mongocxx::client& client) {
		// Initialize db and collection (or just access them)
		auto iot_db = client[DatabaseName];
		auto temp_values_collection = iot_db[mqbroker::DataSimulatorQueue];
		// Clear the content of collection (to return empty one)
		temp_values_collection.delete_many({});
		
		return temp_values_collection;
	}
	
	void InitMessagingWithRuleEngine(AMQP::TcpChannel& channel) {
		// Declare the queue with RuleEngine to redirect temperature values to it
		channel.declareExchange(mqbroker::Exchange, AMQP::direct);
		channel.declareQueue(mqbroker::RuleEngineQueue);
		// Bind the RuleEngine queue to the exchange with the routing key
		channel.bindQueue(mqbroker::Exchange, mqbroker::RuleEngineQueue, mqbroker::REQueueRoutingKey);
	}
}

int main() {
    // Initialize the handler, connection, and channel
    MyTcpHandler handler;
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@rabbitmq/"));
    AMQP::TcpChannel channel(&connection);
		
	// Initialize MongoDB driver instance
	mongocxx::instance instance{};
	// Get client connected to MongoDB
	mongocxx::client client{mongocxx::uri{"mongodb://root:example@mongodb:27017"}};
	// Get collection of temperature values
	auto temp_values_collection = GetTempValuesCollection(client);

	// Initialize components for messaging with Rule Engine
	InitMessagingWithRuleEngine(channel);

    // Declare the queue with DataSimulator to consume messages from it
    channel.declareQueue(mqbroker::DataSimulatorQueue);	
	channel.consume(mqbroker::DataSimulatorQueue).onReceived([&](const AMQP::Message &message,
                                              uint64_t deliveryTag,
                                              bool redelivered) {
		// Extract the message body (temperature value)
		std::string received_message(message.body(), message.bodySize());
		int temperature = std::stoi(received_message);
		
		// Get the current time (from date to milliseconds)
		std::string current_time = GetCurrentTime();
		
		// Insert the current time point (date and stuff) and temperature value into collection
		temp_values_collection.insert_one(bsoncxx::builder::basic::make_document(
			bsoncxx::builder::basic::kvp(current_time, temperature)
		));
		
		channel.publish(mqbroker::Exchange, mqbroker::REQueueRoutingKey, received_message);
	});
	
	while (true) {
		handler.processEvents(&connection);
	}
	
	std::cout << "IoT controller is to be closed!" << std::endl;
    return 0;
}
