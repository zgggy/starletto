#ifndef __STAR_SETTING_SCHEMA_SCHEMA_BOOTSTRAP_H__
#define __STAR_SETTING_SCHEMA_SCHEMA_BOOTSTRAP_H__

#include <atomic>

#include "basement/schema/schema_registry.h"
#include "modules/setting/schema/config/component.s.h"
#include "modules/setting/schema/config/logger.s.h"
#include "modules/setting/schema/config/sensor.s.h"
#include "modules/setting/schema/config/system.s.h"
#include "modules/setting/schema/config/vehicle.s.h"
#include "modules/setting/schema/config_snapshot.s.h"
#include "modules/setting/schema/control/command.s.h"
#include "modules/setting/schema/parameter_snapshot.s.h"
#include "modules/setting/schema/planning/motion.s.h"
#include "modules/setting/schema/planning/trajectory.s.h"
#include "modules/setting/schema/service/parameter_get_request.s.h"
#include "modules/setting/schema/service/parameter_get_response.s.h"
#include "modules/setting/schema/service/parameter_list_request.s.h"
#include "modules/setting/schema/service/parameter_list_response.s.h"
#include "modules/setting/schema/service/parameter_set_request.s.h"
#include "modules/setting/schema/service/parameter_set_response.s.h"

namespace star::setting::schema {

inline auto register_setting_schemas(star::schema::SchemaRegistry& registry) -> void {
    static std::atomic<bool> registered{false};
    if (registered.exchange(true)) {
        return;
    }
    star::generated::setting::parameter::snapshot::ParameterSnapshot::register_schema(registry);
    star::generated::setting::config::snapshot::ConfigSnapshot::register_schema(registry);
    star::setting::parameter::planning::MotionParameter::register_schema(registry);
    star::setting::service::parameter::get_request::ParameterGetRequest::register_schema(registry);
    star::setting::service::parameter::get_response::ParameterGetResponse::register_schema(registry);
    star::setting::service::parameter::list_request::ParameterListRequest::register_schema(registry);
    star::setting::service::parameter::list_response::ParameterListResponse::register_schema(registry);
    star::setting::service::parameter::set_request::ParameterSetRequest::register_schema(registry);
    star::setting::service::parameter::set_response::ParameterSetResponse::register_schema(registry);
    star::setting::config::component::ComponentConfig::register_schema(registry);
    star::setting::config::logger::LoggerConfig::register_schema(registry);
    star::setting::config::sensor::SensorConfig::register_schema(registry);
    star::setting::config::system::SystemConfig::register_schema(registry);
    star::setting::config::vehicle::VehicleConfig::register_schema(registry);
    star::setting::planning::TrajectoryOutput::register_schema(registry);
    star::setting::control::ControlCommand::register_schema(registry);
}

}  // namespace star::setting::schema

#endif  // __STAR_SETTING_SCHEMA_SCHEMA_BOOTSTRAP_H__
