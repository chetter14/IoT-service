#include "MyTcpHandler.hpp"
#include <iostream>
#include <thread>
#include <chrono>


int main(int argc, char* argv[]) {
    // Initialize the handler and event loop
    MyTcpHandler handler;
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@rabbitmq/"));
    AMQP::TcpChannel channel(&connection);

    // Declare a queue and exchange
    channel.declareQueue("my_queue");
	channel.declareExchange("my_exchange", AMQP::direct);
	
	// bind the queue to the exchange using routing key
	channel.bindQueue("my_exchange", "my_queue", "my_routing_key");
	
    channel.publish("my_exchange", "my_routing_key", "Hello from DataSimulator!");
	
	// send temperature values (50 times per second)

	while (true) {
		handler.processEvents(&connection);
		
		// Rest for some time
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(5000ms);
	}

	std::cout << "The DataSimulator is to be closed!" << std::endl;

    return 0;
}
