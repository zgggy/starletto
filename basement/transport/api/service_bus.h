#ifndef __SERVICE_BUS_H__
#define __SERVICE_BUS_H__

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/core/common_types.h"
#include "basement/schema/type_id.h"
#include "libraries/common/schema/schema_value.h"

namespace star::transport {

struct CorrelationId {
    uint64_t value{0};

    auto is_valid() const -> bool { return value != 0; }

    auto operator==(const CorrelationId& o) const -> bool { return value == o.value; }

    static auto generate() -> CorrelationId {
        static std::atomic<uint64_t> next_id{1};
        return CorrelationId{next_id.fetch_add(1, std::memory_order_relaxed)};
    }
};

}  // namespace star::transport

namespace std {

template <>
struct hash<star::transport::CorrelationId> {
    auto operator()(const star::transport::CorrelationId& cid) const -> size_t { return std::hash<uint64_t>{}(cid.value); }
};

}  // namespace std

namespace star::transport {

struct ServiceDescriptor {
    std::string name;
};

struct ServiceRequest {
    star::base::TypedConstPtr data_typed;
    star::schema::SchemaTypeId schema_type_id;
    std::function<std::vector<uint8_t>()> encode_binary;
    star::schema::SchemaValue data;
    CorrelationId correlation_id;
};

struct ServiceResponse {
    bool ok{true};
    std::string message;
    star::base::TypedConstPtr data_typed;
    star::schema::SchemaTypeId schema_type_id;
    std::function<std::vector<uint8_t>()> encode_binary;
    star::schema::SchemaValue data;
    CorrelationId correlation_id;
};

class ServiceClient {
  public:
    virtual ~ServiceClient() = default;

    virtual auto available() const -> bool = 0;
    virtual auto call(const ServiceRequest& request) -> ServiceResponse = 0;
    virtual auto async_call(const ServiceRequest& request) -> std::future<ServiceResponse> = 0;
};

class ServiceServer {
  public:
    virtual ~ServiceServer() = default;
};

class ServiceBus {
  public:
    using Handler = std::function<ServiceResponse(const ServiceRequest&)>;

    virtual ~ServiceBus() = default;

    virtual auto create_client(const ServiceDescriptor& descriptor) -> std::shared_ptr<ServiceClient> = 0;
    virtual auto create_server(const ServiceDescriptor& descriptor, Handler handler) -> std::shared_ptr<ServiceServer> = 0;
};

}  // namespace star::transport

#endif  // __SERVICE_BUS_H__
