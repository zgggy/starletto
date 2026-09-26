// 测试用插件：唯一目的是在**独立 DSO** 内生成 CorrelationId，
// 用于验证多 DSO 下 id 不碰撞（见 correlation_id_test.cc）
#include "basement/transport/api/correlation_id.h"

extern "C" uint64_t test_generate_correlation_id() { return star::transport::CorrelationId::generate().value; }
