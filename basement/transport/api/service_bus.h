#ifndef __SERVICE_BUS_H__
#define __SERVICE_BUS_H__

#include <functional>
#include <future>
#include <memory>
#include <string>

#include "basement/transport/api/message_bus.h"

namespace star::transport {

struct ServiceDescriptor {
    std::string name;
};

struct ServiceRequest {
    MessageView body;
};

struct ServiceResponse {
    bool ok{true};
    std::string message;
    MessageView body;
};

struct SerializedServiceRequest {
    ServiceRequest request;
    MessageSerializer serializer;
};

struct SerializedServiceResponse {
    ServiceResponse response;
    MessageSerializer serializer;
};

class ServiceClient {
  public:
    virtual ~ServiceClient() = default;

    virtual auto available() const -> bool = 0;
    virtual auto call(const ServiceRequest& request) -> ServiceResponse = 0;
    virtual auto call_serialized(const SerializedServiceRequest& request) -> ServiceResponse = 0;
    virtual auto async_call(const ServiceRequest& request) -> std::future<ServiceResponse> = 0;
    virtual auto async_call_serialized(const SerializedServiceRequest& request) -> std::future<ServiceResponse> = 0;
};

class ServiceServer {
  public:
    virtual ~ServiceServer() = default;
    virtual void start() {}
    virtual void stop() {}
};

class ServiceBus {
  public:
    using Handler = std::function<SerializedServiceResponse(const ServiceRequest&)>;

    virtual ~ServiceBus() = default;

    virtual auto create_client(const ServiceDescriptor& descriptor) -> std::shared_ptr<ServiceClient> = 0;
    virtual auto create_server(const ServiceDescriptor& descriptor, Handler handler) -> std::shared_ptr<ServiceServer> = 0;
};

}  // namespace star::transport

#endif  // __SERVICE_BUS_H__
