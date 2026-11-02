package com.passive.fabric;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Locale;

/**
 * Takes the client out of the mod jar and puts it into the game process.
 *
 * <p>The jar carries exactly what the standalone injector ships: the built
 * {@code passive_agent.dll} (all of the menu and module logic, unchanged), the
 * bridge, and the Inter faces the interface draws with. Everything is unpacked
 * into {@code <gameDir>/config/passive/native/}, next to where the client reads
 * its own files from — the client resolves {@code assets/fonts/} beside
 * <em>itself</em>, so the fonts have to land beside the DLL rather than in the
 * game directory.
 *
 * <p>Naming: the client is always written as {@code passive_agent.dll} and the
 * bridge as {@code passive_bridge_<hash>.dll}. The client's exact name matters
 * beyond tidiness — the standalone injector decides whether an instance already
 * has Passive by looking for a module called {@code passive_agent.dll}, and that
 * check has to keep telling the truth when the jar loaded the client. The
 * bridge's name is hashed instead, because it is loaded with
 * {@link System#load} and therefore cannot be replaced while the game runs: a
 * jar update must be free to drop the new bridge beside the old one.
 */
public final class NativeLoader {

    /** The name the client must be mapped under, for the injector's benefit. */
    public static final String AGENT_DLL_NAME = "passive_agent.dll";

    private static final String BRIDGE_DLL_NAME = "passive_bridge.dll";
    private static final String RESOURCE_ROOT = "passive/";
    private static final String FONT_DIR = "assets/fonts";
    private static final String LOGO_FILE = "passive-logo.png";
    private static final String[] FONTS = {
        "Inter-Regular.ttf",
        "Inter-Medium.ttf",
        "Inter-SemiBold.ttf",
    };

    private static Path loadedAgent;

    private NativeLoader() {}

    /**
     * Unpacks the jar's payload if needed, loads the bridge and maps the client.
     * Idempotent: a second call returns the already-mapped client.
     *
     * @param gameDir the Fabric game directory (what the client calls its
     *                own "next to the DLL" directory one level down)
     * @return the path of the mapped {@code passive_agent.dll}
     * @throws IOException when the client could not be brought up. The caller
     *                     reports and carries on: a client that failed to start
     *                     must never be the reason Minecraft does not.
     */
    public static synchronized Path ensureLoaded(Path gameDir) throws IOException {
        if (loadedAgent != null && PassiveNative.isBound() && PassiveNative.isLoaded()) return loadedAgent;

        final String os = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        if (!os.contains("win")) {
            throw new IOException("the Passive client is a Windows x64 DLL; this JVM runs on " + os);
        }

        final Path nativeDir = gameDir.resolve("config").resolve("passive").resolve("native");
        Files.createDirectories(nativeDir);

        // Fonts first: the interface reads them next to the client, and they are
        // the one part that is safe to skip when a file is locked by the running
        // client (an old face still draws).
        final Path fontDir = nativeDir.resolve(FONT_DIR);
        Files.createDirectories(fontDir);
        for (String font : FONTS) {
            writeIfDifferent(fontDir.resolve(font), resource(FONT_DIR + "/" + font), false);
        }
        // The brand mark lives beside the fonts because the client resolves its
        // assets relative to the DLL, not the game directory.
        writeIfDifferent(nativeDir.resolve(LOGO_FILE), resource(LOGO_FILE), false);

        final byte[] bridge = resource(BRIDGE_DLL_NAME);
        final Path bridgePath = nativeDir.resolve(bridgeName(bridge));
        writeIfDifferent(bridgePath, bridge, true);

        final byte[] agent = resource(AGENT_DLL_NAME);
        final Path agentPath = putAgent(nativeDir, agent);

        PassiveNative.bind(bridgePath);

        // The injector (or another mod) may have beaten us to it: in that case
        // the client is already mapped and mapping it again would hook the swap
        // function twice.
        if (PassiveNative.isLoaded()) {
            loadedAgent = agentPath;
            return agentPath;
        }

        if (!PassiveNative.load(agentPath.toAbsolutePath().toString())) {
            throw new IOException("the client did not load: " + PassiveNative.lastError());
        }
        loadedAgent = agentPath;
        return agentPath;
    }

    /** The client this JVM mapped, or null while nothing is loaded. */
    static synchronized Path loadedAgent() {
        return loadedAgent;
    }

    private static String bridgeName(byte[] bridge) {
        return "passive_bridge_" + shortHash(bridge) + ".dll";
    }

    /**
     * Places the client under its exact expected name, replacing an older copy
     * when the jar carries a new build. A copy that is locked by a client already
     * running from this folder cannot be replaced; in that (single) case the new
     * build is written beside it under a hashed name, so a jar update still
     * applies instead of silently running the old DLL.
     */
    private static Path putAgent(Path nativeDir, byte[] agent) throws IOException {
        final Path canonical = nativeDir.resolve(AGENT_DLL_NAME);
        try {
            writeIfDifferent(canonical, agent, true);
            return canonical;
        } catch (IOException locked) {
            if (PassiveNative.isBound() && PassiveNative.isLoaded()) return canonical;   // it is in use; leave it
            final Path fallback = nativeDir.resolve("passive_agent_" + shortHash(agent) + ".dll");
            writeIfDifferent(fallback, agent, true);
            return fallback;
        }
    }

    /** Writes {@code data} to {@code target} unless it is already that exact file. */
    private static void writeIfDifferent(Path target, byte[] data, boolean strict) throws IOException {
        if (Files.isRegularFile(target) && Files.size(target) == data.length && sameBytes(target, data)) return;

        final Path temp = target.resolveSibling(target.getFileName() + ".tmp");
        try {
            Files.write(temp, data);
            Files.move(temp, target, StandardCopyOption.REPLACE_EXISTING);
        } catch (IOException failure) {
            try {
                Files.deleteIfExists(temp);
            } catch (IOException ignored) {
                // the temp file is left to the OS; the original failure is what matters
            }
            // A locked optional file (a font, a second bridge of an older build)
            // is not worth failing the load for.
            if (strict) throw failure;
        }
    }

    private static boolean sameBytes(Path file, byte[] data) throws IOException {
        return MessageDigest.isEqual(Files.readAllBytes(file), data);
    }

    private static byte[] resource(String name) throws IOException {
        try (InputStream in = NativeLoader.class.getClassLoader().getResourceAsStream(RESOURCE_ROOT + name)) {
            if (in == null) {
                throw new IOException("this jar has no " + RESOURCE_ROOT + name
                                      + " — rebuild it with fabric\\build-jar.bat");
            }
            return in.readAllBytes();
        }
    }

    private static String shortHash(byte[] data) {
        try {
            final byte[] digest = MessageDigest.getInstance("SHA-256").digest(data);
            final StringBuilder out = new StringBuilder(12);
            for (int i = 0; i < 6; ++i) out.append(String.format("%02x", digest[i]));
            return out.toString();
        } catch (NoSuchAlgorithmException impossible) {
            throw new IllegalStateException("SHA-256 is required by every Java runtime", impossible);
        }
    }
}
