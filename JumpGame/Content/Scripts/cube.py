import candy


class Cube(candy.ScriptObject):
    """Player cube — fully physics-driven. Landing on obstacles is ok, side-hit is death."""

    def on_construct(self):
        self.speed = 5.0
        self.jump_velocity = 7.0
        self.ground_contact_count = 0
        self.game_over = False
        self.space_was_pressed = False
        self.is_shrunk = False

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
        transform = entity.get_component("TransformComponent")

        # --- Crouch: hold S to shrink to half size, anchored at the bottom-center ---
        # The physics body's position is its center and the engine resyncs the transform
        # from it every frame, so to keep the cube grounded we shift the body down by the
        # difference in half-heights and rebuild the collider at the new size.
        want_shrink = candy.is_key_pressed("S")
        if want_shrink != self.is_shrunk:
            old_scale = transform.Scale
            factor = 0.5 if want_shrink else 2.0
            new_scale = candy.Vec3(old_scale.x * factor, old_scale.y * factor, 1.0)
            old_half = old_scale.y * 0.5
            new_half = new_scale.y * 0.5
            transform.Scale = new_scale
            # keep the bottom edge fixed: move the center by (old half - new half)
            transform.Translation = candy.Vec3(
                transform.Translation.x,
                transform.Translation.y - old_half + new_half,
                transform.Translation.z
            )
            # rebuild the physics body so the collider matches the new size
            entity.scene.recreate_physics_body(entity)
            self.is_shrunk = want_shrink

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
        if transform.Translation.y < -3.0:
            self.game_over = True
            self._entity.queue_free()

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
