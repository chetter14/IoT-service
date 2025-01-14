#include "MyTcpHandler.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <string>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "Prometheus.hpp"
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include "Logger.hpp"
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

class ThreadPool {
public:
    ThreadPool() : stop_flag_(false) {
		Logger::Info(R"({"service":"IoT Controller", "message":"Thread pool started"})");
	}

    ~ThreadPool() {
        Stop();
    }

    void EnqueueMessage(const std::string& message) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            message_queue_.push(message);
        }
        queue_condition_.notify_one();
		Logger::Info(R"({"service":"IoT Controller", "level":"info", "message":"Enqueued message"})");
    }

    void AdjustThreads(std::size_t desired_threads) {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        std::size_t current_threads = workers_.size();

        if (desired_threads > current_threads) {
            for (std::size_t i = current_threads; i < desired_threads; ++i) {
                workers_.emplace_back(&ThreadPool::WorkerThread, this);
            }
        } else if (desired_threads < current_threads) {
            std::size_t threads_to_stop = current_threads - desired_threads;
            for (std::size_t i = 0; i < threads_to_stop; ++i) {
                EnqueueMessage("STOP_THREAD");
            }
        }
    }

    void Stop() {
        AdjustThreads(0);
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stop_flag_ = true;
        }
        queue_condition_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }
	
	void SetProcessMessageFunction(std::function<void(const std::string&)> process_message_functor) {
		process_message_ = process_message_functor;
	}

private:
    void WorkerThread() {
        while (true) {
            std::string message;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_condition_.wait(lock, [this] { return !message_queue_.empty() || stop_flag_; });

                if (stop_flag_ && message_queue_.empty()) {
                    return;
                }

                message = message_queue_.front();
                message_queue_.pop();
            }

            if (message == "STOP_THREAD") {
                return;
            }

            process_message_(message);
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::string> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::mutex pool_mutex_;
    std::atomic<bool> stop_flag_;
	
	std::function<void(const std::string&)> process_message_;
};

void MonitorMessageRateAndAdjustThreads(ThreadPool& thread_pool, AMQP::TcpChannel& channel) {
    const std::size_t messages_per_thread = 100; // Target messages per thread per second
    const std::size_t min_threads = 1;          // Minimum threads to keep alive

    while (true) {
        double rate = FetchMessageRate("http://prometheus:9090", "rate(iot_controller_counter[1s])");
		std::size_t required_threads = std::max(1, static_cast<int>(std::ceil(rate / 100.0)));
		
		Logger::Info(std::format("\"service\":\"IoT Controller\", \"message\":\"Required threads - {}\"", required_threads));

        // Adjust thread pool size
        thread_pool.AdjustThreads(required_threads);
    }
}

int main() {
	// Initialize Logger
	Logger::Initialize("logstash", 5044);
	Logger::Info(R"({"service":"IoT Controller", "level":"info", "message":"Service started"})");
	
	// Initialize thread pool
	ThreadPool thread_pool;
	
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
	
	// Initialize Prometheus:
	
	using namespace prometheus;
	MetricsManager manager("0.0.0.0:8080");
	// Get a registry for collecting metrics
    auto registry = manager.GetRegistry();
	// Create a counter and register it
    auto& counter_family = BuildCounter()
                           .Name("iot_controller_counter")
                           .Help("IoT controller counter")
                           .Register(*registry);
	auto& message_counter = counter_family.Add({{"message_counter", "value"}});

	// Launch monitoring thread (to adjust worker threads)
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(5s);	// Wait for Prometheus to start
	std::thread monitor_thread(MonitorMessageRateAndAdjustThreads, std::ref(thread_pool), std::ref(channel));

	// Set function to process messages
	thread_pool.SetProcessMessageFunction([&message_counter, &temp_values_collection, &channel](const std::string& message) {
		int temperature = std::stoi(message);
		
		// Log reception of temperature
		Logger::Info(R"({"service":"IoT Controller", "level":"info", "message":"Received a temperature"})");
		
		// Update message_counter value
		message_counter.Increment();
		
		// Get the current time
		auto now = std::chrono::system_clock::now();
		auto bson_date = bsoncxx::types::b_date{ now };		// Convert to BSON format
		
		// Insert the current time point (date and stuff) and temperature value into collection
		temp_values_collection.insert_one(bsoncxx::builder::basic::make_document(
			bsoncxx::builder::basic::kvp("Temperature", temperature),
			bsoncxx::builder::basic::kvp("Time", bson_date)
		));
		
		channel.publish(mqbroker::Exchange, mqbroker::REQueueRoutingKey, message);
		
		// Log sending of temperature
		Logger::Info(R"({"service":"IoT Controller", "level":"info", "message":"Sent a temperature"})");
	});

    // Declare the queue with DataSimulator to consume messages from it
    channel.declareQueue(mqbroker::DataSimulatorQueue);	
	channel.consume(mqbroker::DataSimulatorQueue).onReceived([&](const AMQP::Message &message,
                                              uint64_t deliveryTag,
                                              bool redelivered) {
		// Extract the message body (temperature value)
		std::string received_message(message.body(), message.bodySize());
		thread_pool.EnqueueMessage(received_message);
	});
	
	while (true) {
		handler.processEvents(&connection);
	}
	
	monitor_thread.join();
	std::cout << "IoT controller is to be closed!" << std::endl;
    return 0;
}
