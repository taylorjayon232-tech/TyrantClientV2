package com.passive.fabric.modules;

/**
 * Game access for the Java module layer.
 *
 * <p>Passive's host stays native: the injection, the ImGui interface, the packet hooks and the
 * per-frame JNI scope all live in {@code passive_agent.dll}. Java modules are the decision logic;
 * this class is the narrow, typed boundary they use to read the world and perform the same vanilla
 * actions the C++ modules used ({@code KeyBinding.setPressed}, the interaction manager, the
 * entity's own velocity).
 *
 * <p>The native side registers these methods when it loads {@link ModuleManager}. They are only
 * valid inside a frame tick, which is exactly when {@link Module#tick(float)} runs, so every method
 * is null-safe and answers {@code false} / {@code 0} / an empty array rather than throwing.
 */
public final class NativeApi {

    private NativeApi() {}

    /** A local player and world are present and the client's mappings resolved. */
    public static native boolean ready();

    /** A Minecraft screen (inventory, chat, pause) is open. */
    public static native boolean screenOpen();

    /** The Passive menu is open, so game input is frozen. */
    public static native boolean inputFrozen();

    public static native float health();

    /** Ticks since the local player was last hurt (0 = not hurt). */
    public static native int hurtTime();

    public static native boolean onGround();

    /** {x, y, z} of the local player, or an empty array when there is no player. */
    public static native double[] pos();

    /** {x, y, z} motion of the local player, or an empty array when there is no player. */
    public static native double[] velocity();

    /** Writes the local player's motion. Returns false when there is no player. */
    public static native boolean setVelocity(double x, double y, double z);

    /** True when another player is within {@code radius} blocks of the given point. */
    public static native boolean playerWithin(double x, double y, double z, double radius);

    /** Vanilla jump key. Returns false when the key binding could not be resolved. */
    public static native boolean setJumpKey(boolean pressed);

    /** Vanilla sprint key. Returns false when the key binding could not be resolved. */
    public static native boolean setSprintKey(boolean pressed);

    /** Writes one line to passive_agent.log, prefixed with the module name by the caller. */
    public static native void log(String message);
}
