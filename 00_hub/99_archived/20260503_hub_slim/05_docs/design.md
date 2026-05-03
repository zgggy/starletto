# starletto 网站设计系统

本文档描述 starletto 官网的设计语言、组件样式和实现规范。

## 1. 视觉主题与设计理念

starletto 官网的设计灵感来自 WIRED 的编辑风格，采用新闻级密度的网格布局、无圆角、纯色系和严格的排版规则。整体呈现专业、技术导向的视觉风格，强调内容的清晰传达和优雅的交互体验。

**设计特点：**

- 新闻级密度网格：使用边框和留白分隔内容区域，不使用卡片阴影
- 严格的排版系统：使用大号无衬线字体作为标题，技术等宽字体用于标签和分类
- 极简色彩系统：黑、白、灰为主，蓝色作为唯一的强调色
- 无圆角设计：所有容器、按钮、输入框均使用方角
- 硬边框交互：使用 2px 黑色边框定义主要交互元素

## 2. 色彩系统

### 主色（Editorial Ink）

| 名称 | 色值 | 用途 |
|------|------|------|
| WIRED Black | `#000000` | 标题丝带、分割线、按钮边框、正文标题 |
| Page Ink | `#1a1a1a` | 页面墨水，用于标题和正文，与纯黑相比更柔和 |
| Paper White | `#ffffff` | 默认画布，页面的主体背景色 |

### 次要色（Editorial Voice）

| 名称 | 色值 | 用途 |
|------|------|------|
| Link Blue | `#057dbc` | 单一品牌强调色，用于悬停状态和链接 |
| Caption Gray | `#757575` | 次要元数据：副标题、时间戳 |
| Hairline Tint | `#e2e8f0` | 用于 `<hr>` 元素之间的分隔线 |
| Panel Light | `#f8f9fa` | 面板背景色，用于代码块、图表容器等 |

### 语义色

- Link Hover Blue：`#057dbc` — 链接悬停状态
- 无其他语义色（成功/错误/警告），保持编辑风格

## 3. 字体系统

### 字体族

| 名称 | 用途 | 回退字体 |
|------|------|----------|
| Wired Display | 标题和副标题 | Helvetica, Arial |
| Breve Text | 正文内容 | Helvetica, Arial |
| Apercu | UI 标签、按钮、导航 | Helvetica, Arial |
| Wired Mono | 标签、分类、元数据、大写 | Courier New, Monaco |

### 字体层级

| 角色 | 字体 | 字号 | 字重 | 行高 | 字间距 | 说明 |
|------|------|------|------|------|--------|------|
| 页面标题 | Wired Display | 32px | 700 | 0.93 | -0.5px | 主标题，紧凑排列 |
| 弹窗标题 | Wired Display | 24px | 700 | - | - | 弹窗内的主标题 |
| 弹窗副标题 | Wired Display | 20px | 700 | - | - | 弹窗内的副标题 |
| 小标题 | Apercu | 16px | 700 | - | - | 组件标题 |
| 正文 | Breve Text | 16px | normal | 1.5 | 0 | 正文段落 |
| 弹窗正文 | Breve Text | 16px | normal | 1.5 | 0 | 弹窗内的正文 |
| 标签/分类 | Wired Mono | 12px | 700 | - | 0.92px | 大写，用于分类标签 |
| 元数据 | Wired Mono | 11px | normal | - | 1.1px | 大写，用于时间戳 |
| 代码 | Wired Mono | 12-13px | normal | 1.4 | 0 | 代码块和行内代码 |

### 字体使用原则

- Wired Display：用于标题和副标题，不用于正文
- Breve Text：用于所有正文内容阅读，不用于 UI
- Apercu：用于按钮、标签、导航等 UI 元素
- Wired Mono：必须大写，用于所有分类标签和元数据

## 4. 间距系统

### 基础单位

- 基础单位：8px

### 间距比例

| 名称 | 值 | 用途 |
|------|-----|------|
| xs | 4px | 极小间距 |
| sm | 8px | 小间距，元素内部 |
| md | 16px | 中等间距，组件之间 |
| lg | 24px | 大间距，区块之间 |
| xl | 32px | 特大间距，区块之间 |
| xxl | 48px | 最大间距，主要区块之间 |

### 区域间距

- 主要区块之间：48px (xxl)
- 组件之间：24px (lg)
- 元素内部：8-16px (sm-md)

## 5. 边框与深度

### 边框样式

| 名称 | 值 | 用途 |
|------|-----|------|
| Hard | 2px solid #000 | 按钮、输入框、主要交互元素 |
| Hairline | 1px solid #e2e8f0 | 次要分隔线，几乎不可见 |
| Editorial | 1px solid #000 | 编辑列分隔线，结构性强 |

### 深度层级

| 层级 | 样式 | 用途 |
|------|------|------|
| 0 | 无阴影、无边框 | 默认编辑面 |
| 1 | 1px solid #e2e8f0 | 安静的分隔线 |
| 2 | 1px solid #000 | 编辑列分隔线 |
| 3 | 2px solid #000 | 按钮、输入框、丝带 |
| 4 | 黑色填充丝带 | 区域标签 |
| 5 | 深色反转区块 | Footer 区域 |

### 深度哲学

无阴影、无圆角是核心原则。深度通过边框粗细传达，不通过模拟光照效果。

## 6. 组件样式

### 按钮

#### 主要按钮（黑色边框）

```css
background: #ffffff;
border: 2px solid #000000;
padding: 4px 12px;
font-family: Apercu;
font-size: 12px;
font-weight: 700;
```

悬停：背景变为黑色，文字变为白色，无过渡动画。

#### 次要按钮（Header 按钮）

```css
background: #ffffff;
color: #000000;
border: 2px solid #000000;
padding: 4px 12px;
font-family: Apercu;
font-size: 12px;
font-weight: 700;
```

悬停：背景变为黑色，文字变为白色。

### 输入框

```css
border: 1px solid #000000;
padding: 8px;
font-family: Breve Text;
font-size: 14px;
```

聚焦：仅通过光标闪烁表示，无边框颜色变化或光晕效果。

### 卡片

无圆角、无阴影。卡片通过边框分隔，背景与页面背景相同。

```css
padding: 16px;
border: 1px solid #000000;
background: #ffffff;
```

### 分类标签

```css
padding: 8px 16px;
border-bottom: 1px solid #e2e8f0;
font-family: Apercu;
font-size: 14px;
font-weight: 700;
```

悬停：颜色变为 Link Blue (#057dbc)。

### 文档项

```css
padding: 16px;
border: 1px solid #000000;
```

悬停：边框颜色变为 Link Blue (#057dbc)。

### 图表容器

```css
margin: 24px 0;
padding: 16px;
border: 1px solid #000000;
background: #f8f9fa;
```

### 占位符图表

```css
width: 100%;
height: 200px;
background: #ffffff;
border: 1px solid #e2e8f0;
display: flex;
align-items: center;
justify-content: center;
```

## 7. 布局原则

### 主布局

- 最大宽度：1600px
- 水平内边距：24px
- 三栏布局用于顶部区域

### 区块结构

```
Header (固定, 40px高)
  └── Logo (左) + 导航链接 (右)

Main
  ├── Top Section (三栏布局)
  │   ├── Left: 算法可视化
  │   ├── Center: 算法说明
  │   └── Right: 技术指标
  │
  ├── Horizontal Scroll (80px高)
  │   └── 场景滚动卡片
  │
  └── Documentation
      ├── Categories (300px宽)
      └── Doc List (自适应)

Footer (深色背景)
```

### 响应式策略

- 断点：使用 CSS Grid 自动适应
- 元素间距：随视口宽度调整
- 类型大小：保持固定，不随视口缩放

## 8. 模态框

### 模态框结构

```html
<div class="modal">
  <div class="modal-content">
    <div class="modal-header">
      <h2 class="modal-title">标题</h2>
      <button class="modal-close">×</button>
    </div>
    <div class="modal-body">
      <!-- 内容 -->
    </div>
  </div>
</div>
```

### 模态框样式

- 背景遮罩：`rgba(0, 0, 0, 0.5)` + `backdrop-filter: blur(5px)`
- 内容框：90% 宽度，最大 800px
- 边框：2px solid #000
- 内边距：24px
- 标题：Wired Display, 24px
- 关闭按钮：24px，无边框

### 模态框内容样式

```css
.modal-body h2 { /* 标题 */
  font-family: Wired Display;
  font-size: 20px;
  font-weight: 700;
  margin-bottom: 16px;
}

.modal-body h3 { /* 小标题 */
  font-family: Apercu;
  font-size: 16px;
  font-weight: 700;
  margin-top: 24px;
  margin-bottom: 8px;
}

.modal-body p { /* 正文 */
  font-family: Breve Text;
  font-size: 16px;
  line-height: 1.5;
  margin-bottom: 16px;
}

.modal-body pre { /* 代码块 */
  background: #000;
  color: #fff;
  padding: 16px;
  overflow-x: auto;
  font-family: Wired Mono;
  font-size: 12px;
}

.modal-body code { /* 行内代码 */
  font-family: Wired Mono;
  font-size: 13px;
  background: #f8f9fa;
  padding: 2px 6px;
  border: 1px solid #e2e8f0;
}
```

## 9. 交互行为

### 悬停效果

- **颜色变化**：文字颜色变为 Link Blue (#057dbc)
- **边框变化**：边框颜色变为 Link Blue (#057dbc)
- **背景反转**：黑色背景变为白色文字（仅限按钮）
- **过渡时间**：150ms ease

### 无动画效果

- 无缩放
- 无阴影提升
- 无弹跳效果
- 悬停仅改变颜色

## 10. 做得和不做得

### 应该做

- 所有按钮使用 2px 黑色边框，无圆角
- 标签、分类使用大写 Wired Mono
- 正文使用 Breve Text，不使用 Apercu
- 图片使用方角，边框贴合
- 使用边框分隔内容区块，不使用卡片或阴影
- 仅仅对 Footer、丝带、工具栏使用背景反转
- Link Blue (#057dbc) 仅用于悬停状态，不作为背景或按钮填充

### 不应该做

- 任何元素添加 box-shadow
- 圆角（非零 border-radius），除非是圆形图标按钮
- 混用字体角色：Wired Display 不用于正文，Breve Text 不用于按钮
- 使用除灰度 + #057dbc 之外的颜色
- Apercu 小写用于分类标签 — 必须大写
- 使用渐变、模糊、玻璃拟态效果
- 依赖悬停提升效果 — 仅改变颜色

## 11. 实现参考

### CSS 变量

```css
:root {
  /* Color Palette */
  --wired-black: #000000;
  --page-ink: #1a1a1a;
  --paper-white: #ffffff;
  --link-blue: #057dbc;
  --caption-gray: #757575;
  --hairline-tint: #e2e8f0;
  --panel-light: #f8f9fa;

  /* Typography */
  --font-wired-display: 'Helvetica', 'Arial', sans-serif;
  --font-breve-text: 'Helvetica', 'Arial', sans-serif;
  --font-apercu: 'Helvetica', 'Arial', sans-serif;
  --font-wired-mono: 'Courier New', 'Monaco', monospace;

  /* Spacing */
  --base-unit: 8px;
  --spacing-xs: 4px;
  --spacing-sm: 8px;
  --spacing-md: 16px;
  --spacing-lg: 24px;
  --spacing-xl: 32px;
  --spacing-xxl: 48px;

  /* Border */
  --border-hard: 2px solid var(--wired-black);
  --border-hairline: 1px solid var(--hairline-tint);
  --border-editorial: 1px solid var(--wired-black);
}
```

### 快速参考

- Primary Ink：`WIRED Black (#000000)`
- Page Canvas：`Paper White (#ffffff)`
- Headline / Body Text：`Page Ink (#1a1a1a)`
- Caption / Metadata：`Caption Gray (#757575)`
- Hairline / Quiet Border：`Hairline Tint (#e2e8f0)`
- Link Hover Accent：`Link Blue (#057dbc)`

## 12. 附录：与 WIRED 设计系统的差异

本设计系统基于 WIRED 风格，但针对 starletto 项目进行了以下调整：

1. **字体**：使用系统字体替代 WIRED 专用字体
2. **色彩**：保留 WIRED 的核心色彩体系
3. **布局**：根据 starletto 内容需求调整区块结构
4. **组件**：保留核心组件设计，增加适合技术文档的组件

主要原则保持一致：无阴影、无圆角、严格的排版控制、极简的色彩系统。
