package com.passive.fabric.modules;

/**
 * One module setting, mirrored by the C++ interface.
 *
 * <p>The Java side owns the value; {@link ModuleManager#describe()} publishes these descriptors
 * once at startup so the ImGui config panel can draw the right widget, and the panel calls the
 * matching {@code ModuleManager.setSetting*} method when the value changes. Keeping the descriptor
 * here means a new setting needs no C++ edit at all.
 */
public final class Setting {

    public enum Type {
        BOOL,
        INT,
        FLOAT,
        MODE
    }

    private final String key;
    private final String label;
    private final Type type;
    private final int minInt;
    private final int maxInt;
    private final float minFloat;
    private final float maxFloat;
    private final String[] options;

    private boolean boolValue;
    private int intValue;
    private float floatValue;
    private int modeValue;

    private Setting(String key, String label, Type type, int minInt, int maxInt,
                    float minFloat, float maxFloat, String[] options,
                    boolean boolValue, int intValue, float floatValue, int modeValue) {
        this.key = key;
        this.label = label;
        this.type = type;
        this.minInt = minInt;
        this.maxInt = maxInt;
        this.minFloat = minFloat;
        this.maxFloat = maxFloat;
        this.options = options;
        this.boolValue = boolValue;
        this.intValue = intValue;
        this.floatValue = floatValue;
        this.modeValue = modeValue;
    }

    public static Setting bool(String key, String label, boolean value) {
        return new Setting(key, label, Type.BOOL, 0, 0, 0f, 0f, null, value, 0, 0f, 0);
    }

    public static Setting integer(String key, String label, int min, int max, int value) {
        return new Setting(key, label, Type.INT, min, max, 0f, 0f, null,
                           false, clamp(value, min, max), 0f, 0);
    }

    public static Setting decimal(String key, String label, float min, float max, float value) {
        return new Setting(key, label, Type.FLOAT, 0, 0, min, max, null,
                           false, 0, Math.max(min, Math.min(max, value)), 0);
    }

    public static Setting mode(String key, String label, String[] options, int value) {
        return new Setting(key, label, Type.MODE, 0, 0, 0f, 0f, options.clone(),
                           false, 0, 0f, clamp(value, 0, Math.max(0, options.length - 1)));
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }

    public String key() {
        return key;
    }

    public String label() {
        return label;
    }

    public Type type() {
        return type;
    }

    public int minInt() {
        return minInt;
    }

    public int maxInt() {
        return maxInt;
    }

    public float minFloat() {
        return minFloat;
    }

    public float maxFloat() {
        return maxFloat;
    }

    public String[] options() {
        return options;
    }

    public boolean boolValue() {
        return boolValue;
    }

    public int intValue() {
        return intValue;
    }

    public float floatValue() {
        return floatValue;
    }

    public int modeValue() {
        return modeValue;
    }

    public void setBool(boolean value) {
        boolValue = value;
    }

    public void setInt(int value) {
        intValue = clamp(value, minInt, maxInt);
    }

    public void setFloat(float value) {
        floatValue = Math.max(minFloat, Math.min(maxFloat, value));
    }

    public void setMode(int value) {
        modeValue = clamp(value, 0, Math.max(0, options == null ? 0 : options.length - 1));
    }
}
