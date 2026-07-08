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
