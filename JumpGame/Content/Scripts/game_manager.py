import candy
import random


class GameManager(candy.ScriptObject):
    """Manages game state: spawns obstacles, tracks score, updates HUD."""

    def on_construct(self):
        self.spawn_interval = 1.5       # seconds between spawns
        self.spawn_timer = 0.0
        self.obstacle_speed = 3.0
        self.game_over = False
        self.score = 0.0
        self.obstacle_count = 0
        self.min_spawn_y = -0.5
        self.max_spawn_y = 1.5
        self.spawn_x = 8.0
        self.obstacle_index = 0

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
            self._update_hud()
            return

        # Increment score
        self.score += dt

        # Spawn timer
        self.spawn_timer -= dt
        if self.spawn_timer <= 0.0:
            self.spawn_timer = self.spawn_interval
            self._spawn_obstacle(scene)

        # Clean up dead obstacles
        self._cleanup_obstacles(scene)

        # Update HUD
        self._update_hud()

    def _cleanup_obstacles(self, scene):
        """Destroy obstacles that have passed the left boundary."""
        boundary_x = -12.0
        # Collect dead obstacles by scanning entity tags
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

        # Transform - random Y position, spawn at right edge
        transform = obs.get_component("TransformComponent")
        y_pos = random.uniform(self.min_spawn_y, self.max_spawn_y)
        transform.Translation = candy.Vec3(self.spawn_x, y_pos, 0.0)
        transform.Scale = candy.Vec3(0.5, 0.5, 1.0)

        # Sprite renderer
        obs.add_component("SpriteRendererComponent")
        sprite = obs.get_component("SpriteRendererComponent")
        sprite.Color = candy.Vec4(1.0, 0.2, 0.2, 1.0)

        # Rigidbody (Dynamic)
        obs.add_component("Rigidbody2DComponent")
        rb = obs.get_component("Rigidbody2DComponent")
        rb.Type = 2        # Kinematic
        rb.FixedRotation = True

        # Box collider
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
        if "TextBlock_1" in ui.TextBlockDatas:
            block = ui.TextBlockDatas["TextBlock_1"]
            if self.game_over:
                block.Text = f"Game Over! Score: {self.score:.1f}"
            else:
                block.Text = f"Score: {self.score:.1f}"
