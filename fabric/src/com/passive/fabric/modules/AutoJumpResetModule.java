package com.passive.fabric.modules;

import java.util.concurrent.ThreadLocalRandom;

/**
 * Jumps on the exact hit tick to cut horizontal knockback.
 *
 * <p>Ported from the C++ {@code updateAutoJumpReset}. Two signals detect "just took a hit": the
 * rising edge of {@code hurtTime} (the hurt tick itself) and a health drop (a safety net if the edge
 * is missed between frames). The jump writes the vanilla 0.42 upward velocity into the player and
 * holds the real jump key, so hunger, sneaking and vehicles stay the game's rules.
 *
 * <p>Only damage that plausibly came from another player counts: one must be within 8 blocks.
 */
public final class AutoJumpResetModule extends Module {

    private final Setting accuracy =
        Setting.integer("accuracy", "Accuracy (%)", 0, 100, 100);

    private int prevHurtTime;
    private float prevHealth = -1f;
    private boolean jumpHeld;
    private long jumpReleaseAt;

    public AutoJumpResetModule() {
        super("Auto Jump Reset", "Combat",
              "Experimental: jump the moment another player damages you", "Experimental");
        add(accuracy);
    }

    @Override
    protected void onDisable() {
        releaseJump();
        prevHurtTime = 0;
        prevHealth = -1f;
    }

    @Override
    public void tick(float dt) {
        final long now = System.currentTimeMillis();
        if (jumpHeld && now >= jumpReleaseAt) releaseJump();

        if (!NativeApi.ready() || NativeApi.inputFrozen()) {
            prevHurtTime = 0;
            prevHealth = -1f;
            return;
        }

        final int hurt = NativeApi.hurtTime();
        final float health = NativeApi.health();
        final boolean hurtEdge = hurt > 0 && hurt > prevHurtTime;
        final boolean healthEdge = prevHealth > 0f && health > 0f && health < prevHealth - 0.01f;
        final boolean freshHit = hurtEdge || healthEdge;
        prevHurtTime = hurt;
        if (health > 0f) prevHealth = health;

        if (!freshHit || NativeApi.screenOpen() || !NativeApi.onGround() || jumpHeld) return;

        final double[] me = NativeApi.pos();
        if (me.length < 3 || !NativeApi.playerWithin(me[0], me[1], me[2], 8.0)) return;
        if (ThreadLocalRandom.current().nextInt(100) >= accuracy.intValue()) return;

        final double[] velocity = NativeApi.velocity();
        if (velocity.length >= 3 && velocity[1] <= 0.001) {
            NativeApi.setVelocity(velocity[0], 0.42, velocity[2]);
        }
        NativeApi.setJumpKey(true);
        jumpHeld = true;
        jumpReleaseAt = now + 120L;   // the C++ hold: 0.12 s
    }

    private void releaseJump() {
        if (!jumpHeld) return;
        NativeApi.setJumpKey(false);
        jumpHeld = false;
    }
}
