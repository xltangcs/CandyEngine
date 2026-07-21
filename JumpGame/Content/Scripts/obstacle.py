import candy


class Obstacle(candy.ScriptObject):
    """Obstacle that moves left at constant speed."""

    def on_construct(self):
        self.speed = 3.0
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

        rb = entity.get_component("Rigidbody2DComponent")
        rb.set_linear_velocity(-self.speed, 0.0)

        # Check if past left boundary
        transform = entity.get_component("TransformComponent")
        if transform.Translation.x < self.left_boundary:
            self.dead = True
            rb.set_linear_velocity(0.0, 0.0)  # Stop moving

    def on_collision_enter(self, other):
        pass
