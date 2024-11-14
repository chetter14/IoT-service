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

using namespace iot_service;

int main() {
    // Initialize the handler and event loop
    MyTcpHandler handler;
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@rabbitmq/"));
    AMQP::TcpChannel channel(&connection);
	
	// Initialize MongoDB driver instance
	mongocxx::instance instance{};
	// Connect to MongoDB
	mongocxx::client client{mongocxx::uri{"mongodb://root:example@mongodb:27017"}};
	
	auto db = client[DataBaseName];
	auto collection = db[mqbroker::DataSimulatorQueue];

    // Declare the queue with DataSimulator to consume messages from it
    channel.declareQueue(mqbroker::DataSimulatorQueue);	
	channel.consume(mqbroker::DataSimulatorQueue).onReceived([&](const AMQP::Message &message,
                                              uint64_t deliveryTag,
                                              bool redelivered) {
		std::string received_message(message.body(), message.bodySize());
		int temperature = std::stoi(received_message);
		std::cout << "Received temperature: " << temperature << std::endl;
		
		auto now = std::chrono::system_clock::now();
		std::time_t time_now = std::chrono::system_clock::to_time_t(now);
		std::stringstream ss;
		ss << std::put_time(std::localtime(&time_now), "%Y-%m-%d %H:%M:%S");
		
		collection.insert_one(bsoncxx::builder::basic::make_document(
			bsoncxx::builder::basic::kvp(ss.str(), temperature)
		));
		
		channel.publish(mqbroker::Exchange, mqbroker::REQueueRoutingKey, received_message);
	});
	
	// Declare the queue with RuleEngine to redirect temperature values to it
	channel.declareExchange(mqbroker::Exchange, AMQP::direct);
    channel.declareQueue(mqbroker::RuleEngineQueue);
	
	// bind the queue to the exchange
	channel.bindQueue(mqbroker::Exchange, mqbroker::RuleEngineQueue, mqbroker::REQueueRoutingKey);

	while (true) {
		handler.processEvents(&connection);
				
		// Rest for a second
		// using namespace std::chrono_literals;
		// std::this_thread::sleep_for(100ms);
	}
	
	std::cout << "IoT controller is to be closed!" << std::endl;

    return 0;
}
