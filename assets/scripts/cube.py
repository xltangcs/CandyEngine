import candy

class Cube(candy.ScriptObject):
    def on_construct(self):
        self.speed = 45.0

    def on_tick(self, dt):
        if self._entity is None:
            return
        transform = self._entity.get_component("TransformComponent")
        transform.Rotation.z += self.speed * dt
