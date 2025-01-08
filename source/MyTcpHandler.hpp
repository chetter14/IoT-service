#ifndef MY_TCP_HANDLER_H
#define MY_TCP_HANDLER_H

#include "amqpcpp.h"
#include "amqpcpp/linux_tcp.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <string_view>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>


// Define a custom handler to manage connection and errors
class MyTcpHandler : public AMQP::TcpHandler {
public:	
    void onConnected(AMQP::TcpConnection *connection) override {
        std::cout << "Connected to RabbitMQ!" << std::endl;
    }

    void onError(AMQP::TcpConnection *connection, const char *message) override {
        std::cerr << "Connection error: " << message << std::endl;
    }

    void onClosed(AMQP::TcpConnection *connection) override {
        std::cout << "Connection closed!" << std::endl;
    }
	
	// Register fd (file descriptor)
    void monitor(AMQP::TcpConnection *connection, int fd, int flags) override {
        if (flags == 0) {
            // If no flags are set, remove fd from monitoring (i.e., close it)
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            return;
        }

        // Define the events we want to monitor on this file descriptor
        struct epoll_event event;
        event.data.fd = fd;
        event.events = 0;

        // Set events based on flags from AMQP-CPP
        if (flags & AMQP::readable) {
            event.events |= EPOLLIN;  // Ready to read
        }
        if (flags & AMQP::writable) {
            event.events |= EPOLLOUT; // Ready to write
        }

        // Add or modify the file descriptor in epoll's interest list
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) {
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
        }
    }
	
	// Initialize epoll instance
	MyTcpHandler() {
		epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
        }
	}
	
	// Close the epoll instance
    ~MyTcpHandler() {
        close(epoll_fd);
    }
	
	// Process events of fd in the epoll loop
    void processEvents(AMQP::TcpConnection* connection) {
        const int MAX_EVENTS = 10;
        struct epoll_event events[MAX_EVENTS];

		int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 0);
		for (int i = 0; i < num_events; ++i) {
			int fd = events[i].data.fd;
			if (events[i].events & EPOLLIN) {
				connection->process(fd, AMQP::readable);
			}
			if (events[i].events & EPOLLOUT) {
				connection->process(fd, AMQP::writable);
			}
		}
    }

private:
	int epoll_fd;			// File descriptor for epoll
};

namespace iot_service {
	namespace mqbroker {
		constexpr std::string_view Exchange = "exchange";
		constexpr std::string_view DataSimulatorQueue = "temperature_values";
		constexpr std::string_view RuleEngineQueue = "temperature_rules";
		constexpr std::string_view DSQueueRoutingKey = "values";
		constexpr std::string_view REQueueRoutingKey = "rules";
	}
	constexpr std::string_view DatabaseName = "iot_db";
}



#endif 	// MY_TCP_HANDLER_H