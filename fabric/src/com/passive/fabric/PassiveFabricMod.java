package com.passive.fabric;

import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.loader.api.FabricLoader;

import java.nio.file.Path;

/**
 * The jar's whole job: hand the client to the game process at startup.
 *
 * <p>Passive is not a Java client — the menu is ImGui and the modules live in
 * C++ (agent/), with MinHook on the swap function and the window procedure.
 * This entry point unpacks the built client out of the jar
 * ({@link NativeLoader}) and maps it, so a jar in {@code mods/} is all the
 * install there is; no injector, no separate DLL to copy.
 *
 * <p>From there it behaves exactly like an injected client: <b>RSHIFT</b> opens
 * the menu, modules work as they always did, and <b>Ctrl+Shift+End</b> (or
 * Settings &gt; General &gt; SELF DESTRUCT) asks the client to unload itself.
 * That unload is real — the DLL leaves the process — so the standalone
 * {@code passive_injector.exe} can map the very same DLL again afterwards.
 */
public final class PassiveFabricMod implements ClientModInitializer {

    @Override
    public void onInitializeClient() {
        try {
            final Path agent = NativeLoader.ensureLoaded(FabricLoader.getInstance().getGameDir());
            System.out.println("[Passive] native client mapped: " + agent);
            System.out.println("[Passive] RSHIFT opens the menu; Ctrl+Shift+End uninjects");
        } catch (Throwable failure) {
            // A failed load must never be the reason Minecraft does not start:
            // say what happened, point at the log, and let the game run.
            System.err.println("[Passive] the native client did not start: " + failure);
            failure.printStackTrace();
        }
    }

    /**
     * Ask the client to leave the process — the same teardown the in-game
     * uninject performs, exposed for a command or a keybind to call later.
     *
     * @return true when the client's module left the process
     */
    public static boolean uninject() {
        try {
            return PassiveNative.isBound() && PassiveNative.isLoaded() && PassiveNative.unload();
        } catch (Throwable failure) {
            System.err.println("[Passive] uninject failed: " + failure);
            return false;
        }
    }
}
