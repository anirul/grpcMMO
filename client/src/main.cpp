#include "Client.hpp"

#include <exception>
#include <iostream>

#include "frame/logger.h"

int main(int argc, char** argv)
try
{
    grpcmmo::client::Client client;
    return client.Run(argc, argv);
}
catch (const std::exception& ex)
{
    auto& logger = frame::Logger::GetInstance();
    logger->error("Unhandled exception in grpcmmo_client: {}", ex.what());
    logger->flush();
    std::cerr << ex.what() << std::endl;
    std::cerr.flush();
    return 1;
}
