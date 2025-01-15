#ifndef PROMETHEUS_HPP
#define PROMETHEUS_HPP

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <memory>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace prometheus;

// Class to handle Prometheus metrics
class MetricsManager {
public:
	MetricsManager(const std::string& exposer_address) 
		: exposer_(exposer_address), registry_(std::make_shared<Registry>()) {
		exposer_.RegisterCollectable(registry_);
	}
	
	std::shared_ptr<Registry> GetRegistry() {
        return registry_;
    }
	
private:
	Exposer exposer_;
	std::shared_ptr<Registry> registry_;
};

using json = nlohmann::json;

// Callback function for handling HTTP response
static std::size_t WriteCallback(void* contents, std::size_t size, std::size_t nmemb, std::string* out) {
    std::size_t totalSize = size * nmemb;
    if (out) {
        out->append((char*)contents, totalSize);
    }
    return totalSize;
}

double FetchMessageRate(const std::string& prometheus_url, const std::string& query) {
    CURL* curl;
    CURLcode res;
    std::string response;

    // Initialize curl
    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    // Construct the Prometheus query URL
    std::string url = prometheus_url + "/api/v1/query?query=" + query;

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    // Cleanup
    curl_easy_cleanup(curl);

    // Parse the JSON response
    json jsonResponse = json::parse(response);
    if (jsonResponse["status"] != "success") {
        throw std::runtime_error("Prometheus query failed: " + jsonResponse.dump());
    }
	
    // Extract the value
    auto result = jsonResponse["data"]["result"];
    if (result.empty()) {
        // throw std::runtime_error("Prometheus; Fetching rate; No data returned for the query");
        std::cout << "No data returned for the query" << std::endl;
		return 0.0;
    }

    double rate = std::stod(result[0]["value"][1].get<std::string>());
	std::cout << "Prometheus; Fetched rate - " << rate << std::endl;
    return rate;
}

#endif 	// PROMETHEUS_HPP