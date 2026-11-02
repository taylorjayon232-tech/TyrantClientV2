import java.io.File;

// Loads passive_agent.dll via System.load() — the closest analog to
// LoadLibraryW inside the JVM. Then stays alive so the agent threads run.
public class LoadTest {
    public static void main(String[] args) throws Exception {
        String dll = args.length > 0 ? args[0] : "../../build/Release/passive_agent.dll";
        System.out.println("[LoadTest] loading " + new File(dll).getAbsolutePath());
        try {
            System.load(new File(dll).getAbsolutePath());
            System.out.println("[LoadTest] load() returned OK");
        } catch (Throwable t) {
            System.out.println("[LoadTest] load FAILED: " + t);
            return;
        }
        Thread.sleep(6000);
        System.out.println("[LoadTest] survived 6s, exiting");
    }
}
