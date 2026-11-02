package com.passive.fabric;

import java.nio.file.Path;

/**
 * The bridge's JNI surface, as declared by {@code passive_bridge.dll}
 * (fabric/bridge/bridge.cpp).
 *
 * <p>Only the small bridge is loaded with {@link System#load}. The client itself
 * is mapped by plain {@code LoadLibraryExW} from inside the bridge, so the JVM
 * holds no reference to it. The bridge owns the one load reference and releases
 * it after the client reports its teardown complete, which is what makes an
 * uninject real (the module leaves the process, and the standalone injector can
 * then map the same DLL again).
 *
 * <p>{@link #bind(Path)} must run before any other call here; {@link NativeLoader}
 * does that as part of loading the client.
 */
public final class PassiveNative {

    private static boolean bound;

    private PassiveNative() {}

    /** Loads the bridge DLL. Called once, before the first native call. */
    static synchronized void bind(Path bridgeDll) {
        if (bound) return;
        // System.load takes an absolute path (a bare name would be resolved
        // through the library search path, which the jar never controls).
        System.load(bridgeDll.toAbsolutePath().toString());
        bound = true;
    }

    static synchronized boolean isBound() {
        return bound;
    }

    /**
     * Maps {@code passive_agent.dll} from {@code dllPath}.
     *
     * @return true when the client is mapped; false when the bridge refused or
     *         the DLL could not be loaded, with {@link #lastError()} saying why.
     */
    public static boolean load(String dllPath) {
        requireBound();
        return nativeLoad(dllPath);
    }

    /**
     * Asks the mapped client to unload itself — the same teardown the in-game
     * SELF DESTRUCT button and the Ctrl+Shift+End chord perform — and waits for
     * its module to leave the process.
     *
     * <p>Works whether this bridge mapped the client or the standalone injector
     * did: the unload is the client's own, and the bridge only waits. When the
     * bridge mapped it, the bridge also owns the loader reference and releases
     * it after the client reports its teardown complete, so the module really
     * leaves the process rather than remaining mapped behind a reference.
     */
    public static boolean unload() {
        requireBound();
        return nativeUnload();
    }

    /** Is a Passive client mapped in this process right now? */
    public static boolean isLoaded() {
        requireBound();
        return nativeLoaded();
    }

    /** Why the last {@link #load} / {@link #unload} said no. */
    public static String lastError() {
        requireBound();
        return nativeLastError();
    }

    /** The bridge's ABI, so a mismatch with the jar it came in can be reported. */
    public static int bridgeAbi() {
        requireBound();
        return nativeBridgeAbi();
    }

    private static void requireBound() {
        if (!bound) {
            throw new IllegalStateException("passive_bridge.dll is not loaded; call NativeLoader.ensureLoaded first");
        }
    }

    private static native boolean nativeLoad(String dllPath);

    private static native boolean nativeUnload();

    private static native boolean nativeLoaded();

    private static native String nativeLastError();

    private static native int nativeBridgeAbi();
}
