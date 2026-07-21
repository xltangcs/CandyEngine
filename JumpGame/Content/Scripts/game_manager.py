import candy
import random


class GameManager(candy.ScriptObject):
    """Manages game state: spawns obstacles, tracks score, updates HUD."""

    def on_construct(self):
        self.base_spawn_interval = 1.5    # 初始生成间隔（秒）
        self.min_spawn_interval = 0.3     # 最小生成间隔
        self.interval_decrease_rate = 0.02 # 每秒间隔减少量
        self.spawn_timer = 0.0
        self.obstacle_speed = 3.0
        self.game_over = False
        self.score = 0.0
        self.obstacle_count = 0
        self.spawn_x = 8.0
        self.obstacle_index = 0

        # --- 障碍物 X 轴宽度：0.4 ~ 0.8 独立随机 ---
        self.obs_x_scale_min = 0.4
        self.obs_x_scale_max = 0.8

        # --- 障碍物 Y 轴高度：独立随机 ---
        self.obs_y_scale_min = 0.3
        self.obs_y_scale_max = 0.7

        # --- 可通过性相关（基于物理：g=19.8, jump=7.0）---
        self.ground_top = -0.995         # 地面顶面 Y（中心 -1 + 半高 0.005）
        self.cube_crouch_half = 0.1      # 蹲下后 cube 半高（0.2 * 0.5）
        self.jump_peak_bottom = 0.242    # cube 跳跃峰值底部 Y（用于判断能否跳上去）
        self.crouch_margin = 0.05        # 蹲下通过的安全余量
        self.jump_type_ratio = 0.6       # “跳过去”类型占比，其余为“蹲过去”
        self.max_spawn_y = 1.5           # 浮空障碍最高 Y 位置

    def on_tick(self, dt):
        if self.game_over:
            return

        entity = self._entity
        if entity is None:
            return

        scene = entity.scene
        if scene is None:
            return

        # Check if Cube is still alive (queue_free'd → no longer in scene → game over)
        cube = scene.find_entity_by_tag("Cube")
        if cube is None:
            self.game_over = True
            self._show_restart_button(scene)
            self._update_hud()
            return

        # Increment score
        self.score += dt

        # Spawn timer — 间隔随时间递减
        self.spawn_timer -= dt
        if self.spawn_timer <= 0.0:
            # 计算当前间隔 = max(最小间隔, 基础间隔 - 递减量 * 存活时间)
            current_interval = max(
                self.min_spawn_interval,
                self.base_spawn_interval - self.interval_decrease_rate * self.score
            )
            self.spawn_timer = current_interval
            self._spawn_obstacle(scene)

        # Clean up dead obstacles
        self._cleanup_obstacles(scene)

        # Update HUD
        self._update_hud()

    def _cleanup_obstacles(self, scene):
        """Destroy obstacles that have passed the left boundary."""
        boundary_x = -12.0
        for i in range(1, self.obstacle_index + 1):
            name = f"Obstacle_{i}"
            obs = scene.find_entity_by_tag(name)
            if obs is not None:
                transform = obs.get_component("TransformComponent")
                if transform.Translation.x < boundary_x:
                    obs.queue_free()

    def _spawn_obstacle(self, scene):
        self.obstacle_index += 1
        name = f"Obstacle_{self.obstacle_index}"
        obs = scene.create_entity(name)
        if obs is None:
            return

        # Tag
        obs.tag = name

        # X Scale: 独立随机 0.4 ~ 0.8
        x_scale = random.uniform(self.obs_x_scale_min, self.obs_x_scale_max)

        # Y Scale: 独立随机
        y_scale = random.uniform(self.obs_y_scale_min, self.obs_y_scale_max)

        # Y 位置: 保证至少一条路径可通过（跳过去 或 蹲过去）
        if random.random() < self.jump_type_ratio:
            # 跳过去型：坐在地面上，cube 可跳上顶部翻越
            y_pos = self.ground_top + y_scale * 0.5
        else:
            # 蹲过去型：浮空，底部留出足够间隙让蹲下的 cube 通过
            min_y = (self.ground_top + self.cube_crouch_half * 2 + self.crouch_margin) + y_scale * 0.5
            y_pos = random.uniform(min_y, self.max_spawn_y)

        transform = obs.get_component("TransformComponent")
        transform.Translation = candy.Vec3(self.spawn_x, y_pos, 0.0)
        transform.Scale = candy.Vec3(x_scale, y_scale, 1.0)

        # Sprite renderer
        obs.add_component("SpriteRendererComponent")
        sprite = obs.get_component("SpriteRendererComponent")
        sprite.Color = candy.Vec4(1.0, 0.2, 0.2, 1.0)

        # Rigidbody (Kinematic)
        obs.add_component("Rigidbody2DComponent")
        rb = obs.get_component("Rigidbody2DComponent")
        rb.Type = 2        # Kinematic
        rb.FixedRotation = True

        # Box collider — 保持默认 Size(0.5,0.5)，物理半尺寸 = Size * Scale，
        # 全尺寸 = Scale，与视觉大小完全一致（不再用 random_scale 覆盖）
        obs.add_component("BoxCollider2DComponent")
        collider = obs.get_component("BoxCollider2DComponent")
        collider.Size = candy.Vec2(0.5, 0.5)
        collider.Density = 1.0
        collider.Friction = 0
        collider.Restitution = 0.0

        # Script component (obstacle.py)
        obs.add_component("ScriptComponent")
        sc = obs.get_component("ScriptComponent")
        sc.ScriptPath = "VFS://Game/Scripts/obstacle.py"
        sc.ClassName = "Obstacle"

        # Register with Box2D physics
        scene.create_physics_body(obs)

        # Instantiate the Python script
        scene.instantiate_script(obs)

        print(f"Spawned {name}")

    def _show_restart_button(self, scene):
        hud = scene.find_entity_by_tag("HUD")
        if hud is not None:
            if hud.has_component("UIButtonComponent"):
                hud.get_component("UIButtonComponent").set_button_visible("Restart", True)
            if hud.has_component("UITextBlockComponent"):
                hud.get_component("UITextBlockComponent").set_text_visible("Ending", True)

    def _hide_restart_button(self, scene):
        hud = scene.find_entity_by_tag("HUD")
        if hud is not None:
            if hud.has_component("UIButtonComponent"):
                hud.get_component("UIButtonComponent").set_button_visible("Restart", False)
            if hud.has_component("UITextBlockComponent"):
                hud.get_component("UITextBlockComponent").set_text_visible("Ending", False)

    def restart_game(self):
        """Called (via HUD.on_restart_btn_clicked) when the Restart button is clicked."""
        entity = self._entity
        if entity is None:
            return

        scene = entity.scene
        if scene is None:
            return

        # Remove every remaining obstacle.
        for i in range(1, self.obstacle_index + 1):
            obs = scene.find_entity_by_tag(f"Obstacle_{i}")
            if obs is not None:
                obs.queue_free()

        # Reset game state.
        self.spawn_timer = 0.0
        self.score = 0.0
        self.obstacle_index = 0
        self.game_over = False

        # Respawn the player Cube if it is gone.
        if scene.find_entity_by_tag("Cube") is None:
            self._spawn_cube(scene)

        # Hide the button again and refresh the HUD.
        self._hide_restart_button(scene)
        self._update_hud()

    def _spawn_cube(self, scene):
        cube = scene.create_entity("Cube")
        if cube is None:
            return

        cube.tag = "Cube"

        transform = cube.get_component("TransformComponent")
        transform.Translation = candy.Vec3(0.0, 0.0, 0.0)
        transform.Scale = candy.Vec3(0.4, 0.4, 1.0)

        cube.add_component("SpriteRendererComponent")
        sprite = cube.get_component("SpriteRendererComponent")
        sprite.Color = candy.Vec4(1.0, 0.5, 0.0, 1.0)

        cube.add_component("Rigidbody2DComponent")
        rb = cube.get_component("Rigidbody2DComponent")
        rb.Type = 1        # Dynamic
        rb.FixedRotation = True

        cube.add_component("BoxCollider2DComponent")
        collider = cube.get_component("BoxCollider2DComponent")
        collider.Size = candy.Vec2(0.5, 0.5)
        collider.Density = 1.0
        collider.Friction = 0
        collider.Restitution = 0.0

        cube.add_component("ScriptComponent")
        sc = cube.get_component("ScriptComponent")
        sc.ScriptPath = "VFS://Game/Scripts/cube.py"
        sc.ClassName = "Cube"

        scene.create_physics_body(cube)
        scene.instantiate_script(cube)

        print("Respawned Cube")

    def _update_hud(self):
        scene = self._entity.scene
        if scene is None:
            return

        hud_entity = scene.find_entity_by_tag("HUD")
        if hud_entity is None:
            return

        if not hud_entity.has_component("UITextBlockComponent"):
            return

        ui = hud_entity.get_component("UITextBlockComponent")
        # 使用 set_text 方法（避免 pybind11 map 返回副本导致修改不生效）
        ui.set_text("LifeTime", f"You have survived {int(self.score)}s")
