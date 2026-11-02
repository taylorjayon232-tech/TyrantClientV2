package com.passive.fabric.modules;

/**
 * Keeps the vanilla sprint key held while enabled.
 *
 * <p>The game still enforces hunger, sneaking, blindness and collisions, because this is a real
 * {@code KeyBinding.setPressed} on the sprint key, not a velocity write. Ported from the C++
 * {@code updateSprint}: the key is released the moment the module is disabled, the menu opens, a
 * screen is up, or there is no world.
 */
public final class SprintModule extends Module {

    private boolean applied;

    public SprintModule() {
        super("Sprint", "Movement",
              "Experimental: keep sprinting without holding the sprint key", "Experimental");
    }

    @Override
    protected void onDisable() {
        release();
    }

    @Override
    public void tick(float dt) {
        if (!NativeApi.ready() || NativeApi.inputFrozen() || NativeApi.screenOpen()) {
            release();
            return;
        }
        applied = NativeApi.setSprintKey(true);
    }

    private void release() {
        if (!applied) return;
        NativeApi.setSprintKey(false);
        applied = false;
    }
}
