#include "MyTcpHandler.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <string>
#include <cmath>

using namespace iot_service;

int main() {
    // Initialize everything required
    MyTcpHandler handler;
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@rabbitmq/"));
    AMQP::TcpChannel channel(&connection);

	// 23 (in Celsius degrees) - default room temperature 
	int prev_last_temp = 23;
	int last_temp = 23;

    // Declare the queue and consume messages from it
    channel.declareQueue(mqbroker::RuleEngineQueue);
    channel.consume(mqbroker::RuleEngineQueue).onReceived([&](const AMQP::Message &message,
                                              uint64_t deliveryTag,
                                              bool redelivered) {					
		std::string received_message(message.body(), message.bodySize());
		int cur_temp = std::stoi(received_message);
		
		constexpr int HUGE_DIFF = 3;
		constexpr int BOTTOM_TEMP = 22;
		constexpr int TOP_TEMP = 24;
		
		if (std::abs(cur_temp - last_temp) > HUGE_DIFF) {
			std::cout << "Sharp change in temperature measurements! From " << last_temp << " to " 
				<< cur_temp << std::endl;
		} else if (prev_last_temp < BOTTOM_TEMP && last_temp < BOTTOM_TEMP && cur_temp < BOTTOM_TEMP) {
			std::cout << "Room temperature has decreased!" << std::endl;
		} else if (prev_last_temp > TOP_TEMP && last_temp > TOP_TEMP && cur_temp > TOP_TEMP) {
			std::cout << "Room temperature has increased!" << std::endl;
		}
		prev_last_temp = last_temp;
		last_temp = cur_temp;
    });

	while (true) {
		handler.processEvents(&connection);
		
		// Rest for some time
		// using namespace std::chrono_literals;
		// std::this_thread::sleep_for(100ms);
	}
	
	std::cout << "RuleEngine is to be closed!" << std::endl;

    return 0;
}


// add handling of temperature values:
// 1) sharp increase or decrease
// 2) if > {some value} for 3 times then room temperature has increased
// 3) the same with <