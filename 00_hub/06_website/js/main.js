document.addEventListener('DOMContentLoaded', function() {
    initLogoClick();
    initFooterClick();
    initNavigationModals();
    initSimulationCanvas();
    initCategoryFilter();
    initDocModal();
    initScrollAnimations();
    initHorizontalScroll();
});

// Logo 点击返回顶部
function initLogoClick() {
    const logo = document.getElementById('logo');
    if (!logo) return;
    
    logo.addEventListener('click', function() {
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
    });
}

// 初始化页尾点击事件
function initFooterClick() {
    const companyLogo = document.getElementById('company-logo');
    const productLogo = document.getElementById('product-logo');
    
    // 点击公司名返回顶部
    if (companyLogo) {
        companyLogo.addEventListener('click', function() {
            window.scrollTo({
                top: 0,
                behavior: 'smooth'
            });
        });
    }
    
    // 点击产品名返回顶部
    if (productLogo) {
        productLogo.addEventListener('click', function() {
            window.scrollTo({
                top: 0,
                behavior: 'smooth'
            });
        });
    }
}

// 初始化导航弹窗
function initNavigationModals() {
    const modal = document.getElementById('docModal');
    const modalTitle = document.getElementById('modalTitle');
    const modalBody = document.getElementById('modalBody');
    const modalClose = document.getElementById('modalClose');
    
    // 导航内容
    const navContent = {
        '什么是': `
            <h2>什么是 starletto？</h2>
            <p>starletto 是一个高性能、模块化的自动驾驶系统框架，为开发者提供完整的感知、规划、控制解决方案。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">高性能</div>
                    <div class="card-content">采用现代 C++ 架构，支持单进程零拷贝和跨进程共享内存传输</div>
                </div>
                <div class="card">
                    <div class="card-title">模块化</div>
                    <div class="card-content">清晰的模块划分，便于扩展和维护</div>
                </div>
                <div class="card">
                    <div class="card-title">完整解决方案</div>
                    <div class="card-content">从感知到控制的全栈自动驾驶功能</div>
                </div>
                <div class="card">
                    <div class="card-title">开源开放</div>
                    <div class="card-content">基于 MIT 许可证，欢迎社区贡献</div>
                </div>
            </div>
            
            <h3>技术栈</h3>
            <div class="icon-list">
                <div class="icon-item">
                    <div class="icon-item-icon">⚙️</div>
                    <div class="icon-item-text">C++17/20</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🔨</div>
                    <div class="icon-item-text">CMake</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">📦</div>
                    <div class="icon-item-text">Conan</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">📄</div>
                    <div class="icon-item-text">Protobuf</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🖥️</div>
                    <div class="icon-item-text">Web UI</div>
                </div>
            </div>
        `,
        '关于我们': `
            <h2>关于我们</h2>
            <p>我们是一个专注于自动驾驶技术的开源团队，致力于打造最先进的自动驾驶系统框架。</p>
            
            <h3>团队愿景</h3>
            <p>通过开源协作，推动自动驾驶技术的发展，让智能驾驶技术惠及更多人。</p>
            
            <h3>核心价值观</h3>
            <div class="card-container">
                <div class="card">
                    <div class="card-title">技术创新</div>
                    <div class="card-content">不断探索前沿技术，推动行业发展</div>
                </div>
                <div class="card">
                    <div class="card-title">开放协作</div>
                    <div class="card-content">拥抱开源文化，与社区共同成长</div>
                </div>
                <div class="card">
                    <div class="card-title">品质至上</div>
                    <div class="card-content">追求代码质量和系统可靠性</div>
                </div>
                <div class="card">
                    <div class="card-title">用户导向</div>
                    <div class="card-content">以开发者需求为中心，提供优质工具</div>
                </div>
            </div>
            
            <h3>联系方式</h3>
            <div class="icon-list">
                <div class="icon-item">
                    <div class="icon-item-icon">🐱</div>
                    <div class="icon-item-text">GitHub</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">✉️</div>
                    <div class="icon-item-text">Email</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">💬</div>
                    <div class="icon-item-text">Discord</div>
                </div>
            </div>
        `,
        '登录': `
            <h2>登录</h2>
            <p>请登录您的 starletto 账户以访问更多功能。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">开发者账户</div>
                    <div class="card-content">访问完整的开发工具和资源</div>
                </div>
                <div class="card">
                    <div class="card-title">企业账户</div>
                    <div class="card-content">获取企业级支持和服务</div>
                </div>
            </div>
            
            <h3>登录表单</h3>
            <form style="max-width: 400px; margin: 0 auto;">
                <div style="margin-bottom: var(--spacing-md);">
                    <label style="display: block; margin-bottom: var(--spacing-xs); font-family: var(--font-apercu); font-size: 14px; font-weight: 700;">邮箱</label>
                    <input type="email" style="width: 100%; padding: var(--spacing-sm); border: var(--border-editorial); font-family: var(--font-breve-text); font-size: 14px;">
                </div>
                <div style="margin-bottom: var(--spacing-md);">
                    <label style="display: block; margin-bottom: var(--spacing-xs); font-family: var(--font-apercu); font-size: 14px; font-weight: 700;">密码</label>
                    <input type="password" style="width: 100%; padding: var(--spacing-sm); border: var(--border-editorial); font-family: var(--font-breve-text); font-size: 14px;">
                </div>
                <div style="margin-bottom: var(--spacing-md);">
                    <input type="checkbox" id="remember" style="margin-right: var(--spacing-sm);">
                    <label for="remember" style="font-family: var(--font-breve-text); font-size: 14px;">记住我</label>
                </div>
                <button type="submit" style="background: var(--wired-black); color: var(--paper-white); border: var(--border-hard); padding: var(--spacing-sm) var(--spacing-md); font-family: var(--font-apercu); font-size: 14px; font-weight: 700; cursor: pointer; width: 100%;">登录</button>
            </form>
            <p style="margin-top: var(--spacing-md); text-align: center; font-family: var(--font-breve-text); font-size: 14px;">
                还没有账户？<a href="#" style="color: var(--link-blue); text-decoration: none;">注册</a>
            </p>
        `,
        '条款': `
            <h2>服务条款</h2>
            <p>欢迎使用 starletto 服务。以下是使用我们服务的条款和条件。</p>
            
            <div class="chart-container">
                <div class="chart-title">服务使用统计</div>
                <div class="chart-placeholder">[使用统计图表]</div>
            </div>
            
            <h3>1. 接受条款</h3>
            <p>通过访问或使用 starletto 服务，您同意遵守本条款。如果您不同意这些条款，请不要使用我们的服务。</p>
            
            <h3>2. 使用许可</h3>
            <p>starletto 授予您非独占、不可转让的许可，以使用我们的服务用于个人或商业目的。</p>
            
            <h3>3. 用户责任</h3>
            <ul>
                <li>您负责维护您账户的安全性</li>
                <li>您同意不使用我们的服务进行任何非法活动</li>
                <li>您同意不侵犯他人的知识产权</li>
            </ul>
            
            <h3>4. 知识产权</h3>
            <p>starletto 及其相关服务的所有知识产权归 starletto 团队所有。</p>
            
            <h3>5. 免责声明</h3>
            <p>starletto 服务按"原样"提供，不提供任何明示或暗示的保证。</p>
        `,
        '隐私': `
            <h2>隐私政策</h2>
            <p>starletto 重视您的隐私。本隐私政策解释了我们如何收集、使用和保护您的个人信息。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">个人信息</div>
                    <div class="card-content">当您注册账户时，我们可能收集您的姓名、邮箱等信息</div>
                </div>
                <div class="card">
                    <div class="card-title">使用数据</div>
                    <div class="card-content">我们可能收集您使用我们服务的方式和频率</div>
                </div>
                <div class="card">
                    <div class="card-title">技术数据</div>
                    <div class="card-content">我们可能收集设备信息、IP 地址等技术数据</div>
                </div>
            </div>
            
            <h3>使用信息</h3>
            <p>我们使用收集的信息来：</p>
            <ul>
                <li>提供和维护我们的服务</li>
                <li>改进和个性化我们的服务</li>
                <li>与您沟通</li>
                <li>分析使用情况</li>
            </ul>
            
            <h3>信息共享</h3>
            <p>我们不会向第三方共享您的个人信息，除非：</p>
            <ul>
                <li>获得您的明确许可</li>
                <li>遵守法律法规</li>
                <li>保护我们的权利和财产</li>
            </ul>
        `
    };
    
    // 绑定点击事件
    const navLinks = {
        'what-is': '什么是',
        'about-us': '关于我们',
        'login': '登录',
        'terms': '条款',
        'privacy': '隐私'
    };
    
    Object.entries(navLinks).forEach(([id, title]) => {
        const element = document.getElementById(id);
        if (element) {
            element.addEventListener('click', function() {
                modalTitle.textContent = title;
                modalBody.innerHTML = navContent[title];
                modal.classList.add('active');
                
                // 不滚动到页面顶部，保持当前位置
            });
        }
    });
    
    // 关闭模态框
    modalClose.addEventListener('click', function() {
        modal.classList.remove('active');
    });
    
    // 点击背景关闭
    modal.addEventListener('click', function(e) {
        if (e.target === modal) {
            modal.classList.remove('active');
        }
    });
}

// 初始化规划界面
function initSimulationCanvas() {
    const canvas = document.getElementById('simulationCanvas');
    if (!canvas) return;
    
    // 确保canvas尺寸正确
    canvas.width = canvas.offsetWidth;
    canvas.height = 200;
    
    // 创建PathPlanner实例
    const planner = new PathPlanner(canvas);
    
    // 启动动画循环
    planner.startAnimation();
}

// 绘制坐标轴系统（无文字）
function drawCoordinateSystem(ctx, canvas) {
    const padding = 40;
    const width = canvas.width - 2 * padding;
    const height = canvas.height - 2 * padding;
    
    // 绘制坐标轴
    ctx.strokeStyle = '#1a1a1a';
    ctx.lineWidth = 2;
    
    // X轴 (s)
    ctx.beginPath();
    ctx.moveTo(padding, canvas.height - padding);
    ctx.lineTo(canvas.width - padding, canvas.height - padding);
    ctx.stroke();
    
    // Y轴 (l)
    ctx.beginPath();
    ctx.moveTo(padding, padding);
    ctx.lineTo(padding, canvas.height - padding);
    ctx.stroke();
    
    // 绘制刻度（无数字）
    ctx.lineWidth = 1;
    const xStep = width / 10;
    const yStep = height / 10;
    
    // X轴刻度
    for (let i = 0; i <= 10; i++) {
        const x = padding + i * xStep;
        ctx.beginPath();
        ctx.moveTo(x, canvas.height - padding);
        ctx.lineTo(x, canvas.height - padding + 5);
        ctx.stroke();
    }
    
    // Y轴刻度
    for (let i = 0; i <= 10; i++) {
        const y = canvas.height - padding - i * yStep;
        ctx.beginPath();
        ctx.moveTo(padding - 5, y);
        ctx.lineTo(padding, y);
        ctx.stroke();
    }
}

// 绘制障碍物（从外侧挤压边界）
function drawObstacles(ctx, canvas) {
    const padding = 40;
    const width = canvas.width - 2 * padding;
    const height = canvas.height - 2 * padding;
    
    // 绘制矩形障碍物
    ctx.fillStyle = 'rgba(255, 0, 0, 0.2)';
    ctx.strokeStyle = '#ff0000';
    ctx.lineWidth = 2;
    
    // 障碍物从外侧挤压边界
    const obstacleX = padding + width * 0.35;
    const obstacleY = padding + height * 0.1;
    const obstacleWidth = width * 0.3;
    const obstacleHeight = height * 0.8;
    
    ctx.fillRect(obstacleX, obstacleY, obstacleWidth, obstacleHeight);
    ctx.strokeRect(obstacleX, obstacleY, obstacleWidth, obstacleHeight);
}

// 绘制边界（被障碍物挤压）
function drawBounds(ctx, canvas) {
    const padding = 40;
    const width = canvas.width - 2 * padding;
    const height = canvas.height - 2 * padding;
    
    // 绘制上下边界
    ctx.strokeStyle = '#057dbc';
    ctx.lineWidth = 2;
    ctx.setLineDash([5, 5]);
    
    // 上边界 (upper bound) - 被障碍物挤压
    ctx.beginPath();
    ctx.moveTo(padding, padding + height * 0.2);
    ctx.lineTo(padding + width * 0.35, padding + height * 0.2);
    // 被障碍物挤压
    ctx.lineTo(padding + width * 0.35, padding + height * 0.1);
    ctx.lineTo(padding + width * 0.65, padding + height * 0.1);
    // 恢复正常
    ctx.lineTo(padding + width, padding + height * 0.2);
    ctx.stroke();
    
    // 下边界 (lower bound) - 被障碍物挤压
    ctx.beginPath();
    ctx.moveTo(padding, padding + height * 0.8);
    ctx.lineTo(padding + width * 0.35, padding + height * 0.8);
    // 被障碍物挤压
    ctx.lineTo(padding + width * 0.35, padding + height * 0.9);
    ctx.lineTo(padding + width * 0.65, padding + height * 0.9);
    // 恢复正常
    ctx.lineTo(padding + width, padding + height * 0.8);
    ctx.stroke();
    ctx.setLineDash([]);
}

// 绘制自车
function drawEgoVehicle(ctx, canvas) {
    const padding = 40;
    const width = canvas.width - 2 * padding;
    const height = canvas.height - 2 * padding;
    
    // 自车位置
    const carX = padding + width * 0.2;
    const carY = padding + height * 0.5;
    
    // 绘制车box
    ctx.strokeStyle = '#1a1a1a';
    ctx.lineWidth = 2;
    ctx.strokeRect(carX - 15, carY - 8, 30, 16);
    
    // 绘制车头
    ctx.beginPath();
    ctx.moveTo(carX + 15, carY - 8);
    ctx.lineTo(carX + 20, carY);
    ctx.lineTo(carX + 15, carY + 8);
    ctx.closePath();
    ctx.fillStyle = '#1a1a1a';
    ctx.fill();
    
    // 绘制车轮
    ctx.fillStyle = '#1a1a1a';
    ctx.beginPath();
    ctx.arc(carX - 10, carY - 8, 3, 0, Math.PI * 2);
    ctx.fill();
    ctx.beginPath();
    ctx.arc(carX + 10, carY - 8, 3, 0, Math.PI * 2);
    ctx.fill();
    ctx.beginPath();
    ctx.arc(carX - 10, carY + 8, 3, 0, Math.PI * 2);
    ctx.fill();
    ctx.beginPath();
    ctx.arc(carX + 10, carY + 8, 3, 0, Math.PI * 2);
    ctx.fill();
    
    return carX;
}

// 绘制轨迹（从车底下向右延伸，速度从低到高再到低）
function drawTrajectory(ctx, canvas, carX) {
    const padding = 40;
    const width = canvas.width - 2 * padding;
    const height = canvas.height - 2 * padding;
    
    // 绘制主轨迹线
    ctx.strokeStyle = '#057dbc';
    ctx.lineWidth = 3;
    
    ctx.beginPath();
    ctx.moveTo(carX, padding + height * 0.5);
    
    // 生成平滑轨迹
    for (let i = 0; i <= width - (carX - padding); i += 10) {
        const x = carX + i;
        const t = i / (width - (carX - padding));
        // 轨迹在障碍物处绕行
        let y;
        if (t > 0.15 && t < 0.45) {
            // 绕过障碍物
            const offset = Math.sin((t - 0.15) / 0.3 * Math.PI) * height * 0.25;
            y = padding + height * 0.5 + offset;
        } else {
            // 正常轨迹
            y = padding + height * 0.5 + Math.sin(t * 2 * Math.PI) * height * 0.05;
        }
        ctx.lineTo(x, y);
    }
    
    ctx.stroke();
    
    // 绘制速度变化（从低到高再到低）
    ctx.lineWidth = 1;
    
    for (let i = 0; i <= width - (carX - padding); i += 5) {
        const x = carX + i;
        const t = i / (width - (carX - padding));
        
        // 速度从低到高再到低
        let speed;
        if (t < 0.3) {
            // 加速
            speed = t / 0.3;
        } else if (t < 0.7) {
            // 减速
            speed = 1 - (t - 0.3) / 0.4;
        } else {
            // 再次加速
            speed = (t - 0.7) / 0.3;
        }
        
        // 根据速度设置线的高度
        const lineHeight = speed * 10;
        
        // 速度颜色变化
        const alpha = 0.5 + speed * 0.5;
        ctx.strokeStyle = `rgba(5, 125, 188, ${alpha})`;
        
        // 计算当前点的y坐标
        let y;
        if (t > 0.15 && t < 0.45) {
            const offset = Math.sin((t - 0.15) / 0.3 * Math.PI) * height * 0.25;
            y = padding + height * 0.5 + offset;
        } else {
            y = padding + height * 0.5 + Math.sin(t * 2 * Math.PI) * height * 0.05;
        }
        
        // 绘制速度线
        ctx.beginPath();
        ctx.moveTo(x, y - lineHeight / 2);
        ctx.lineTo(x, y + lineHeight / 2);
        ctx.stroke();
    }
}

// 初始化分类筛选
function initCategoryFilter() {
    const categoryItems = document.querySelectorAll('.category-item');
    const docItems = document.querySelectorAll('.doc-item');
    
    categoryItems.forEach(item => {
        item.addEventListener('click', function() {
            // 移除所有激活状态
            categoryItems.forEach(i => i.classList.remove('active'));
            // 添加当前激活状态
            this.classList.add('active');
            
            const category = this.dataset.category;
            
            // 筛选文档
            docItems.forEach(doc => {
                if (category === 'all' || doc.dataset.category === category) {
                    doc.style.display = 'block';
                } else {
                    doc.style.display = 'none';
                }
            });
        });
    });
}

// 初始化文档模态框
function initDocModal() {
    const modal = document.getElementById('docModal');
    const modalTitle = document.getElementById('modalTitle');
    const modalBody = document.getElementById('modalBody');
    const modalClose = document.getElementById('modalClose');
    const docItems = document.querySelectorAll('.doc-item');
    
    // 文档内容数据
    const docContent = {
        '系统架构设计': `
            <h2>系统架构设计</h2>
            <p>NeonDrive 采用分层架构设计，包括以下核心层次：</p>
            
            <div class="architecture-diagram">
                <div class="diagram-placeholder">[系统架构图]</div>
            </div>
            
            <h3>1. Schema 层</h3>
            <p>定义数据结构和接口规范，确保数据一致性和类型安全。通过 .schema 文件定义数据模式，自动生成 C++ 数据结构，保证类型安全和序列化效率。</p>
            
            <h3>2. Transport 层</h3>
            <p>负责模块间通信，支持单进程零拷贝和跨进程共享内存传输。提供统一的 Channel 接口，实现高效的进程间通信。</p>
            
            <h3>3. Runtime 层</h3>
            <p>提供进程管理、调度器、触发器等运行时基础设施。保障系统稳定运行，提供实时性保障和故障恢复机制。</p>
            
            <h3>4. Application 层</h3>
            <p>包含感知、规划、控制等核心算法模块。实现完整的自动驾驶功能，包括环境感知、路径规划、车辆控制等。</p>
            
            <h3>5. Tools 层</h3>
            <p>提供开发工具、可视化、数据记录和回放等功能。辅助开发者进行系统调试和性能优化。</p>
            
            <div class="chart-container">
                <div class="chart-title">模块性能对比</div>
                <div class="chart-placeholder">[性能对比图表]</div>
            </div>
        `,
        '感知模块设计': `
            <h2>感知模块设计</h2>
            <p>感知模块是自动驾驶系统的眼睛，负责理解周围环境。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">多传感器融合</div>
                    <div class="card-content">激光雷达、摄像头、毫米波雷达数据融合</div>
                </div>
                <div class="card">
                    <div class="card-title">目标检测</div>
                    <div class="card-content">实时检测道路上的车辆、行人、障碍物</div>
                </div>
                <div class="card">
                    <div class="card-title">语义分割</div>
                    <div class="card-content">理解道路场景，识别车道线、交通标志</div>
                </div>
                <div class="card">
                    <div class="card-title">环境建模</div>
                    <div class="card-content">构建周围环境的 3D 模型</div>
                </div>
            </div>
            
            <h3>技术特点</h3>
            <ul>
                <li>实时处理，延迟低于 10ms</li>
                <li>高精度目标定位和分类</li>
                <li>鲁棒的传感器融合算法</li>
                <li>自适应环境感知能力</li>
                <li>支持多种传感器配置</li>
            </ul>
            
            <div class="icon-list">
                <div class="icon-item">
                    <div class="icon-item-icon">📷</div>
                    <div class="icon-item-text">摄像头</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">📡</div>
                    <div class="icon-item-text">激光雷达</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">📳</div>
                    <div class="icon-item-text">毫米波雷达</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🔊</div>
                    <div class="icon-item-text">超声波</div>
                </div>
            </div>
        `,
        '路径规划算法': `
            <h2>路径规划算法</h2>
            <p>路径规划模块负责为车辆规划安全、高效的行驶路径。</p>
            
            <div class="architecture-diagram">
                <div class="diagram-placeholder">[路径规划流程图]</div>
            </div>
            
            <h3>规划层次</h3>
            <ul>
                <li><strong>全局路径规划</strong>：基于地图的长距离路径规划，考虑道路网络和交通规则</li>
                <li><strong>局部路径规划</strong>：实时避障和轨迹优化，考虑动态环境</li>
                <li><strong>行为规划</strong>：决策车辆行为（如变道、超车、停车等）</li>
            </ul>
            
            <div class="chart-container">
                <div class="chart-title">规划算法性能</div>
                <div class="chart-placeholder">[算法性能对比图表]</div>
            </div>
            
            <h3>算法特点</h3>
            <ul>
                <li>考虑交通规则和道路约束</li>
                <li>支持动态环境和未知障碍物</li>
                <li>优化行驶舒适度和效率</li>
                <li>实时响应，规划周期低于 50ms</li>
                <li>支持多场景适配</li>
            </ul>
        `,
        '仿真系统使用指南': `
            <h2>仿真系统使用指南</h2>
            <p>NeonDrive 提供了功能强大的仿真系统，用于算法测试和验证。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">场景模拟</div>
                    <div class="card-content">创建各种驾驶场景进行测试</div>
                </div>
                <div class="card">
                    <div class="card-title">传感器模拟</div>
                    <div class="card-content">模拟各种传感器的数据输出</div>
                </div>
                <div class="card">
                    <div class="card-title">车辆动力学</div>
                    <div class="card-content">模拟真实车辆的物理行为</div>
                </div>
                <div class="card">
                    <div class="card-title">数据记录</div>
                    <div class="card-content">记录和回放仿真数据</div>
                </div>
            </div>
            
            <h3>使用方法</h3>
            <ol>
                <li><strong>创建场景</strong>：使用场景编辑器创建或加载预设场景</li>
                <li><strong>配置车辆</strong>：选择车辆模型和传感器配置</li>
                <li><strong>设置参数</strong>：调整仿真参数和环境条件</li>
                <li><strong>运行仿真</strong>：启动仿真并观察系统行为</li>
                <li><strong>分析结果</strong>：查看仿真数据和性能指标</li>
                <li><strong>优化算法</strong>：基于仿真结果调整算法参数</li>
            </ol>
            
            <div class="icon-list">
                <div class="icon-item">
                    <div class="icon-item-icon">🏙️</div>
                    <div class="icon-item-text">城市场景</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🛣️</div>
                    <div class="icon-item-text">高速场景</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🌧️</div>
                    <div class="icon-item-text">雨天场景</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🌫️</div>
                    <div class="icon-item-text">雾天场景</div>
                </div>
            </div>
        `,
        '系统部署指南': `
            <h2>系统部署指南</h2>
            <p>本指南介绍如何在不同环境中部署 NeonDrive 系统。</p>
            
            <div class="chart-container">
                <div class="chart-title">硬件需求对比</div>
                <div class="chart-placeholder">[硬件需求图表]</div>
            </div>
            
            <h3>硬件要求</h3>
            <ul>
                <li><strong>CPU</strong>：至少 8 核处理器，推荐 Intel i7 或 AMD Ryzen 7</li>
                <li><strong>内存</strong>：至少 16GB RAM，推荐 32GB</li>
                <li><strong>GPU</strong>：推荐 NVIDIA RTX 系列，用于深度学习加速</li>
                <li><strong>存储</strong>：至少 100GB 固态硬盘</li>
            </ul>
            
            <h3>软件要求</h3>
            <ul>
                <li><strong>操作系统</strong>：Ubuntu 20.04+</li>
                <li><strong>依赖库</strong>：CUDA 11.0+, OpenCV 4.5+, Eigen 3.3+</li>
                <li><strong>编译工具</strong>：CMake 3.16+，GCC 9+</li>
            </ul>
            
            <h3>部署步骤</h3>
            <pre>
# 步骤 1：安装依赖
sudo apt update
sudo apt install build-essential cmake git

# 步骤 2：克隆代码
git clone https://github.com/neondrive/neondrive.git
cd neondrive

# 步骤 3：编译系统
mkdir build && cd build
cmake ..
make -j8

# 步骤 4：配置传感器
./neon config sensors

# 步骤 5：启动系统
./neon run
            </pre>
        `,
        'API 参考文档': `
            <h2>API 参考文档</h2>
            <p>NeonDrive 提供了丰富的 API 接口，用于模块间通信和系统集成。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">Transport API</div>
                    <div class="card-content">模块间通信接口</div>
                </div>
                <div class="card">
                    <div class="card-title">Module API</div>
                    <div class="card-content">模块管理和生命周期</div>
                </div>
                <div class="card">
                    <div class="card-title">Data API</div>
                    <div class="card-content">数据结构和序列化</div>
                </div>
                <div class="card">
                    <div class="card-title">Config API</div>
                    <div class="card-content">配置管理接口</div>
                </div>
            </div>
            
            <h3>使用示例</h3>
            <pre>
// 创建感知模块
auto perception = CreateModule<PerceptionModule>("perception");

// 创建 Channel
auto channel = Transport::CreateChannel<PerceptionOutput>("/perception/output");

// 创建 Writer
auto writer = channel->CreateWriter();

// 发布感知结果
PerceptionOutput output;
output.objects = DetectObjects();
writer->Write(output);
            </pre>
            
            <h3>API 版本</h3>
            <p>当前 API 版本：v1.0.0</p>
            <p>API 文档更新日期：2026-04-14</p>
        `,
        '快速开始示例': `
            <h2>快速开始示例</h2>
            <p>本示例将帮助您快速上手 NeonDrive 系统。</p>
            
            <div class="icon-list">
                <div class="icon-item">
                    <div class="icon-item-icon">🚀</div>
                    <div class="icon-item-text">快速开始</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">📚</div>
                    <div class="icon-item-text">文档</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">💡</div>
                    <div class="icon-item-text">示例</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">❓</div>
                    <div class="icon-item-text">帮助</div>
                </div>
            </div>
            
            <h3>步骤 1：安装依赖</h3>
            <pre>
sudo apt update
sudo apt install build-essential cmake git
            </pre>
            
            <h3>步骤 2：克隆代码</h3>
            <pre>
git clone https://github.com/neondrive/neondrive.git
cd neondrive
            </pre>
            
            <h3>步骤 3：编译系统</h3>
            <pre>
mkdir build && cd build
cmake ..
make -j8
            </pre>
            
            <h3>步骤 4：运行示例</h3>
            <pre>
./neon run example
            </pre>
            
            <h3>步骤 5：查看结果</h3>
            <p>打开浏览器访问 http://localhost:8080 查看系统状态和可视化结果。</p>
            
            <div class="chart-container">
                <div class="chart-title">系统启动时间</div>
                <div class="chart-placeholder">[启动时间图表]</div>
            </div>
        `,
        '常见问题解答': `
            <h2>常见问题解答</h2>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">系统启动</div>
                    <div class="card-content">解决系统启动失败的问题</div>
                </div>
                <div class="card">
                    <div class="card-title">性能优化</div>
                    <div class="card-content">提升系统性能的方法</div>
                </div>
                <div class="card">
                    <div class="card-title">传感器配置</div>
                    <div class="card-content">正确配置传感器的步骤</div>
                </div>
                <div class="card">
                    <div class="card-title">故障排除</div>
                    <div class="card-content">常见故障的解决方法</div>
                </div>
            </div>
            
            <h3>Q: 系统启动失败怎么办？</h3>
            <p>A: 检查日志文件，确认依赖项是否安装正确，传感器是否连接正常。</p>
            
            <h3>Q: 感知模块延迟过高如何解决？</h3>
            <p>A: 检查硬件性能，优化算法参数，考虑使用 GPU 加速。</p>
            
            <h3>Q: 如何添加新的传感器？</h3>
            <p>A: 在配置文件中添加传感器参数，实现相应的驱动接口。</p>
            
            <h3>Q: 系统支持哪些车型？</h3>
            <p>A: NeonDrive 支持多种车型，包括轿车、SUV、卡车等，可通过配置文件适配不同车型。</p>
            
            <h3>Q: 如何贡献代码？</h3>
            <p>A: Fork 代码仓库，创建分支，提交 PR，我们会及时 review 和合并。</p>
        `,
        '数据流程设计': `
            <h2>数据流程设计</h2>
            <p>NeonDrive 系统的数据流程设计，包括数据采集、处理和分发机制。</p>
            
            <div class="architecture-diagram">
                <div class="diagram-placeholder">[数据流程图]</div>
            </div>
            
            <h3>数据采集</h3>
            <ul>
                <li>传感器数据采集：激光雷达、摄像头、毫米波雷达等</li>
                <li>车辆状态数据：速度、加速度、方向盘角度等</li>
                <li>地图数据：高精度地图、实时交通信息等</li>
                <li>环境数据：天气、路况、交通流量等</li>
            </ul>
            
            <h3>数据处理</h3>
            <ul>
                <li>数据预处理：去噪、校准、时间同步</li>
                <li>特征提取：从原始数据中提取有用信息</li>
                <li>数据融合：多传感器数据融合</li>
                <li>数据压缩：减少数据传输带宽</li>
            </ul>
            
            <div class="chart-container">
                <div class="chart-title">数据处理延迟</div>
                <div class="chart-placeholder">[数据处理延迟图表]</div>
            </div>
        `,
        '定位模块设计': `
            <h2>定位模块设计</h2>
            <p>高精度定位模块的详细设计，包括多源融合定位算法。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">GNSS 定位</div>
                    <div class="card-content">全球卫星导航系统定位</div>
                </div>
                <div class="card">
                    <div class="card-title">IMU 融合</div>
                    <div class="card-content">惯性测量单元数据融合</div>
                </div>
                <div class="card">
                    <div class="card-title">激光 SLAM</div>
                    <div class="card-content">基于激光雷达的定位</div>
                </div>
                <div class="card">
                    <div class="card-title">视觉 SLAM</div>
                    <div class="card-content">基于摄像头的定位</div>
                </div>
            </div>
            
            <h3>核心功能</h3>
            <ul>
                <li>GNSS 定位：全球卫星导航系统</li>
                <li>IMU 融合：惯性测量单元数据融合</li>
                <li>激光 SLAM：基于激光雷达的同时定位与地图构建</li>
                <li>视觉 SLAM：基于摄像头的同时定位与地图构建</li>
                <li>地图匹配：与高精度地图匹配</li>
                <li>定位精度评估：实时评估定位精度</li>
            </ul>
            
            <div class="chart-container">
                <div class="chart-title">定位精度对比</div>
                <div class="chart-placeholder">[定位精度对比图表]</div>
            </div>
        `,
        '控制算法': `
            <h2>控制算法</h2>
            <p>车辆控制算法的详细说明，包括 PID、MPC 等控制策略。</p>
            
            <div class="architecture-diagram">
                <div class="diagram-placeholder">[控制算法流程图]</div>
            </div>
            
            <h3>控制策略</h3>
            <ul>
                <li><strong>PID 控制</strong>：比例-积分-微分控制</li>
                <li><strong>MPC 控制</strong>：模型预测控制</li>
                <li><strong>LQR 控制</strong>：线性二次调节器</li>
                <li><strong>滑模控制</strong>：鲁棒控制策略</li>
                <li><strong>自适应控制</strong>：适应不同车辆状态</li>
            </ul>
            
            <h3>控制目标</h3>
            <ul>
                <li>轨迹跟踪：精确跟踪规划轨迹</li>
                <li>稳定性：保证车辆稳定行驶</li>
                <li>舒适性：提供平滑的驾驶体验</li>
                <li>安全性：避免危险操作</li>
                <li>燃油经济性：优化能源消耗</li>
            </ul>
            
            <div class="chart-container">
                <div class="chart-title">控制算法性能</div>
                <div class="chart-placeholder">[控制算法性能对比图表]</div>
            </div>
        `,
        '场景编辑器使用': `
            <h2>场景编辑器使用</h2>
            <p>NeonDrive 场景编辑器的使用方法，包括场景创建和编辑。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">场景创建</div>
                    <div class="card-content">从零开始创建新场景</div>
                </div>
                <div class="card">
                    <div class="card-title">场景编辑</div>
                    <div class="card-content">修改现有场景</div>
                </div>
                <div class="card">
                    <div class="card-title">元素添加</div>
                    <div class="card-content">添加道路、建筑、车辆等</div>
                </div>
                <div class="card">
                    <div class="card-title">参数调整</div>
                    <div class="card-content">调整环境参数和物理属性</div>
                </div>
            </div>
            
            <h3>使用步骤</h3>
            <ol>
                <li>启动场景编辑器：<code>./neon editor</code></li>
                <li>选择场景模板或创建新场景</li>
                <li>添加道路和环境元素</li>
                <li>添加动态元素（车辆、行人等）</li>
                <li>设置场景参数（天气、时间等）</li>
                <li>保存场景配置</li>
                <li>在仿真中使用场景</li>
            </ol>
            
            <div class="icon-list">
                <div class="icon-item">
                    <div class="icon-item-icon">🏙️</div>
                    <div class="icon-item-text">城市场景</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🛣️</div>
                    <div class="icon-item-text">高速场景</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🏞️</div>
                    <div class="icon-item-text">乡村场景</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🚧</div>
                    <div class="icon-item-text">施工场景</div>
                </div>
            </div>
        `,
        '预测模块设计': `
            <h2>预测模块设计</h2>
            <p>障碍物行为预测模块的详细设计，包括轨迹预测和意图识别。</p>
            
            <div class="architecture-diagram">
                <div class="diagram-placeholder">[预测模块架构图]</div>
            </div>
            
            <h3>核心功能</h3>
            <ul>
                <li>车辆轨迹预测</li>
                <li>行人行为预测</li>
                <li>交通参与者意图识别</li>
                <li>多模态预测融合</li>
                <li>预测不确定性估计</li>
            </ul>
            
            <div class="chart-container">
                <div class="chart-title">预测准确率</div>
                <div class="chart-placeholder">[预测准确率图表]</div>
            </div>
        `,
        '机器学习模型': `
            <h2>机器学习模型</h2>
            <p>NeonDrive 中使用的机器学习模型，包括目标检测、语义分割等。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">目标检测</div>
                    <div class="card-content">YOLO, Faster R-CNN 等模型</div>
                </div>
                <div class="card">
                    <div class="card-title">语义分割</div>
                    <div class="card-content">DeepLab, U-Net 等模型</div>
                </div>
                <div class="card">
                    <div class="card-title">行为预测</div>
                    <div class="card-content">LSTM, Transformer 等模型</div>
                </div>
                <div class="card">
                    <div class="card-title">传感器融合</div>
                    <div class="card-content">多模态融合模型</div>
                </div>
            </div>
            
            <h3>模型性能</h3>
            <ul>
                <li>实时推理，延迟低于 5ms</li>
                <li>支持模型量化和加速</li>
                <li>自适应模型选择</li>
                <li>在线模型更新</li>
            </ul>
        `,
        '硬件选型指南': `
            <h2>硬件选型指南</h2>
            <p>自动驾驶系统的硬件选型指南，包括传感器、计算平台等。</p>
            
            <div class="chart-container">
                <div class="chart-title">硬件性能对比</div>
                <div class="chart-placeholder">[硬件性能对比图表]</div>
            </div>
            
            <h3>传感器选型</h3>
            <ul>
                <li><strong>激光雷达</strong>：推荐 Hesai Pandar 系列或 Velodyne VLP 系列</li>
                <li><strong>摄像头</strong>：推荐 Sony IMX 系列或 ON Semiconductor AR0234</li>
                <li><strong>毫米波雷达</strong>：推荐 Continental ARS408 或 Bosch LR4</li>
                <li><strong>IMU</strong>：推荐 Xsens MTI 系列或 MicroStrain 3DM</li>
            </ul>
            
            <h3>计算平台</h3>
            <ul>
                <li><strong>车载计算单元</strong>：NVIDIA Drive Xavier 或 Intel Atom</li>
                <li><strong>开发平台</strong>：NVIDIA Jetson Xavier NX 或 Intel NUC</li>
                <li><strong>存储系统</strong>：NVMe SSD 或 eMMC</li>
            </ul>
        `,
        'SDK 开发指南': `
            <h2>SDK 开发指南</h2>
            <p>NeonDrive SDK 的开发指南，包括如何使用 SDK 开发自定义应用。</p>
            
            <div class="card-container">
                <div class="card">
                    <div class="card-title">C++ SDK</div>
                    <div class="card-content">核心开发接口</div>
                </div>
                <div class="card">
                    <div class="card-title">Python SDK</div>
                    <div class="card-content">快速原型开发</div>
                </div>
                <div class="card">
                    <div class="card-title">ROS 接口</div>
                    <div class="card-content">与 ROS 系统集成</div>
                </div>
                <div class="card">
                    <div class="card-title">REST API</div>
                    <div class="card-content">Web 应用集成</div>
                </div>
            </div>
            
            <h3>使用示例</h3>
            <pre>
// C++ SDK 示例
#include &lt;neon/sdk.h&gt;

int main() {
    // 初始化 SDK
    neon::SDK sdk;
    
    // 创建感知模块
    auto perception = sdk.createModule("perception");
    
    // 订阅感知数据
    perception->subscribe([](const neon::PerceptionData& data) {
        // 处理感知数据
        std::cout << "Detected " << data.objects.size() << " objects" << std::endl;
    });
    
    // 运行 SDK
    sdk.run();
    
    return 0;
}
            </pre>
        `,
        '传感器配置示例': `
            <h2>传感器配置示例</h2>
            <p>不同传感器配置的示例，帮助开发者快速配置传感器。</p>
            
            <div class="icon-list">
                <div class="icon-item">
                    <div class="icon-item-icon">📷</div>
                    <div class="icon-item-text">单目相机</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">👁️</div>
                    <div class="icon-item-text">双目相机</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">🔄</div>
                    <div class="icon-item-text">环视相机</div>
                </div>
                <div class="icon-item">
                    <div class="icon-item-icon">📡</div>
                    <div class="icon-item-text">激光雷达</div>
                </div>
            </div>
            
            <h3>配置文件示例</h3>
            <pre>
# 传感器配置文件
sensors:
  cameras:
    - name: front_camera
      type: mono
      resolution: [1920, 1080]
      fps: 30
      topic: /camera/front
  lidars:
    - name: main_lidar
      type: velodyne
      model: VLP-16
      topic: /lidar/main
  radars:
    - name: front_radar
      type: continental
      model: ARS408
      topic: /radar/front
            </pre>
        `,
        '性能优化指南': `
            <h2>性能优化指南</h2>
            <p>NeonDrive 系统的性能优化指南，帮助开发者优化系统性能。</p>
            
            <div class="chart-container">
                <div class="chart-title">性能优化效果</div>
                <div class="chart-placeholder">[性能优化效果图表]</div>
            </div>
            
            <h3>优化策略</h3>
            <ul>
                <li><strong>硬件优化</strong>：选择合适的计算平台，使用 GPU 加速</li>
                <li><strong>算法优化</strong>：优化算法复杂度，使用近似算法</li>
                <li><strong>并行计算</strong>：利用多核心和多线程</li>
                <li><strong>内存优化</strong>：减少内存分配和拷贝</li>
                <li><strong>缓存优化</strong>：提高缓存命中率</li>
            </ul>
            
            <h3>工具链</h3>
            <ul>
                <li><strong>性能分析</strong>：使用 perf、valgrind 等工具</li>
                <li><strong>编译优化</strong>：使用 -O3 优化级别</li>
                <li><strong>内存分析</strong>：使用 address sanitizer</li>
                <li><strong>代码分析</strong>：使用 clang-tidy、cppcheck 等工具</li>
            </ul>
        `
    };
    
    // 打开模态框
    docItems.forEach(item => {
        item.addEventListener('click', function() {
            const title = this.querySelector('.doc-item-title').textContent;
            const content = docContent[title] || '<p>文档内容正在更新中...</p>';
            
            modalTitle.textContent = title;
            modalBody.innerHTML = content;
            modal.classList.add('active');
            
            // 不滚动到页面顶部，保持当前位置
        });
    });
}

// 初始化滚动动画
function initScrollAnimations() {
    const observerOptions = {
        threshold: 0.1,
        rootMargin: '0px 0px -50px 0px'
    };

    const observer = new IntersectionObserver(function(entries) {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.style.opacity = '1';
                entry.target.style.transform = 'translateY(0)';
                observer.unobserve(entry.target);
            }
        });
    }, observerOptions);

    const animatedElements = document.querySelectorAll('.section-left, .section-center, .section-right, .scroll-item, .doc-item');
    animatedElements.forEach(el => {
        el.style.opacity = '0';
        el.style.transform = 'translateY(20px)';
        el.style.transition = 'opacity 0.6s ease, transform 0.6s ease';
        observer.observe(el);
    });
}

// 初始化横向滚动（真正的循环滚动）
function initHorizontalScroll() {
    const scrollContainer = document.querySelector('.scroll-container');
    if (!scrollContainer) return;
    
    let scrollItems = document.querySelectorAll('.scroll-item');
    if (scrollItems.length === 0) return;
    
    let scrollPosition = 0;
    const scrollSpeed = 1;
    
    function animateScroll() {
        scrollPosition += scrollSpeed;
        
        // 当第一个元素完全移出左侧时，将其移动到末尾
        scrollItems = document.querySelectorAll('.scroll-item');
        if (scrollItems.length > 0) {
            const itemWidth = scrollItems[0].offsetWidth;
            if (scrollPosition >= itemWidth) {
                const firstItem = scrollItems[0];
                scrollContainer.appendChild(firstItem);
                scrollPosition -= itemWidth;
            }
        }
        
        scrollContainer.scrollLeft = scrollPosition;
        requestAnimationFrame(animateScroll);
    }
    
    // 启动动画
    animateScroll();
}

// 响应式处理
window.addEventListener('resize', function() {
    initSimulationCanvas();
});

console.log('starletto website initialized successfully');
