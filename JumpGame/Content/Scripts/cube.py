import candy


class Cube(candy.ScriptObject):
    """Player cube — fully physics-driven. Landing on obstacles is ok, side-hit is death."""

    def on_construct(self):
        self.speed = 5.0
        self.jump_velocity = 7.0
        self.ground_contact_count = 0
        self.game_over = False
        self.space_was_pressed = False

    def on_tick(self, dt):
        if self.game_over:
            return
        entity = self._entity
        if entity is None:
            return

        # bgm = scene.find_entity_by_tag("BGM")
        # if not bgm:
        #     return 

        rb = entity.get_component("Rigidbody2DComponent")

        # Horizontal movement (preserve vertical velocity, physics drives position)
        vel = rb.get_linear_velocity()
        vx = 0.0
        if candy.is_key_pressed("A"):
            vx = -self.speed
        elif candy.is_key_pressed("D"):
            vx = self.speed
        rb.set_linear_velocity(vx, vel.y)

        # Single jump: only when grounded (edge detection)
        grounded = self.ground_contact_count > 0
        space_down = candy.is_key_pressed("SPACE")
        if space_down and not self.space_was_pressed and grounded:
            rb.set_linear_velocity(vx, self.jump_velocity)
        self.space_was_pressed = space_down

        # Fell off the ground → game over
        transform = entity.get_component("TransformComponent")
        if transform.Translation.y < -3.0:
            self.game_over = True
            scene.destroy_entity(self._entity)

    def on_collision_enter(self, other):
        if other is None:
            return

        tag = other.tag
        if tag.startswith("Ground"):
            self.ground_contact_count += 1
        elif tag.startswith("Obstacle"):
            self.ground_contact_count += 1
    def on_collision_exit(self, other):
        if other is None:
            return
        tag = other.tag
        if tag.startswith("Ground") or tag.startswith("Obstacle"):
            self.ground_contact_count -= 1
