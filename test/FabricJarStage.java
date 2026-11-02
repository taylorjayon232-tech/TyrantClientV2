// ============================================================================
//  Passive — test harness: the Fabric jar's own loader, inside a real JVM.
//
//  What this exercises (driven by test/fabric_jar_check.mjs):
//    * com.passive.fabric.NativeLoader.ensureLoaded - the exact code the Fabric
//      client entry point calls: unpack the payload, load the bridge, map the
//      client. Nothing here reimplements the mod; it is the mod.
//    * a real GLFW window that swaps buffers, because the client draws from the
//      swap hook: without frames there is no overlay to observe, no handshake
//      to publish and no GL teardown for an unload to complete.
//    * the client's own unload, and the standalone injector mapping the same
//      DLL again afterwards.
//
//  Commands arrive on stdin, one per line: status | frames | unload | load |
//  exit. Results are printed as STAGE-<key>=<value> lines for the driver.
//
//  Usage: FabricJarStage <gameDir> <nativesDir> <lwjgl-natives.jar>...
// ============================================================================
import com.passive.fabric.NativeLoader;
import com.passive.fabric.PassiveNative;

import org.lwjgl.glfw.GLFW;
import org.lwjgl.glfw.GLFWErrorCallback;
import org.lwjgl.opengl.GL;
import org.lwjgl.opengl.GL11;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.Enumeration;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicLong;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public final class FabricJarStage {

    private static volatile boolean running = true;
    private static final AtomicLong FRAMES = new AtomicLong();

    private FabricJarStage() {}

    public static void main(String[] args) throws Exception {
        if (args.length < 3) {
            System.out.println("STAGE-ERROR=usage: <gameDir> <nativesDir> <nativesJar>...");
            System.exit(2);
        }
        final Path gameDir = Paths.get(args[0]);
        final Path nativesDir = Paths.get(args[1]);
        Files.createDirectories(nativesDir);
        for (int i = 2; i < args.length; ++i) extractNatives(Paths.get(args[i]), nativesDir);
        System.setProperty("org.lwjgl.librarypath", nativesDir.toAbsolutePath().toString());

        System.out.println("STAGE-PID=" + ProcessHandle.current().pid());
        System.out.println("STAGE-LOG=" + Paths.get(System.getProperty("java.home"), "bin", "passive_agent.log"));
        System.out.println("STAGE-LOG-TMP="
                           + Paths.get(System.getProperty("java.io.tmpdir"), "passive_agent.log"));

        // 1) The jar's loader, exactly as the Fabric entry point calls it.
        final Path agent = NativeLoader.ensureLoaded(gameDir);
        System.out.println("STAGE-AGENT=" + agent);
        System.out.println("STAGE-BRIDGE-ABI=" + PassiveNative.bridgeAbi());
        System.out.println("STAGE-MAPPED=" + PassiveNative.isLoaded());

        // 2) A window that presents frames. The client hooks the swap call, so
        //    frames are what make the overlay (and its handshake) real.
        GLFWErrorCallback.createPrint(System.err).set();
        if (!GLFW.glfwInit()) {
            System.out.println("STAGE-GL=none");
            System.exit(3);
        }
        GLFW.glfwDefaultWindowHints();
        GLFW.glfwWindowHint(GLFW.GLFW_VISIBLE, GLFW.GLFW_TRUE);
        GLFW.glfwWindowHint(GLFW.GLFW_CONTEXT_VERSION_MAJOR, 3);
        GLFW.glfwWindowHint(GLFW.GLFW_CONTEXT_VERSION_MINOR, 2);
        GLFW.glfwWindowHint(GLFW.GLFW_OPENGL_PROFILE, GLFW.GLFW_OPENGL_CORE_PROFILE);
        final long window = GLFW.glfwCreateWindow(854, 480, "Passive jar check", 0L, 0L);
        if (window == 0L) {
            System.out.println("STAGE-GL=no-window");
            System.exit(3);
        }
        GLFW.glfwMakeContextCurrent(window);
        GL.createCapabilities();
        GLFW.glfwSwapInterval(0);
        System.out.println("STAGE-GL=ready");

        // Commands run on their own thread: the unload has to be observable
        // while frames keep presenting, which is also how the game sees it.
        final Thread commands = new Thread(() -> commandLoop(agent), "tyrant-jar-check-stdin");
        commands.setDaemon(true);
        commands.start();

        // GLFW wants its event pump and its swaps on the thread that made the
        // window, so the main thread stays here until "exit".
        while (running) {
            GLFW.glfwPollEvents();
            final int[] w = new int[1];
            final int[] h = new int[1];
            GLFW.glfwGetFramebufferSize(window, w, h);
            GL11.glViewport(0, 0, Math.max(w[0], 1), Math.max(h[0], 1));
            GL11.glClearColor(0.05f, 0.02f, 0.10f, 1.0f);
            GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
            GLFW.glfwSwapBuffers(window);
            FRAMES.incrementAndGet();
            Thread.sleep(16);
        }

        GLFW.glfwDestroyWindow(window);
        GLFW.glfwTerminate();
        System.out.println("STAGE-EXIT frames=" + FRAMES.get());
        System.out.flush();
        System.exit(0);
    }

    private static void commandLoop(Path agent) {
        try (BufferedReader in = new BufferedReader(new InputStreamReader(System.in))) {
            String line;
            while ((line = in.readLine()) != null) {
                final String command = line.trim();
                if (command.equals("status")) {
                    System.out.println("STAGE-MAPPED=" + PassiveNative.isLoaded());
                } else if (command.equals("frames")) {
                    System.out.println("STAGE-FRAMES=" + FRAMES.get());
                } else if (command.equals("unload")) {
                    final boolean ok = PassiveNative.unload();
                    System.out.println("STAGE-UNLOAD=" + ok);
                    if (!ok) System.out.println("STAGE-ERROR=" + PassiveNative.lastError());
                    System.out.println("STAGE-MAPPED=" + PassiveNative.isLoaded());
                } else if (command.equals("load")) {
                    final boolean ok = PassiveNative.load(agent.toAbsolutePath().toString());
                    System.out.println("STAGE-LOAD=" + ok);
                    if (!ok) System.out.println("STAGE-ERROR=" + PassiveNative.lastError());
                    System.out.println("STAGE-MAPPED=" + PassiveNative.isLoaded());
                } else if (command.equals("exit")) {
                    System.out.println("STAGE-CMD=exit");
                    running = false;
                    System.out.flush();
                    return;
                }
                System.out.flush();
            }
        } catch (IOException ignored) {
            // stdin closed: treat it as an exit request
        }
        running = false;
    }

    /** Unpacks the LWJGL native DLLs, the way a launcher would. */
    private static void extractNatives(Path jarPath, Path dest) throws IOException {
        try (ZipFile zip = new ZipFile(jarPath.toFile())) {
            final Enumeration<? extends ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                final ZipEntry entry = entries.nextElement();
                final String name = entry.getName();
                if (!name.toLowerCase(Locale.ROOT).endsWith(".dll")) continue;
                final Path out = dest.resolve(Paths.get(name).getFileName().toString());
                try (InputStream in = zip.getInputStream(entry)) {
                    Files.copy(in, out, StandardCopyOption.REPLACE_EXISTING);
                }
            }
        }
    }
}
