import candy

class Obstacle(candy.ScriptObject):
    def on_construct(self):
        self.speed = 3.0
        self.reset_x = 10.0
        self.boundary_x = -10.0

    def on_tick(self, dt):
        t = self._entity.get_component("TransformComponent")
        t.Translation.x -= self.speed * dt
        if t.Translation.x < self.boundary_x:
            t.Translation.x = self.reset_x