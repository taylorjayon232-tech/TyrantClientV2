import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.File;

// Minimal GL host: a Swing window with a JOGL-like GL panel is overkill —
// instead we start the REAL Minecraft client jar in-process on the main
// thread (Fabric loader entrypoint). Passive's agent is then LoadLibrary'd
// by the Injector exactly like production.
public class Host {
    public static void main(String[] args) throws Exception {
        System.out.println("[Host] alive, pid=" + ProcessHandle.current().pid());
        JFrame f = new JFrame("PassiveHost");
        f.setSize(300, 200);
        f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        f.setVisible(true);
        // keep alive
        Thread.currentThread().join();
    }
}
