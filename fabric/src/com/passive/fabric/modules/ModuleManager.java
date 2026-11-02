package com.passive.fabric.modules;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * The Java module host.
 *
 * <p>{@code passive_agent.dll} loads this class through the game's own class loader on the first
 * rendered frame, registers {@link NativeApi}, reads {@link #describe()} to build the C++ module
 * cards, and calls {@link #tick(float)} once per frame. The interface and keybinds still live on the
 * native side; this class is only the registry and the per-frame loop.
 *
 * <p>All entry points run on the game's render thread. A module that throws is logged and skipped,
 * never allowed to unwind into the game.
 */
public final class ModuleManager {

    private static final ModuleManager INSTANCE = new ModuleManager();

    private final Map<String, Module> modules = new LinkedHashMap<>();

    private ModuleManager() {
        register(new SprintModule());
        register(new AutoJumpResetModule());
    }

    public static ModuleManager getInstance() {
        return INSTANCE;
    }

    private void register(Module module) {
        modules.put(module.name(), module);
    }

    /** Runs every enabled module. Called once per rendered frame by the native client. */
    public void tick(float dt) {
        for (Module module : modules.values()) {
            if (!module.isEnabled()) continue;
            try {
                module.tick(dt);
            } catch (Throwable failure) {
                NativeApi.log("module " + module.name() + " threw: " + failure);
            }
        }
    }

    /** Called by the native card switch / keybind (and by config load through the mirrors). */
    public void setEnabled(String name, boolean on) {
        final Module module = modules.get(name);
        if (module != null) module.setEnabled(on);
    }

    public boolean isEnabled(String name) {
        final Module module = modules.get(name);
        return module != null && module.isEnabled();
    }

    /** Disables everything, releasing keys held by a module. Called before the DLL unloads. */
    public void shutdown() {
        for (Module module : modules.values()) {
            try {
                module.setEnabled(false);
            } catch (Throwable failure) {
                NativeApi.log("module " + module.name() + " failed to shut down: " + failure);
            }
        }
    }

    // ---- settings ---------------------------------------------------------------------------

    private Setting setting(String moduleName, String key) {
        final Module module = modules.get(moduleName);
        if (module == null) return null;
        for (Setting setting : module.settings()) {
            if (setting.key().equals(key)) return setting;
        }
        return null;
    }

    public void setSettingBool(String module, String key, boolean value) {
        final Setting setting = setting(module, key);
        if (setting != null) setting.setBool(value);
    }

    public void setSettingInt(String module, String key, int value) {
        final Setting setting = setting(module, key);
        if (setting != null) setting.setInt(value);
    }

    public void setSettingFloat(String module, String key, float value) {
        final Setting setting = setting(module, key);
        if (setting != null) setting.setFloat(value);
    }

    public void setSettingMode(String module, String key, int value) {
        final Setting setting = setting(module, key);
        if (setting != null) setting.setMode(value);
    }

    // ---- descriptor -------------------------------------------------------------------------

    /**
     * JSON array of every module and its settings, read once by the native host:
     * {@code [{name,category,description,tag,enabled,settings:[{key,label,type,value,...}]}]}.
     */
    public String describe() {
        final StringBuilder out = new StringBuilder(512);
        out.append('[');
        boolean firstModule = true;
        for (Module module : modules.values()) {
            if (!firstModule) out.append(',');
            firstModule = false;
            out.append("{\"name\":\"").append(esc(module.name()))
               .append("\",\"category\":\"").append(esc(module.category()))
               .append("\",\"description\":\"").append(esc(module.description()))
               .append("\",\"tag\":\"").append(esc(module.tag()))
               .append("\",\"enabled\":").append(module.isEnabled())
               .append(",\"settings\":[");
            boolean firstSetting = true;
            for (Setting setting : module.settings()) {
                if (!firstSetting) out.append(',');
                firstSetting = false;
                out.append("{\"key\":\"").append(esc(setting.key()))
                   .append("\",\"label\":\"").append(esc(setting.label()))
                   .append("\",\"type\":\"").append(typeName(setting.type())).append('"');
                switch (setting.type()) {
                    case BOOL -> out.append(",\"value\":").append(setting.boolValue());
                    case INT -> out.append(",\"value\":").append(setting.intValue())
                                   .append(",\"min\":").append(setting.minInt())
                                   .append(",\"max\":").append(setting.maxInt());
                    case FLOAT -> out.append(",\"value\":").append(setting.floatValue())
                                     .append(",\"min\":").append(setting.minFloat())
                                     .append(",\"max\":").append(setting.maxFloat());
                    case MODE -> {
                        out.append(",\"value\":").append(setting.modeValue()).append(",\"options\":[");
                        final String[] options = setting.options();
                        for (int i = 0; i < options.length; ++i) {
                            if (i > 0) out.append(',');
                            out.append('"').append(esc(options[i])).append('"');
                        }
                        out.append(']');
                    }
                }
                out.append('}');
            }
            out.append("]}");
        }
        out.append(']');
        return out.toString();
    }

    private static String typeName(Setting.Type type) {
        return switch (type) {
            case BOOL -> "bool";
            case INT -> "int";
            case FLOAT -> "float";
            case MODE -> "mode";
        };
    }

    /** Minimal JSON string escaping: the descriptor only ever carries ASCII metadata. */
    private static String esc(String text) {
        final StringBuilder out = new StringBuilder(text.length() + 8);
        for (int i = 0; i < text.length(); ++i) {
            final char c = text.charAt(i);
            switch (c) {
                case '"' -> out.append("\\\"");
                case '\\' -> out.append("\\\\");
                case '\n' -> out.append("\\n");
                case '\r' -> out.append("\\r");
                case '\t' -> out.append("\\t");
                default -> {
                    if (c < 0x20) out.append(String.format("\\u%04x", (int)c));
                    else out.append(c);
                }
            }
        }
        return out.toString();
    }
}
