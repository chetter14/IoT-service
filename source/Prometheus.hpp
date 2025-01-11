#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <memory>
#include <string>

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
