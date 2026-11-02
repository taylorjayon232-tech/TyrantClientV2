import java.io.File;
import java.lang.reflect.Field;

public class silent_rotation_jvm_check {
    private static native boolean install();
    private static native void update(boolean active, float yaw, float pitch);
    private static native boolean restore();
    private static void equal(float actual,float expected,String label) {
        if(Math.abs(actual-expected)>0.0001f) throw new AssertionError(label+": "+actual+" != "+expected);
    }
    public static void main(String[] args) throws Exception {
        System.load(new File(args[0]).getAbsolutePath());
        net.minecraft.class_746 local=new net.minecraft.class_746();
        net.minecraft.class_746 other=new net.minecraft.class_746();
        net.minecraft.class_310.method_1551().field_1724=local;
        local.cameraYaw=15; local.cameraPitch=20; other.cameraYaw=45;
        if(!install()) throw new AssertionError("install failed");
        update(true,90,-30);
        local.method_5724(1,new net.minecraft.class_243());
        equal(local.lastMovementYaw,90,"local movement");
        other.method_5724(1,new net.minecraft.class_243());
        equal(other.lastMovementYaw,45,"other movement");
        local.callPacket();
        equal(local.lastPacketYaw,90,"packet yaw"); equal(local.lastPacketPitch,-30,"packet pitch");
        equal(local.cameraYaw,15,"camera yaw"); equal(local.cameraPitch,20,"camera pitch");
        update(false,0,0);
        local.method_5724(1,new net.minecraft.class_243()); local.callPacket();
        equal(local.lastMovementYaw,15,"released movement"); equal(local.lastPacketYaw,15,"released packet");
        if(!restore()) throw new AssertionError("restore failed");
        update(true,120,60);
        local.method_5724(1,new net.minecraft.class_243()); local.callPacket();
        equal(local.lastMovementYaw,15,"restored movement"); equal(local.lastPacketYaw,15,"restored packet");
        System.out.println("live JVM movement, packet, camera and restore checks passed");
    }
}
