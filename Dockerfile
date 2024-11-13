# GNU compiler and build tools
FROM gcc:latest

# Necessary dependencies
RUN apt-get update && apt-get install -y \
	cmake \
	libssl-dev \
	git \
	&& rm -rf /var/lib/apt/lists/*
	
# Clone and build AMPQ-CPP
RUN git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git  &&\
	# mkdir /tmp/amqp-cpp/build && \
	cd AMQP-CPP && \
	make && make install
	
RUN ldconfig

# Set the working directory
WORKDIR /app

# Copy sources into /app directory
COPY source/ /app

# Compile DataSimulator and IoTController
RUN g++ -std=c++20 -I/usr/local/include -lamqpcpp -lpthread -ldl DataSimulator.cpp -o DataSimulator && \
    g++ -std=c++20 -I/usr/local/include -lamqpcpp -lpthread -ldl IoTController.cpp -o IoTController
	
CMD ["./DataSimulator"]

