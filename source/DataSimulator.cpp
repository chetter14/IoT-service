#include "MyTcpHandler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <string>


int main(int argc, char* argv[]) {
    // Initialize the handler
    MyTcpHandler handler;
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@rabbitmq/"));
    AMQP::TcpChannel channel(&connection);

    // Declare a queue and exchange
    channel.declareQueue(mqbroker::DataSimulatorQueue);
	channel.declareExchange(mqbroker::Exchange, AMQP::direct);
	
	// bind the queue to the exchange
	channel.bindQueue(mqbroker::Exchange, mqbroker::DataSimulatorQueue, mqbroker::DSQueueRoutingKey);

	std::random_device dev;
	std::mt19937 gen(dev());
	constexpr int MIN_TEMP = 20, MAX_TEMP = 26;
	std::uniform_int_distribution<std::mt19937::result_type> temp_gen(MIN_TEMP, MAX_TEMP);

	while (true) {
		int temperature = temp_gen(gen);
		channel.publish(mqbroker::Exchange, mqbroker::DSQueueRoutingKey, std::to_string(temperature));
		
		handler.processEvents(&connection);
				
		// Rest for a little 
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);
	}

	std::cout << "The DataSimulator is to be closed!" << std::endl;

    return 0;
}
