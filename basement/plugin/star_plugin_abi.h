#ifndef __STAR_PLUGIN_ABI_H__
#define __STAR_PLUGIN_ABI_H__

/*
 * Starletto 插件 C ABI（v1）
 *
 * 这个头文件是**纯 C**：不含任何 C++ 类型、模板或 STL。用任意 C 编译器
 * （gcc/clang/交叉编译器）编出的 .so 都可以被宿主加载，不需要与宿主同工具链。
 *
 * 能力边界（v1）：
 *   - 组件只能通过端口读写**原始 wire 字节**，不做类型化解码；
 *     schema 身份以 name/version/hash 三元组随 payload 传递。
 *   - 不支持在 C 插件里注册 schema（schema 仍由 C++ 插件或宿主注册）。
 *   - 组件配置（component.conf）不在 ABI 内暴露；需要配置的组件继续用 C++ 插件。
 *
 * 生命周期：宿主 create → init(api) → process* → shutdown → destroy。
 * 所有回调都在宿主的调度线程上调用，插件无需自己加锁；但 read_latest/publish
 * 只能在 process/init/shutdown 调用栈内使用（api->ctx 仅在此期间有效）。
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STAR_PLUGIN_ABI_VERSION 1u
#define STAR_PLUGIN_ENTRY_SYMBOL "star_plugin_entry_v1"

typedef struct star_component_api_v1 {
    uint32_t abi_version; /* == STAR_PLUGIN_ABI_VERSION */

    /* 宿主持有的不透明上下文；原样传回本结构里的回调 */
    void* ctx;

    /* 当前调度帧信息，每次 process 前由宿主刷新 */
    int64_t frame_id;
    int64_t timestamp_ns;

    /*
     * 读取端口最近一条消息的原始 payload。
     *
     * 返回：
     *   0  成功；buffer 已写入 out_size 字节
     *   1  暂无数据
     *   2  buffer 太小或为 NULL；out_size 已写入所需字节数（可据此二次调用）
     *  <0  错误（详情见 last_error）
     *
     * 若消息携带 schema 身份，schema_name 指向宿主内部字符串（有效期到下一次
     * read_latest 或 process 返回）；不需要时可传 NULL。
     */
    int (*read_latest)(void* ctx, const char* port, void* buffer, size_t capacity, size_t* out_size, const char** schema_name, int64_t* schema_version,
                       uint64_t* schema_hash);

    /*
     * 发布一条原始 payload 到已声明的输出端口。
     *
     * schema_name/version/hash 会写进 shm 帧头，供接收方做身份校验；传 NULL/0
     * 表示不声明身份。返回 0 成功，<0 失败（详情见 last_error）。
     */
    int (*publish)(void* ctx, const char* port, const void* data, size_t size, const char* schema_name, int64_t schema_version, uint64_t schema_hash);

    /* 最近一次失败的可读描述；无错误时返回空字符串。指针在下次调用前有效。 */
    const char* (*last_error)(void* ctx);
} star_component_api_v1;

typedef struct star_plugin_component_v1 {
    /* 必须与 component.conf 的 component_class 一致 */
    const char* class_name;

    void* (*create)(void);
    void (*destroy)(void* self);

    /* 以下回调可空；init 返回非 0 表示组件初始化失败 */
    int (*init)(void* self, const star_component_api_v1* api);
    int (*process)(void* self);
    void (*shutdown)(void* self);
} star_plugin_component_v1;

typedef struct star_plugin_v1 {
    uint32_t abi_version; /* 必须 == STAR_PLUGIN_ABI_VERSION */
    const char* plugin_name;
    const star_plugin_component_v1* components;
    size_t component_count;
} star_plugin_v1;

/*
 * 插件必须导出：
 *   const star_plugin_v1* star_plugin_entry_v1(void);
 * 返回的 descriptor 必须静态存储、在插件卸载前一直有效。
 */
typedef const star_plugin_v1* (*star_plugin_entry_v1_fn)(void);

/* 宿主侧实现：返回本运行时支持的插件 ABI 版本（供外部工具探测）。 */
uint32_t star_runtime_plugin_abi_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __STAR_PLUGIN_ABI_H__ */
