// 路径规划模块
class PathPlanner {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.width = canvas.width;
        this.height = 200; // 调整为200px，以适应新的布局
        
        // 配置参数
        this.padding = 40;
        this.gridSize = 10; // 增大网格密度
        this.carWidth = 20; // 2米宽
        this.carLength = 50; // 5米长
        this.obstacleSize = 20; // 2米的小正方形障碍物
        this.safetyMargin = 10; // 1米的挤压距离
        this.obstacleOccupancyMargin = 10; // 障碍物占据网格的额外空间（1米）
        this.obstacles = [
            { x: 200, y: 150, width: this.obstacleSize, height: this.obstacleSize, dragging: false },
            { x: 300, y: 250, width: this.obstacleSize, height: this.obstacleSize, dragging: false }
        ];
        this.path = [];
        this.lastPath = [];
        
        // 坐标转换参数（像素到米）
        this.pixelToMeter = 10; // 10像素 = 1米
        this.meterToPixel = 10; // 1米 = 10像素
        
        // l的上下边界（米）
        this.lLowerBound = -4; // -4米
        this.lUpperBound = 4;  // 4米
        
        // 鼠标事件
        this.mousePos = { x: 0, y: 0 };
        this.setupEventListeners();
        
        // 初始化
        this.updatePath();
        this.startAnimation();
    }
    
    setupEventListeners() {
        this.canvas.addEventListener('mousedown', (e) => this.handleMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.handleMouseUp(e));
        this.canvas.addEventListener('mouseleave', (e) => this.handleMouseUp(e));
        this.canvas.addEventListener('dblclick', (e) => this.handleDoubleClick(e));
    }
    
    handleMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        this.mousePos = {
            x: e.clientX - rect.left,
            y: e.clientY - rect.top
        };
        
        // 检查是否点击到障碍物
        for (let obstacle of this.obstacles) {
            if (this.isPointInRect(this.mousePos, obstacle)) {
                obstacle.dragging = true;
                break;
            }
        }
    }
    
    handleMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        this.mousePos = {
            x: e.clientX - rect.left,
            y: e.clientY - rect.top
        };
        
        // 拖动障碍物
        for (let obstacle of this.obstacles) {
            if (obstacle.dragging) {
                // 计算障碍物中心位置并吸附到网格
                let centerX = Math.round(this.mousePos.x / this.gridSize) * this.gridSize;
                let centerY = Math.round(this.mousePos.y / this.gridSize) * this.gridSize;
                
                // 更新障碍物位置
                obstacle.x = centerX - obstacle.width / 2;
                obstacle.y = centerY - obstacle.height / 2;
                
                // 确保障碍物在画布内
                obstacle.x = Math.max(this.padding, Math.min(this.width - this.padding - obstacle.width, obstacle.x));
                obstacle.y = Math.max(this.padding, Math.min(this.height - this.padding - obstacle.height, obstacle.y));
                
                // 再次吸附到网格，确保障碍物位置对齐网格
                obstacle.x = Math.round(obstacle.x / this.gridSize) * this.gridSize;
                obstacle.y = Math.round(obstacle.y / this.gridSize) * this.gridSize;
                
                // 更新路径
                this.updatePath();
                break;
            }
        }
    }
    
    handleMouseUp(e) {
        for (let obstacle of this.obstacles) {
            obstacle.dragging = false;
        }
        this.draggingObstacle = null;
    }
    
    handleDoubleClick(e) {
        const rect = this.canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;
        
        // 检查是否点击了障碍物
        let clickedObstacle = null;
        for (let i = 0; i < this.obstacles.length; i++) {
            const obstacle = this.obstacles[i];
            if (this.isPointInRect({ x: mouseX, y: mouseY }, obstacle)) {
                clickedObstacle = i;
                break;
            }
        }
        
        if (clickedObstacle !== null) {
            // 双击障碍物，删除该障碍物
            this.obstacles.splice(clickedObstacle, 1);
            this.updatePath();
        } else {
            // 双击网格，在双击的位置增加一个障碍物
            // 调整位置，使障碍物中心对齐网格
            const gridX = Math.round(mouseX / this.gridSize) * this.gridSize;
            const gridY = Math.round(mouseY / this.gridSize) * this.gridSize;
            
            // 确保障碍物在画布内
            const x = Math.max(this.padding, Math.min(this.width - this.padding - this.obstacleSize, gridX - this.obstacleSize / 2));
            const y = Math.max(this.padding, Math.min(this.height - this.padding - this.obstacleSize, gridY - this.obstacleSize / 2));
            
            // 添加新障碍物，2*2米的小正方形
            this.obstacles.push({ x, y, width: this.obstacleSize, height: this.obstacleSize, dragging: false });
            this.updatePath();
        }
    }
    
    isPointInRect(point, rect) {
        return point.x >= rect.x && point.x <= rect.x + rect.width &&
               point.y >= rect.y && point.y <= rect.y + rect.height;
    }
    
    getBounds(s) {
        const centerY = this.height / 2;
        
        // 转换为像素坐标的上下边界
        // 注意：画布y轴向下，所以上边界y值小，下边界y值大
        let upperBound = centerY - this.lUpperBound * this.meterToPixel;
        let lowerBound = centerY - this.lLowerBound * this.meterToPixel;
        
        // 障碍物不再影响边界，只返回原始边界
        
        return { upperBound, lowerBound, centerY };
    }
    
    generateGrid() {
        const grid = [];
        
        // 生成网格
        for (let x = this.padding; x < this.width - this.padding; x += this.gridSize) {
            const { upperBound, lowerBound } = this.getBounds(x);
            for (let y = upperBound; y < lowerBound; y += this.gridSize) {
                grid.push({ x, y });
            }
        }
        
        return grid;
    }
    
    dynamicProgramming() {
        const path = [];
        
        // 确保路径从0,0开始
        const startX = this.padding;
        
        // 生成网格
        const grid = this.generateGrid();
        
        // 初始化动态规划表
        const dp = [];
        const prev = [];
        
        for (let x = 0; x < grid.length; x++) {
            dp[x] = [];
            prev[x] = [];
            for (let y = 0; y < grid[x].length; y++) {
                dp[x][y] = Infinity;
                prev[x][y] = null;
            }
        }
        
        // 找到起点在网格中的位置
        const startY = Math.floor(grid[0].length / 2);
        dp[0][startY] = 0;
        
        // 动态规划
        for (let x = 0; x < grid.length - 1; x++) {
            for (let y = 0; y < grid[x].length; y++) {
                if (dp[x][y] === Infinity) continue;
                
                // 检查当前网格是否被障碍物占据
                if (!grid[x][y].passable) continue;
                
                // 尝试所有可能的下一步（上下左右，以及对角线）
                for (let dy = -2; dy <= 2; dy++) {
                    const newY = y + dy;
                    if (newY < 0 || newY >= grid[x + 1].length) continue;
                    
                    // 检查目标网格是否被障碍物占据
                    if (!grid[x + 1][newY].passable) continue;
                    
                    // 计算成本：距离中心线的距离 + 路径平滑度
                    const centerY = Math.floor(grid[x + 1].length / 2);
                    const distanceCost = Math.abs(newY - centerY) * 0.5; // 增加吸引中心线的权重
                    const smoothnessCost = Math.abs(dy) * 1;
                    const totalCost = dp[x][y] + distanceCost + smoothnessCost;
                    
                    if (totalCost < dp[x + 1][newY]) {
                        dp[x + 1][newY] = totalCost;
                        prev[x + 1][newY] = y;
                    }
                }
            }
        }
        
        // 回溯找到路径
        let currentY = startY;
        for (let x = grid.length - 1; x >= 0; x--) {
            if (x < grid.length - 1) {
                currentY = prev[x + 1][currentY];
            }
            
            // 转换网格坐标到像素坐标
            const pixelX = startX + x * this.gridSize;
            const { upperBound, lowerBound } = this.getBounds(pixelX);
            const pixelY = upperBound + (lowerBound - upperBound) * currentY / grid[x].length;
            
            path.unshift({ x: pixelX, y: pixelY });
        }
        
        // 使用改进的平滑算法生成更平滑的曲线
        return this.smoothPath(path);
    }
    
    generateGrid() {
        const grid = [];
        
        // 确保路径从0,0开始
        const startX = this.padding;
        
        // 确保网格的范围包括s=smax的地方
        for (let x = startX; x <= this.width - this.padding; x += this.gridSize) {
            const row = [];
            const { upperBound, lowerBound } = this.getBounds(x);
            
            // 计算当前x处的网格行数
            const numRows = Math.floor((lowerBound - upperBound) / this.gridSize) + 1;
            
            for (let y = 0; y < numRows; y++) {
                // 转换网格坐标到像素坐标
                const pixelY = upperBound + y * this.gridSize;
                const pixelX = x;
                
                // 检查是否被障碍物占据（包括额外的1米空间）
                let passable = true;
                for (let obstacle of this.obstacles) {
                    // 创建包含额外空间的扩展障碍物边界
                    const extendedObstacle = {
                        x: obstacle.x - this.obstacleOccupancyMargin,
                        y: obstacle.y - this.obstacleOccupancyMargin,
                        width: obstacle.width + 2 * this.obstacleOccupancyMargin,
                        height: obstacle.height + 2 * this.obstacleOccupancyMargin
                    };
                    
                    if (this.isPointInRect({ x: pixelX, y: pixelY }, extendedObstacle)) {
                        passable = false;
                        break;
                    }
                }
                
                row.push({ passable });
            }
            
            grid.push(row);
        }
        
        return grid;
    }
    
    smoothPath(path) {
        if (path.length < 3) return path;
        
        // 使用二次移动平均平滑算法
        const smoothedPath = [];
        
        // 第一次移动平均
        const firstPass = [];
        const windowSize = 3;
        
        // 保持起点
        firstPass.push(path[0]);
        
        // 平滑中间点
        for (let i = 1; i < path.length - 1; i++) {
            let sumY = 0;
            let count = 0;
            
            // 计算窗口内的平均值
            for (let j = Math.max(0, i - windowSize + 1); j <= Math.min(path.length - 1, i + windowSize - 1); j++) {
                sumY += path[j].y;
                count++;
            }
            
            const x = path[i].x;
            const y = sumY / count;
            
            firstPass.push({ x, y });
        }
        
        // 保持终点
        firstPass.push(path[path.length - 1]);
        
        // 第二次移动平均，进一步平滑
        // 保持起点
        smoothedPath.push(firstPass[0]);
        
        // 平滑中间点
        for (let i = 1; i < firstPass.length - 1; i++) {
            let sumY = 0;
            let count = 0;
            
            // 计算窗口内的平均值
            for (let j = Math.max(0, i - windowSize + 1); j <= Math.min(firstPass.length - 1, i + windowSize - 1); j++) {
                sumY += firstPass[j].y;
                count++;
            }
            
            const x = firstPass[i].x;
            const y = sumY / count;
            
            smoothedPath.push({ x, y });
        }
        
        // 保持终点
        smoothedPath.push(firstPass[firstPass.length - 1]);
        
        // 最后再次检查所有点是否在边界内
        for (let i = 0; i < smoothedPath.length; i++) {
            const { x, y } = smoothedPath[i];
            const { upperBound, lowerBound } = this.getBounds(x);
            smoothedPath[i].y = Math.max(upperBound, Math.min(lowerBound, y));
        }
        
        return smoothedPath;
    }
    
    updatePath() {
        const newPath = this.dynamicProgramming();
        this.path = newPath;
        this.lastPath = [...newPath];
    }
    
    draw() {
        // 清空画布
        this.ctx.fillStyle = '#ffffff';
        this.ctx.fillRect(0, 0, this.width, this.height);
        
        // 绘制边界和填充区域（灰色区域在网格下方）
        this.drawBounds();
        
        // 绘制l=0虚线
        this.drawCenterLine();
        
        // 绘制网格
        this.drawGrid();
        
        // 绘制障碍物
        this.drawObstacles();
        
        // 绘制路径
        this.drawPath();
    }
    

    
    drawGrid() {
        const centerY = this.height / 2;
        
        // 绘制网格线
        this.ctx.strokeStyle = '#e0e0e0';
        this.ctx.lineWidth = 0.5;
        
        // 垂直线（s方向）
        for (let x = this.padding; x < this.width - this.padding; x += this.gridSize) {
            this.ctx.beginPath();
            this.ctx.moveTo(x, this.padding);
            this.ctx.lineTo(x, this.height - this.padding);
            this.ctx.stroke();
        }
        
        // 水平线（l方向），确保包含l=0的线
        const lRange = this.lUpperBound - this.lLowerBound;
        const lStep = this.gridSize / this.meterToPixel;
        const numLines = Math.floor(lRange / lStep) + 1;
        
        for (let i = 0; i < numLines; i++) {
            const l = this.lLowerBound + i * lStep;
            const y = centerY - l * this.meterToPixel;
            
            this.ctx.beginPath();
            this.ctx.moveTo(this.padding, y);
            this.ctx.lineTo(this.width - this.padding, y);
            this.ctx.stroke();
        }
    }
    
    drawCenterLine() {
        const centerY = this.height / 2;
        
        // 绘制l=0虚线
        this.ctx.strokeStyle = '#999';
        this.ctx.lineWidth = 1;
        this.ctx.setLineDash([5, 5]);
        
        this.ctx.beginPath();
        this.ctx.moveTo(this.padding, centerY);
        this.ctx.lineTo(this.width - this.padding, centerY);
        this.ctx.stroke();
        
        this.ctx.setLineDash([]);
    }
    
    drawBounds() {
        // 获取固定的边界值
        const { upperBound, lowerBound } = this.getBounds(this.padding);
        
        // 计算与网格一致的终点
        const endX = Math.floor((this.width - this.padding - this.padding) / this.gridSize) * this.gridSize + this.padding;
        
        // 绘制填充区域：path bound之内是灰色
        this.ctx.fillStyle = '#f0f0f0';
        this.ctx.beginPath();
        this.ctx.moveTo(this.padding, upperBound);
        this.ctx.lineTo(endX, upperBound);
        this.ctx.lineTo(endX, lowerBound);
        this.ctx.lineTo(this.padding, lowerBound);
        this.ctx.closePath();
        this.ctx.fill();
        
        // 绘制边界线：细实线
        this.ctx.strokeStyle = '#057dbc';
        this.ctx.lineWidth = 1;
        this.ctx.setLineDash([]);
        
        // 上边界
        this.ctx.beginPath();
        this.ctx.moveTo(this.padding, upperBound);
        this.ctx.lineTo(endX, upperBound);
        this.ctx.stroke();
        
        // 下边界
        this.ctx.beginPath();
        this.ctx.moveTo(this.padding, lowerBound);
        this.ctx.lineTo(endX, lowerBound);
        this.ctx.stroke();
    }
    
    drawObstacles() {
        for (let obstacle of this.obstacles) {
            this.ctx.fillStyle = 'rgba(255, 0, 0, 0.2)';
            this.ctx.strokeStyle = '#ff0000';
            this.ctx.lineWidth = 2;
            this.ctx.fillRect(obstacle.x, obstacle.y, this.obstacleSize, this.obstacleSize);
            this.ctx.strokeRect(obstacle.x, obstacle.y, this.obstacleSize, this.obstacleSize);
        }
    }
    
    drawEgoVehicle() {
        const carX = this.padding;
        const carY = this.height / 2 - this.carWidth / 2;
        
        // 绘制车box（2米宽，5米长，朝右）
        // 不透明蓝色车子，边框深蓝
        this.ctx.fillStyle = '#4a90e2';
        this.ctx.strokeStyle = '#057dbc';
        this.ctx.lineWidth = 2;
        this.ctx.fillRect(carX, carY, this.carLength, this.carWidth);
        this.ctx.strokeRect(carX, carY, this.carLength, this.carWidth);
    }
    
    drawPath() {
        if (this.path.length < 2) return;
        
        // 绘制主轨迹线
        // 半透明浅绿色粗实线
        this.ctx.strokeStyle = 'rgba(76, 175, 80, 0.7)';
        this.ctx.lineWidth = 4;
        
        this.ctx.beginPath();
        this.ctx.moveTo(this.path[0].x, this.path[0].y);
        
        for (let i = 1; i < this.path.length; i++) {
            this.ctx.lineTo(this.path[i].x, this.path[i].y);
        }
        
        this.ctx.stroke();
    }
    
    startAnimation() {
        let lastTime = 0;
        const updateInterval = 2; // 500Hz
        const self = this;
        
        function animate(currentTime) {
            if (currentTime - lastTime >= updateInterval) {
                self.updatePath();
                self.draw();
                lastTime = currentTime;
            }
            requestAnimationFrame(animate);
        }
        
        animate(0);
    }
}

// 初始化路径规划器
document.addEventListener('DOMContentLoaded', function() {
    const canvas = document.getElementById('simulationCanvas');
    if (canvas) {
        new PathPlanner(canvas);
    } else {
        console.error('Canvas element not found');
    }
});
