# 工具函数片段

## 时间戳生成

```cpp
static auto make_timestamp() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}
```

## 路径列表分割（冒号分隔）

```cpp
static auto split_path_list(const char* value) -> std::vector<std::string> {
    if (value == nullptr) { return {}; }
    auto s = std::string(value);
    std::vector<std::string> out;
    std::string cur;
    for (const auto ch : s) {
        if (ch == ':') {
            if (not cur.empty()) { out.emplace_back(std::move(cur)); cur = {}; }
            continue;
        }
        cur.push_back(ch);
    }
    if (not cur.empty()) { out.emplace_back(std::move(cur)); }
    return out;
}
```

## i64 参数解析

```cpp
static auto parse_i64_arg(const std::string& s, const std::string& prefix)
    -> std::optional<int64_t> {
    if (s.rfind(prefix, 0) != 0) { return std::nullopt; }
    const auto v = s.substr(prefix.size());
    if (v.empty()) { return std::nullopt; }
    try { return static_cast<int64_t>(std::stoll(v)); }
    catch (...) { return std::nullopt; }
}
```

## 文件路径短名

```cpp
static auto short_file(const char* file) -> const char* {
    const auto* slash = std::strrchr(file, '/');
    return slash ? slash + 1 : file;
}
```
