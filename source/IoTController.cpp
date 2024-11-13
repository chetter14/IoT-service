#include "MyTcpHandler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>


int main() {
    // Initialize the handler and event loop
    MyTcpHandler handler;
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@rabbitmq/"));
    AMQP::TcpChannel channel(&connection);

    // Declare the queue and consume messages from it
    channel.declareQueue("my_queue");
    channel.consume("my_queue").onReceived([](const AMQP::Message &message,
                                              uint64_t deliveryTag,
                                              bool redelivered) {
		std::string receivedMessage(message.body(), message.bodySize());							
        std::cout << "Received message: " << receivedMessage << std::endl;
    });

	while (true) {
		handler.processEvents(&connection);
		
		// Rest for some time
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(5000ms);
	}
	
	std::cout << "IoT controller is to be closed!" << std::endl;

    return 0;
}
