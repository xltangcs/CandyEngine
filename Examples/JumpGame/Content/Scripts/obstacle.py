import candy


class Obstacle(candy.ScriptObject):
    """Obstacle that moves left, speed increases over time."""

    def on_construct(self):
        self.base_speed = 3.0          # 初始速度
        self.max_speed = 12.0          # 最大速度
        self.speed_increase_rate = 0.3 # 每秒速度增加量
        self.elapsed = 0.0             # 已存活时间
        self.left_boundary = -12.0
        self.dead = False

    def on_tick(self, dt):
        if self.dead:
            return

        entity = self._entity
        if entity is None:
            return

        scene = entity.scene
        if scene is None:
            return

        cube = scene.find_entity_by_tag("Cube")
        if not cube:
            return

            # 速度随时间递增，但有上限
        self.elapsed += dt
        current_speed = min(
            self.base_speed + self.speed_increase_rate * self.elapsed,
            self.max_speed
        )

        rb = entity.get_component("Rigidbody2DComponent")
        rb.set_linear_velocity(-current_speed, 0.0)

        # Check if past left boundary
        transform = entity.get_component("TransformComponent")
        if transform.Translation.x < self.left_boundary:
            self.dead = True
            rb.set_linear_velocity(0.0, 0.0)  # Stop moving

    def on_collision_enter(self, other):
        pass
