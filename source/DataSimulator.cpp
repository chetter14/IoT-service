#include "MyTcpHandler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <string>

using namespace iot_service;

// Locally defined functions and variables
namespace {
	constexpr int MIN_TEMP = 20, MAX_TEMP = 26;
	
	void InitMessagingWithController(AMQP::TcpChannel& channel) {
		// Declare a queue and exchange
		channel.declareQueue(mqbroker::DataSimulatorQueue);
		channel.declareExchange(mqbroker::Exchange, AMQP::direct);
		
		// Bind the DataSimulator queue to the exchange with the routing key
		channel.bindQueue(mqbroker::Exchange, mqbroker::DataSimulatorQueue, mqbroker::DSQueueRoutingKey);
	}
}

int main(int argc, char* argv[]) {
    // Initialize the handler, connection, and channel	
    MyTcpHandler handler;
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@rabbitmq/"));
    AMQP::TcpChannel channel(&connection);
	
	// Initialize components for messaging with IoT controller
    InitMessagingWithController(channel);
	
	// Initialization of generator of random temperature values
	std::random_device dev;
	std::mt19937 gen(dev());
	std::uniform_int_distribution<std::mt19937::result_type> temp_gen(MIN_TEMP, MAX_TEMP);

	while (true) {
		int temperature = temp_gen(gen);
		channel.publish(mqbroker::Exchange, mqbroker::DSQueueRoutingKey, std::to_string(temperature));
		
		handler.processEvents(&connection);
				
		// To send 10 messages per second
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(50ms);
	}

	std::cout << "The DataSimulator is to be closed!" << std::endl;
    return 0;
}
