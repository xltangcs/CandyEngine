import candy


class HUD(candy.ScriptObject):
    """HUD script attached to the entity that owns the Restart button.

    - Hides the Restart button when the game starts.
    - Forwards the button click to the GameManager to restart the game.
    """

    def on_construct(self):
        # Make sure the Restart button and Ending text start hidden.
        entity = self._entity
        if entity is not None:
            if entity.has_component("UIButtonComponent"):
                entity.get_component("UIButtonComponent").set_button_visible("Restart", False)
            if entity.has_component("UITextBlockComponent"):
                entity.get_component("UITextBlockComponent").set_text_visible("Ending", False)

    def on_tick(self, dt):
        pass

    def on_restart_btn_clicked(self):
        # Dispatched by UISystem when the "Restart" button is clicked.
        entity = self._entity
        if entity is None:
            return

        scene = entity.scene
        if scene is None:
            return

        gm = scene.find_entity_by_tag("GameManager")
        if gm is not None:
            gm.call_function("restart_game")
