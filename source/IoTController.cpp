#include "MyTcpHandler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "Prometheus.hpp"
#include <prometheus/registry.h>
#include <prometheus/counter.h>
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

/*
Yes, it is possible to fetch performance data from Prometheus in your C++ code. Prometheus provides an **HTTP API** that allows you to query its data programmatically. You can use this API to fetch metrics and use them for decision-making, such as dynamically creating new threads based on incoming request rates.

---

### **Steps to Fetch Prometheus Data in C++**

1. **Understand the Prometheus HTTP API**:
   - The Prometheus HTTP API allows you to query metrics using the `/api/v1/query` endpoint.
   - Example query URL:
     ```
     http://<PROMETHEUS_SERVER>:9090/api/v1/query?query=rate(example_counter[1m])
     ```
   - The response is in JSON format.

2. **Use an HTTP Client Library in C++**:
   - Libraries like **cURL**, **Boost.Beast**, or **cpprestsdk** can be used to make HTTP requests.

3. **Parse JSON Responses**:
   - Use a JSON parsing library, such as **nlohmann/json** or **RapidJSON**, to handle the Prometheus API response.

4. **Integrate the Logic**:
   - Fetch the request rate from Prometheus.
   - Dynamically adjust the number of threads based on the rate.

---

### **Example Code**

Here’s an example of how you might fetch data from Prometheus and adjust threads:

#### Prerequisites:
- Install **libcurl** and **nlohmann/json** for HTTP requests and JSON parsing.

#### Code:

```cpp
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

using json = nlohmann::json;

// Callback function to capture HTTP response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Fetch data from Prometheus
double fetchPrometheusMetric(const std::string& query) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    std::string url = "http://localhost:9090/api/v1/query?query=" + query;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    if (res != CURLE_OK) {
        std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
        return -1;
    }

    // Parse the JSON response
    auto jsonResponse = json::parse(readBuffer);
    if (jsonResponse["status"] == "success") {
        double value = std::stod(jsonResponse["data"]["result"][0]["value"][1].get<std::string>());
        return value;
    }

    std::cerr << "Error fetching metric: " << jsonResponse.dump() << std::endl;
    return -1;
}

// Example: Dynamically adjust threads
void manageThreads(double requestRate, std::vector<std::thread>& threads) {
    int desiredThreads = static_cast<int>(requestRate * 10); // Example scaling logic
    while (threads.size() < desiredThreads) {
        threads.emplace_back([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Example task
        });
        std::cout << "New thread created. Total threads: " << threads.size() << std::endl;
    }
}

int main() {
    std::vector<std::thread> threads;

    // Example query for incoming request rate
    std::string query = "rate(requests_total[1m])";

    while (true) {
        double requestRate = fetchPrometheusMetric(query);
        if (requestRate >= 0) {
            std::cout << "Current request rate: " << requestRate << " requests/s" << std::endl;
            manageThreads(requestRate, threads);
        } else {
            std::cerr << "Failed to fetch metrics" << std::endl;
        }

        // Cleanup finished threads
        threads.erase(std::remove_if(threads.begin(), threads.end(),
                                     [](std::thread& t) { return !t.joinable(); }),
                      threads.end());

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    return 0;
}
```

---

### **Explanation**

1. **Fetch Metrics**:
   - The `fetchPrometheusMetric` function sends an HTTP GET request to the Prometheus `/api/v1/query` endpoint.
   - It parses the JSON response to extract the metric value.

2. **Adjust Threads**:
   - The `manageThreads` function adjusts the number of threads based on the incoming request rate.

3. **Thread Cleanup**:
   - The code removes finished threads from the vector to avoid resource leakage.

---

### **Key Considerations**

1. **Rate Limiting**:
   - Avoid querying Prometheus too frequently. Use a reasonable interval (e.g., every 10 seconds).

2. **Error Handling**:
   - Add robust error handling for network issues, invalid responses, and JSON parsing errors.

3. **Optimization**:
   - Make thread management efficient to prevent excessive creation and destruction of threads.

4. **Secure Access**:
   - If Prometheus requires authentication, include the necessary credentials in the HTTP request.

---

Let me know if you have further questions or need help with implementation details!



*/ 

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

    // Declare the queue with DataSimulator to consume messages from it
    channel.declareQueue(mqbroker::DataSimulatorQueue);	
	channel.consume(mqbroker::DataSimulatorQueue).onReceived([&](const AMQP::Message &message,
                                              uint64_t deliveryTag,
                                              bool redelivered) {
		// Extract the message body (temperature value)
		std::string received_message(message.body(), message.bodySize());
		int temperature = std::stoi(received_message);
		
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
		
		channel.publish(mqbroker::Exchange, mqbroker::REQueueRoutingKey, received_message);
	});
	
	while (true) {
		handler.processEvents(&connection);
	}
	
	std::cout << "IoT controller is to be closed!" << std::endl;
    return 0;
}
