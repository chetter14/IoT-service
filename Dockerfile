# GNU compiler and build tools
FROM gcc:latest

# Necessary dependencies for C++ compilation, build, RabbitMQ and MongoDB
RUN apt-get update && apt-get install -y \
	libbson-1.0 \
	cmake \
	libssl-dev \
	libsasl2-dev \
	git \
	&& rm -rf /var/lib/apt/lists/*
	
# Clone and build AMPQ-CPP
RUN git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git && \
	# mkdir /tmp/amqp-cpp/build && \
	cd AMQP-CPP && \
	make && make install
	
# Pull the tar.gz MongoDB C++ driver
RUN curl -OL https://github.com/mongodb/mongo-cxx-driver/releases/download/r3.10.1/mongo-cxx-driver-r3.10.1.tar.gz && \
	tar -xzf mongo-cxx-driver-r3.10.1.tar.gz
	
# Build the MongoDB C++ driver and install it
RUN	cd mongo-cxx-driver-r3.10.1/build && \
	cmake .. -DCMAKE_BUILD_TYPE=Release -DMONGOCXX_OVERRIDE_DEFAULT_INSTALL_PREFIX=OFF -DCMAKE_CXX_STANDARD=20 && \
	cmake --build . && \
	cmake --build . --target install
	
RUN ldconfig

# Set the working directory
WORKDIR /app

# Copy sources into /app directory
COPY source/ /app

# Compile DataSimulator and IoTController
RUN g++ -std=c++20 -I/usr/local/include -I/usr/local/include/mongocxx/v_noabi -I/usr/local/include/bsoncxx/v_noabi \
		-lamqpcpp -lpthread -ldl -lmongocxx -lbsoncxx DataSimulator.cpp -o DataSimulator && \
    g++ -std=c++20 -I/usr/local/include -I/usr/local/include/mongocxx/v_noabi -I/usr/local/include/bsoncxx/v_noabi \ 
		-lamqpcpp -lpthread -ldl -lmongocxx -lbsoncxx IoTController.cpp -o IoTController && \
    g++ -std=c++20 -I/usr/local/include -I/usr/local/include/mongocxx/v_noabi -I/usr/local/include/bsoncxx/v_noabi \
		-lamqpcpp -lpthread -ldl -lmongocxx -lbsoncxx RuleEngine.cpp -o RuleEngine

