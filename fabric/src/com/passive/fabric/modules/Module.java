package com.passive.fabric.modules;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * One module.
 *
 * <p>This is the same contract the C++ modules used: a name, a category, a description, a tag, a
 * list of settings, and {@link #tick(float)} while enabled. Toggling is driven by the native
 * interface (card switch or keybind), which owns persistence; the module only sees
 * {@link #onEnable()} / {@link #onDisable()}.
 *
 * <p>{@code tick} runs on the game's render thread inside the frame, so {@link NativeApi} calls are
 * valid there and nowhere else. Keep it short: it runs every frame, not every game tick.
 */
public abstract class Module {

    private final String name;
    private final String category;
    private final String description;
    private final String tag;
    private final List<Setting> settings = new ArrayList<>();
    private boolean enabled;

    protected Module(String name, String category, String description, String tag) {
        this.name = name;
        this.category = category;
        this.description = description;
        this.tag = tag;
    }

    /** Adds a setting and returns it, so modules can keep it as a field. */
    protected final Setting add(Setting setting) {
        settings.add(setting);
        return setting;
    }

    public final String name() {
        return name;
    }

    public final String category() {
        return category;
    }

    public final String description() {
        return description;
    }

    public final String tag() {
        return tag;
    }

    public final boolean isEnabled() {
        return enabled;
    }

    public final List<Setting> settings() {
        return Collections.unmodifiableList(settings);
    }

    public final void setEnabled(boolean on) {
        if (enabled == on) return;
        enabled = on;
        if (on) onEnable();
        else onDisable();
    }

    protected void onEnable() {
    }

    protected void onDisable() {
    }

    public abstract void tick(float dt);
}
