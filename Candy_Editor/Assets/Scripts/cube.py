import candy

class Cube(candy.ScriptObject):
    def on_construct(self):
        self.speed = 5.0

    def on_tick(self, dt):
        t = self._entity.get_component("TransformComponent")
        if candy.is_key_pressed("W"):
            t.Translation.y += self.speed * dt
        if candy.is_key_pressed("S"):
            t.Translation.y -= self.speed * dt
        if candy.is_key_pressed("A"):
            t.Translation.x -= self.speed * dt
        if candy.is_key_pressed("D"):
            t.Translation.x += self.speed * dt

        # Update HUD
        hud = self._entity.scene.find_entity_by_tag("HUD")
        if hud is not None and hud.has_component("UITextBlockComponent"):
            ui = hud.get_component("UITextBlockComponent")
            tb = ui.TextBlocks["TextBlock_1"]
            tb.Text = (f"Cube  Pos:({t.Translation.x:.1f},{t.Translation.y:.1f},{t.Translation.z:.1f})  "
                       f"Rot:({t.Rotation.x:.1f},{t.Rotation.y:.1f},{t.Rotation.z:.1f})  "
                       f"Scale:({t.Scale.x:.1f},{t.Scale.y:.1f},{t.Scale.z:.1f})")
