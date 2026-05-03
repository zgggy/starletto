# 组件代码片段

## Component 模板

```cpp
#ifndef __EXAMPLE_COMPONENT_H__
#define __EXAMPLE_COMPONENT_H__

#include "basement/runtime/component/component.h"

namespace star::example {

class ExampleComponent final : public star::runtime::Component {
public:
    void init() override {
        auto_configure();
    }

    auto process_once() -> bool override {
        auto input = read_latest<InputType>("input.topic");
        if (!input) {
            return true;
        }
        const auto output = process(*input);
        write("output.topic", output);
        return true;
    }

    void shutdown() override {}

private:
    static auto process(const InputType&) -> OutputType;
};

}  // namespace star::example

#endif  // __EXAMPLE_COMPONENT_H__
```

## Component 注册

```cpp
void register_example_components(star::runtime::ComponentRegistry& registry) {
    registry.register_component<star::example::ExampleComponent>();
}

extern "C" void star_register_components(star::runtime::ComponentRegistry& registry) {
    star::example::register_example_components(registry);
}
```
