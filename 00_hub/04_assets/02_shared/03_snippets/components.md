# 组件代码片段

## Component 模板

```cpp
#ifndef __EXAMPLE_COMPONENT_H__
#define __EXAMPLE_COMPONENT_H__

#include "basement/runtime/component/component.h"

namespace star::example {

class ExampleComponent final : public star::runtime::Component {
public:
    auto name() const -> std::string override { return "example"; }
    auto version() const -> std::string override { return "0.1.0"; }

    void Configure(const star::runtime::ComponentContext& context) override {
        reader_ = context.message_bus().create_reader<InputType>("input.topic");
        writer_ = context.message_bus().create_writer<OutputType>("output.topic");
    }

    void Start() override {}

    void Stop() override {}

    bool Process(star::runtime::FrameContext& frame) override {
        auto input = read_latest<InputType>(*reader_);
        if (!input) { return true; }
        OutputType output;
        output = process(*input);
        writer_->write(output);
        return true;
    }

private:
    std::shared_ptr<star::transport::Reader> reader_;
    std::shared_ptr<star::transport::Writer> writer_;
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
