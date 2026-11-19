# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Atomic OVUI widgets bound to the inspector's single application model."""
from inspector_model import LABELS, LIMITS, PANEL_CENTER, PANEL_SIZE, RESOLUTION


STYLE = {
    "Label": {"color": 0xffeee6dc, "font_size": 23},
    "Button": {"background_color": 0xff40372b, "border_radius": 7},
    "Button:hovered": {"background_color": 0xff625039},
    "Button:pressed": {"background_color": 0xff87602c},
    "Button:selected": {"background_color": 0xff765026},
    "Button.Label": {"font_size": 23},
    "Slider": {"background_color": 0xff40372b, "font_size": 23, "secondary_color": 0xffad7738},
}


class Controls:
    def __init__(self, model):
        self.model = model
        self.widgets = {}
        self.syncing = False
        self.accept_events = False
        self.revision = -1
        self.sync_checks = 0

    # [snippet:inspector-widgets]
    def build(self, ui):
        with ui.ZStack(style=STYLE):
            ui.Rectangle(style={"background_color": 0xff241d16})
            with ui.HStack():
                ui.Spacer(width=28)
                with ui.VStack(spacing=12):
                    ui.Spacer(height=12)
                    ui.Label("OVUI  /  SCENE INSPECTOR", height=52, style={"font_size": 32})
                    ui.Label("Select a part in the scene or in the list. Drag to edit.", height=34)
                    with ui.HStack(spacing=28, height=370):
                        with ui.VStack(spacing=12, width=250):
                            ui.Label("SCENE", height=30)
                            for path, name in LABELS.items():
                                self.widgets[path] = ui.Button(
                                    name, height=60,
                                    clicked_fn=lambda p=path: self.invoke(self.model.select, p, "panel"))
                            self.selection = ui.Label("No selection", height=64, word_wrap=True,
                                                      style={"font_size": 21})
                            ui.Label("Green outline = selected\nSphere = scene pointer", height=64,
                                     word_wrap=True, style={"font_size": 20, "color": 0xffaca296})
                            ui.Spacer()
                        with ui.VStack(spacing=12):
                            self.title = ui.Label("Select an object", height=30)
                            captions = {"x": "Position X", "y": "Height Y", "z": "Position Z",
                                        "yaw": "Yaw (deg)", "scale": "Scale"}
                            for field, (low, high) in LIMITS.items():
                                with ui.HStack(height=45, spacing=10):
                                    ui.Label(captions[field], width=130)
                                    slider = ui.FloatSlider(min=low, max=high, enabled=False)
                                    slider.model.add_value_changed_fn(lambda value, f=field: self.edit(f, value))
                                    self.widgets[field] = slider
                            with ui.HStack(height=49, spacing=10):
                                for name, action in (("undo", self.model.undo), ("redo", self.model.redo),
                                                     ("reset", self.model.reset)):
                                    self.widgets[name] = ui.Button(
                                        name.title(), clicked_fn=lambda a=action: self.invoke(a))
                    self.status = ui.Label("Ready", height=32, style={"font_size": 21})
                    ui.Label("Desktop: 1 / 2 select    A / D move    U undo    Y redo    R reset    Q quit",
                             height=30, style={"font_size": 20, "color": 0xffaca296})
                    ui.Label("Scene depth blocks the panel and its pointer input.", height=28,
                             style={"font_size": 20, "color": 0xffaca296})
                    ui.Spacer()
                ui.Spacer(width=28)
        self.refresh()

    def invoke(self, action, *args):
        if self.accept_events and not self.syncing:
            action(*args)

    def edit(self, field, value):
        if self.accept_events and not self.syncing:
            self.model.edit(field, value.as_float)

    def refresh(self):
        if self.revision == self.model.revision:
            return
        self.syncing = True
        try:
            selected = self.model.selected
            self.title.text = LABELS.get(selected, "Select an object")
            self.selection.text = selected or "No selection"
            for path in LABELS:
                self.widgets[path].selected = path == selected
            for field in LIMITS:
                widget = self.widgets[field]
                widget.enabled = selected is not None
                if selected:
                    value = getattr(self.model.objects[selected], field)
                    if abs(widget.model.as_float - value) > 1e-6:
                        widget.model.set_value(value)
            self.widgets["undo"].enabled = bool(self.model.undo_stack)
            self.widgets["redo"].enabled = bool(self.model.redo_stack)
            self.widgets["reset"].enabled = selected is not None
            self.status.text = f"Revision {self.model.revision}  /  {self.model.source}  /  metres, Y up"
            self.revision = self.model.revision
        finally:
            self.syncing = False

    def check_sync(self):
        """Check native value models, not just the Python values used to populate them."""
        if self.model.selected:
            for field in LIMITS:
                expected = getattr(self.model.objects[self.model.selected], field)
                if abs(self.widgets[field].model.as_float - expected) > 1e-5:
                    raise RuntimeError(f"OVUI {field} value disagrees with the scene model")
        self.sync_checks += 1
    # [/snippet:inspector-widgets]

    def demo_pointer(self, frame):
        """Synthetic XR rays hit real widgets; no direct callback invocation."""
        origin = (.3, 1.25, 2.5)
        down = False
        if frame < 14 or 50 <= frame < 58:
            path = tuple(LABELS)[int(frame >= 50)]
            item = self.model.objects[path]
            target = (item.x, item.y, item.z)
            down = frame in (8, 9, 54, 55)
        else:
            name, fraction = "y", 0.5
            if 14 <= frame < 31:
                fraction = 0.3 + 0.035 * max(0, min(8, frame - 20))
                down = 20 <= frame <= 28
            elif frame < 42:
                name = "yaw"
                fraction = 0.55 + 0.035 * max(0, min(6, frame - 32))
                down = 32 <= frame <= 38
            elif frame < 50:
                name, down = "undo", frame in (45, 46)
            elif frame < 66:
                name, down = tuple(LABELS)[0], frame in (60, 61)
            elif frame < 77:
                name, down = "reset", frame in (70, 71)
            elif frame < 87:
                name, down = "undo", frame in (80, 81)
            else:
                name, down = "redo", frame in (90, 91)
            widget = self.widgets[name]
            pixels = (widget.screen_position_x + widget.computed_width * fraction,
                      widget.screen_position_y + widget.computed_height * 0.5)
            target = (PANEL_CENTER[0] + (pixels[0] / RESOLUTION[0] - 0.5) * PANEL_SIZE[0],
                      PANEL_CENTER[1] + (0.5 - pixels[1] / RESOLUTION[1]) * PANEL_SIZE[1], PANEL_CENTER[2])
        return origin, tuple(target[i] - origin[i] for i in range(3)), down, True, "demo"
