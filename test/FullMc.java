public class FullMc {
    public static void main(String[] args) throws Exception {
        System.out.println("[FullMc] alive, pid=" + ProcessHandle.current().pid());
        // Order of Main.main (from bytecode): class_155.method_36208 (adds SystemDetails),
        // class_156.method_18350 (Util.log?/setup), Bootstrap.logWorking builds registries.
        try { Class.forName("net.minecraft.class_155").getMethod("method_36208").invoke(null); System.out.println("[FullMc] class_155.method_36208 ok"); }
        catch (java.lang.reflect.InvocationTargetException it) { System.out.println("[FullMc] class_155 root: " + it.getCause()); }
        catch (Throwable t) { System.out.println("[FullMc] class_155: " + t); }
        try { Class.forName("net.minecraft.class_2966").getMethod("method_12851").invoke(null); System.out.println("[FullMc] logWorking ok"); }
        catch (java.lang.reflect.InvocationTargetException it) { System.out.println("[FullMc] logWorking root: " + it.getCause()); }
        catch (Throwable t) { System.out.println("[FullMc] logWorking: " + t); }
        for (String c : new String[]{"net.minecraft.class_1297","net.minecraft.class_1299","net.minecraft.class_1802","net.minecraft.class_638","net.minecraft.class_310","net.minecraft.class_7923","net.minecraft.class_2398"}) {
            try { Class.forName(c); System.out.println("[FullMc] " + c + " OK"); }
            catch (Throwable t) { System.out.println("[FullMc] " + c + " FAIL: " + t.getCause()); }
        }
        Thread.sleep(300000);
    }
}
