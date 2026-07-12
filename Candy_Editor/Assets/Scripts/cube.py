import candy

class Cube(candy.ScriptObject):
    def on_construct(self):
        self.speed = 5.0
        self.velocity_y = 0.0
        self.gravity = -20.0
        self.jump_velocity = 9.0
        self.max_jumps = 2
        self.jump_count = 0
        self.ground_y = 0.0
        self.space_was_pressed = False

    def on_tick(self, dt):
        t = self._entity.get_component("TransformComponent")

        if candy.is_key_pressed("A"):
            t.Translation.x -= self.speed * dt
        if candy.is_key_pressed("D"):
            t.Translation.x += self.speed * dt

        space_down = candy.is_key_pressed("SPACE")
        if space_down and not self.space_was_pressed and self.jump_count < self.max_jumps:
            self.velocity_y = self.jump_velocity
            self.jump_count += 1
        self.space_was_pressed = space_down

        self.velocity_y += self.gravity * dt
        t.Translation.y += self.velocity_y * dt

        half_h = t.Scale.y * 0.5
        if t.Translation.y - half_h <= self.ground_y:
            t.Translation.y = self.ground_y + half_h
            self.velocity_y = 0.0
            self.jump_count = 0

        # HUD
        hud = self._entity.scene.find_entity_by_tag("HUD")
        if hud is not None and hud.has_component("UITextBlockComponent"):
            tb = hud.get_component("UITextBlockComponent").TextBlocks["TextBlock_1"]
            tb.Text = (f"Cube  Pos:({t.Translation.x:.1f},{t.Translation.y:.1f})  "
                       f"VelY:{self.velocity_y:.1f}")