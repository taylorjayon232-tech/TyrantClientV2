package passive.internal;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

public final class SilentRotationHelper {
    public static volatile boolean active;
    public static volatile float yaw;
    public static volatile float pitch;
    private static final Class<?> ENTITY;
    private static final Method GET_YAW;
    private static final Method GET_PITCH;
    private static final Object CLIENT;
    private static final Field PLAYER;

    static {
        try {
            ClassLoader loader = SilentRotationHelper.class.getClassLoader();
            ENTITY = loader.loadClass("net.minecraft.class_1297");
            Class<?> client = loader.loadClass("net.minecraft.class_310");
            GET_YAW = ENTITY.getMethod("method_36454");
            GET_PITCH = ENTITY.getMethod("method_36455");
            CLIENT = client.getMethod("method_1551").invoke(null);
            PLAYER = client.getField("field_1724");
        } catch (ReflectiveOperationException ex) {
            throw new ExceptionInInitializerError(ex);
        }
    }

    private static boolean isLocal(Object entity) {
        if (!active) return false;
        try { return entity == PLAYER.get(CLIENT); }
        catch (ReflectiveOperationException ex) { throw new IllegalStateException(ex); }
    }
    private static float read(Method method, Object entity) {
        try { return ((Float)method.invoke(entity)).floatValue(); }
        catch (ReflectiveOperationException ex) { throw new IllegalStateException(ex); }
    }
    public static float movementYaw(Object entity) {
        return isLocal(entity) ? yaw : read(GET_YAW, entity);
    }
    public static float packetYaw(Object entity) {
        return isLocal(entity) ? yaw : read(GET_YAW, entity);
    }
    public static float packetPitch(Object entity) {
        return isLocal(entity) ? pitch : read(GET_PITCH, entity);
    }
}
