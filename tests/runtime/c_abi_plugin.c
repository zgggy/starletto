/* 纯 C 插件：只包含 C ABI 头与 libc，用来证明宿主不再要求同工具链。 */
#include "basement/plugin/star_plugin_abi.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef STAR_CABI_TEST_BAD_ABI
#define STAR_CABI_PLUGIN_ABI_VERSION 99u
#else
#define STAR_CABI_PLUGIN_ABI_VERSION STAR_PLUGIN_ABI_VERSION
#endif

typedef struct {
    const star_component_api_v1* api;
    int initialized;
    int shutdown_called;
} echo_state;

static void* echo_create(void) {
    return calloc(1, sizeof(echo_state));
}

static void echo_destroy(void* self) {
    free(self);
}

static int echo_init(void* self, const star_component_api_v1* api) {
    echo_state* state = (echo_state*)self;
    if (state == NULL || api == NULL) {
        return -1;
    }
    state->api = api;
    state->initialized = 1;
    return 0;
}

static void put_u32(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void put_u64(uint8_t* out, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out[i] = (uint8_t)((value >> (8 * i)) & 0xFFu);
    }
}

static int echo_process(void* self) {
    echo_state* state = (echo_state*)self;
    if (state == NULL || state->api == NULL) {
        return -2;
    }

    /* 输出布局：flags(1) | read_size(4) | schema_version(8) | schema_hash(8) | payload */
    uint32_t flags = 0;
    if (state->initialized) {
        flags |= 1u << 4;
    }

    const char* schema_name = NULL;
    int64_t schema_version = 0;
    uint64_t schema_hash = 0;
    size_t size = 0;
    uint8_t* input = NULL;
    int rc = state->api->read_latest(state->api->ctx, "in", NULL, 0, &size, &schema_name, &schema_version, &schema_hash);
    if (rc == 2) {
        input = (uint8_t*)malloc(size > 0 ? size : 1);
        if (input == NULL) {
            return -3;
        }
        rc = state->api->read_latest(state->api->ctx, "in", input, size, &size, &schema_name, &schema_version, &schema_hash);
    }
    if (rc == 0) {
        flags |= 1u << 0;
    }
    if (schema_name != NULL && strcmp(schema_name, "test.cabi.input") == 0) {
        flags |= 1u << 3;
    }

    /* 未声明的端口必须失败，且 last_error 有可读信息 */
    const uint8_t probe = 0;
    if (state->api->publish(state->api->ctx, "missing", &probe, 1, "", 0, 0) < 0 && state->api->last_error(state->api->ctx)[0] != '\0') {
        flags |= 1u << 2;
    }

    const size_t payload_size = (rc == 0) ? size : 0;
    const size_t total = 21 + payload_size;
    uint8_t* output = (uint8_t*)malloc(total > 0 ? total : 1);
    if (output == NULL) {
        free(input);
        return -4;
    }
    output[0] = (uint8_t)flags;
    put_u32(output + 1, (uint32_t)payload_size);
    put_u64(output + 5, (uint64_t)schema_version);
    put_u64(output + 13, schema_hash);
    if (payload_size > 0) {
        memcpy(output + 21, input, payload_size);
    }
    free(input);

    const int publish_rc = state->api->publish(state->api->ctx, "out", output, total, "test.cabi.echo", 123, 0x1122334455667788ULL);
    free(output);
    return publish_rc == 0 ? 0 : 1;
}

static void echo_shutdown(void* self) {
    echo_state* state = (echo_state*)self;
    if (state != NULL) {
        state->shutdown_called = 1;
    }
}

static const star_plugin_component_v1 k_components[] = {
    {"star::cabi::Echo", echo_create, echo_destroy, echo_init, echo_process, echo_shutdown},
};

static const star_plugin_v1 k_plugin = {
    STAR_CABI_PLUGIN_ABI_VERSION,
    "c_abi_test_plugin",
    k_components,
    sizeof(k_components) / sizeof(k_components[0]),
};

const star_plugin_v1* star_plugin_entry_v1(void) {
    return &k_plugin;
}
