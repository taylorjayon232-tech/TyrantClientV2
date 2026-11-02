// ============================================================================
//  Passive — item & armor icons for Nametags.
//
//  The jar that ships with the game already contains every icon as a PNG
//  (assets/minecraft/textures/item/diamond_helmet.png). We ask the JVM for the
//  raw bytes through the class loader and decompress them with the JVM's own
//  java.util.zip.Inflater, then decode the PNG ourselves (see png_image.h) and
//  upload one GL texture per icon.
//
//  Only java.base is used — no Minecraft mappings, no AWT, no ImageIO, no
//  bundled zlib. That matters because this runs inside the game's JVM: keeping
//  to classes that are always present, and caching every jclass as a GLOBAL
//  reference (a local reference dies with the frame that created it), is what
//  keeps the loader from being able to take the process down.
//
//  Everything is cached for the session, failures included, so a missing or
//  unreadable texture costs one attempt and then stays a cheap 0 while the
//  caller keeps drawing its text fallback.
// ============================================================================
#include "itemicons.h"
#include "png_image.h"
#include "glex.h"          // windows.h + GL + the GL3 loader (must come first)

#include "../jni/jvm.h"
#include "../core/agent.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace tyr {
namespace itemicons {

namespace {

struct Cached {
    ImTextureID tex = 0;
    bool        resolved = false;   // a lookup ran (success or failure)
};

std::unordered_map<std::string, Cached> g_cache;
bool g_jdkMissing = false;          // java.base handles missing: stop asking entirely
bool g_loadedLogged = false;

// ---- one-time JDK handle resolution ---------------------------------------
// Every class reference handed out by jni::findClass is a LOCAL reference that
// dies with the frame, so each one is promoted to a global here.
struct Jdk {
    bool ok = false;
    jclass classLoader = nullptr;      // java.lang.ClassLoader
    jclass inflater = nullptr;         // java.util.zip.Inflater
    jmethodID getSystemResourceAsStream = nullptr;
    jmethodID getResourceAsStream = nullptr;
    jmethodID readAllBytes = nullptr;
    jmethodID streamClose = nullptr;
    jmethodID inflaterCtor = nullptr;
    jmethodID setInput = nullptr;
    jmethodID inflate = nullptr;
    jmethodID finished = nullptr;
    jmethodID end = nullptr;
};

jclass promoteToGlobal(JNIEnv* env, jclass local) {
    if (!local) return nullptr;
    jclass global = (jclass)env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    return global;
}

Jdk& jdk() {
    static Jdk s;
    static bool s_tried = false;
    if (s_tried) return s;
    s_tried = true;

    JNIEnv* env = jni::env();
    if (!env) return s;

    s.classLoader = promoteToGlobal(env, jni::findClass(env, "java.lang.ClassLoader"));
    s.inflater    = promoteToGlobal(env, jni::findClass(env, "java.util.zip.Inflater"));
    jclass inputStream = jni::findClass(env, "java.io.InputStream");
    if (!s.classLoader || !s.inflater || !inputStream) {
        if (inputStream) env->DeleteLocalRef(inputStream);
        jni::clearException(env, "itemicons classes");
        trace::log("itemicons: java.base image classes unavailable, keeping text labels");
        return s;
    }

    s.getSystemResourceAsStream = env->GetStaticMethodID(
        s.classLoader, "getSystemResourceAsStream",
        "(Ljava/lang/String;)Ljava/io/InputStream;");
    s.getResourceAsStream = env->GetMethodID(
        s.classLoader, "getResourceAsStream",
        "(Ljava/lang/String;)Ljava/io/InputStream;");
    s.readAllBytes = env->GetMethodID(inputStream, "readAllBytes", "()[B");
    s.streamClose  = env->GetMethodID(inputStream, "close", "()V");
    s.inflaterCtor = env->GetMethodID(s.inflater, "<init>", "()V");
    s.setInput     = env->GetMethodID(s.inflater, "setInput", "([B)V");
    s.inflate      = env->GetMethodID(s.inflater, "inflate", "([B)I");
    s.finished     = env->GetMethodID(s.inflater, "finished", "()Z");
    s.end          = env->GetMethodID(s.inflater, "end", "()V");
    env->DeleteLocalRef(inputStream);
    jni::clearException(env, "itemicons methods");

    s.ok = s.getSystemResourceAsStream && s.getResourceAsStream && s.readAllBytes &&
           s.streamClose && s.inflaterCtor && s.setInput && s.inflate && s.finished && s.end;
    if (!s.ok)
        trace::log("itemicons: java.base image methods unavailable, keeping text labels");
    return s;
}

// Whole resource as bytes (ClassLoader.getSystemResourceAsStream -> readAllBytes).
bool readResource(const std::string& resourcePath, std::vector<unsigned char>& out) {
    JNIEnv* env = jni::env();
    Jdk& k = jdk();
    if (!env || !k.ok) return false;

    jstring path = jni::toJString(env, resourcePath.c_str());
    if (!path) return false;
    // The game's own loader sees the client jar (and therefore its assets); the
    // system loader is the fallback for launchers that put the jar elsewhere.
    jobject stream = nullptr;
    jobject gameLoader = jni::classLoader();
    if (gameLoader) {
        stream = env->CallObjectMethod(gameLoader, k.getResourceAsStream, path);
        if (env->ExceptionCheck()) { jni::clearException(env, "getResourceAsStream"); stream = nullptr; }
    }
    if (!stream) {
        stream = env->CallStaticObjectMethod(k.classLoader, k.getSystemResourceAsStream, path);
        if (env->ExceptionCheck()) { jni::clearException(env, "getSystemResourceAsStream"); stream = nullptr; }
    }
    env->DeleteLocalRef(path);
    if (!stream) return false;                       // not in this classpath

    jbyteArray bytes = (jbyteArray)env->CallObjectMethod(stream, k.readAllBytes);
    if (env->ExceptionCheck()) { jni::clearException(env, "readAllBytes"); bytes = nullptr; }
    env->CallVoidMethod(stream, k.streamClose);      // never leave the stream open
    if (env->ExceptionCheck()) jni::clearException(env, "stream close");
    env->DeleteLocalRef(stream);
    if (!bytes) return false;

    const jsize size = env->GetArrayLength(bytes);
    if (size <= 0 || size > 8 * 1024 * 1024) { env->DeleteLocalRef(bytes); return false; }
    out.resize((size_t)size);
    env->GetByteArrayRegion(bytes, 0, size, (jbyte*)out.data());
    env->DeleteLocalRef(bytes);
    if (env->ExceptionCheck()) { jni::clearException(env, "GetByteArrayRegion"); out.clear(); return false; }
    return true;
}

// Zlib stream -> raw bytes, using the JVM's own Inflater (java.base).
bool inflateStream(const std::vector<unsigned char>& zlibData, std::vector<unsigned char>& out) {
    JNIEnv* env = jni::env();
    Jdk& k = jdk();
    if (!env || !k.ok || zlibData.empty()) return false;

    jbyteArray input = env->NewByteArray((jsize)zlibData.size());
    if (!input) return false;
    env->SetByteArrayRegion(input, 0, (jsize)zlibData.size(), (const jbyte*)zlibData.data());
    if (env->ExceptionCheck()) { jni::clearException(env, "SetByteArrayRegion"); env->DeleteLocalRef(input); return false; }

    jobject inflater = env->NewObject(k.inflater, k.inflaterCtor);
    if (!inflater) { env->DeleteLocalRef(input); return false; }

    env->CallVoidMethod(inflater, k.setInput, input);
    env->DeleteLocalRef(input);
    if (env->ExceptionCheck()) { jni::clearException(env, "Inflater.setInput"); env->DeleteLocalRef(inflater); return false; }

    bool ok = false;
    std::vector<unsigned char> chunk(64 * 1024);
    jbyteArray buffer = env->NewByteArray((jsize)chunk.size());
    if (buffer) {
        ok = true;
        for (int pass = 0; pass < 512; ++pass) {          // 512 * 64 KiB cap
            const jint written = env->CallIntMethod(inflater, k.inflate, buffer);
            if (env->ExceptionCheck()) { jni::clearException(env, "Inflater.inflate"); ok = false; break; }
            if (written > 0) {
                const size_t at = out.size();
                out.resize(at + (size_t)written);
                env->GetByteArrayRegion(buffer, 0, written, (jbyte*)(out.data() + at));
                if (env->ExceptionCheck()) { jni::clearException(env, "inflate copy"); ok = false; break; }
            }
            const jboolean done = env->CallBooleanMethod(inflater, k.finished);
            if (env->ExceptionCheck()) { jni::clearException(env, "Inflater.finished"); ok = false; break; }
            if (done == JNI_TRUE) break;
            if (written <= 0) { ok = false; break; }       // no progress: truncated stream
        }
        env->DeleteLocalRef(buffer);
    } else {
        ok = false;
    }

    env->CallVoidMethod(inflater, k.end);                 // release the native zlib state
    if (env->ExceptionCheck()) jni::clearException(env, "Inflater.end");
    env->DeleteLocalRef(inflater);
    return ok && !out.empty();
}

// Full pipeline for one resource path: read, inflate, decode.
bool decodeResource(const std::string& resourcePath, pngimg::Image& out) {
    std::vector<unsigned char> fileBytes;
    if (!readResource(resourcePath, fileBytes)) return false;

    pngimg::Parsed parsed;
    if (!pngimg::parseChunks(fileBytes.data(), fileBytes.size(), parsed)) return false;

    std::vector<unsigned char> raw;
    if (!inflateStream(parsed.idat, raw)) return false;

    // Rows are stored top-first; GL wants the first row at the bottom.
    return pngimg::toRgba(parsed, raw, out, true);
}

ImTextureID upload(const std::vector<unsigned char>& rgba, int w, int h) {
    if (!tyr::glx::g_glexReady || rgba.empty() || w <= 0 || h <= 0) return 0;
    if (rgba.size() < (size_t)w * (size_t)h * 4) return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) return 0;
    GLint prevAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    // Nearest keeps the pixel-art icons crisp; mipmaps are unnecessary at these sizes.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlignment);
    return (ImTextureID)(ImU64)tex;
}

// Loads the first candidate path that exists. Cached either way.
ImTextureID loadCandidates(const std::string& key, const std::vector<std::string>& candidates) {
    Cached& slot = g_cache[key];
    if (slot.resolved) return slot.tex;
    slot.resolved = true;

    if (g_jdkMissing) return 0;
    if (!jni::ready()) { slot.resolved = false; return 0; }   // try again next frame
    if (!jdk().ok) { g_jdkMissing = true; return 0; }

    for (const std::string& path : candidates) {
        pngimg::Image image;
        if (!decodeResource(path, image)) continue;
        slot.tex = upload(image.rgba, image.width, image.height);
        if (slot.tex) {
            if (!g_loadedLogged) {
                g_loadedLogged = true;
                trace::log("itemicons: loaded %s (%dx%d)", path.c_str(), image.width, image.height);
            }
            return slot.tex;
        }
    }
    return 0;
}

} // namespace

ImTextureID get(const std::string& itemId) {
    if (itemId.empty()) return 0;
    // Item ids reach us as registry paths ("minecraft:diamond_helmet") or bare
    // paths ("diamond_helmet"); the asset tree only ever uses the bare path.
    std::string path = itemId;
    const size_t colon = path.find(':');
    if (colon != std::string::npos) path = path.substr(colon + 1);
    if (path.empty()) return 0;
    // A few items have no item texture of their own: the game builds their inventory icon from a
    // different texture tree through the item model. The shield is the one that showed up as text
    // in the HUD ("Shield") because of it - its icon is the shield model, a base plate plus the
    // wood pattern - so the base plate is used as the icon's closest single texture.
    if (path == "shield") {
        return loadCandidates("item/shield",
                              { "assets/minecraft/textures/entity/shield_base.png",
                                "assets/minecraft/textures/entity/shield_base_nopattern.png",
                                "assets/minecraft/textures/item/shield.png" });
    }
    // The item tree only holds the items that have an inventory texture of their own. A block
    // item's icon is its *block* texture, which is why a cobweb used to fall back to the word
    // "Cobweb": there is no textures/item/cobweb.png at all. The block tree is the same image
    // every resource pack retextures, so this is texture-pack aware as well.
    //
    // The suffixed variants matter for the same reason: grass_block, oak_log, water and a slab
    // have no unsuffixed block texture, only _top / _side / _front / _still. Trying the handful
    // of names the game itself uses covers every block item without a per-item table.
    static const char* const kBlockSuffixes[] = { "", "_top", "_side", "_front", "_still", "_animated" };
    std::vector<std::string> candidates;
    candidates.reserve(1 + 6);
    candidates.push_back("assets/minecraft/textures/item/" + path + ".png");
    for (const char* suffix : kBlockSuffixes)
        candidates.push_back("assets/minecraft/textures/block/" + path + suffix + ".png");
    return loadCandidates("item/" + path, candidates);
}

ImTextureID getTrim(const std::string& patternId) {
    if (patternId.empty()) return 0;
    std::string path = patternId;
    const size_t colon = path.find(':');
    if (colon != std::string::npos) path = path.substr(colon + 1);
    if (path.empty()) return 0;
    // Only the item-side trim texture is a 16x16 overlay that can be drawn on top of an
    // item icon. The model-side sheet (textures/trims/models/armor/*.png) is a full armour
    // layer texture, and drawing it over a helmet icon is what made the armour textures
    // look scrambled — so it is deliberately not a fallback any more.
    return loadCandidates("trim/" + path,
                          { "assets/minecraft/textures/trims/items/" + path + ".png" });
}

PotionIcon potion(const std::string& itemId, const std::string& potionId, ImU32 customTint) {
    PotionIcon out;
    const std::string base = potionBaseId(itemId);
    if (base.empty()) return out;
    // Base and overlay are cached as their own keys: a splash and a lingering potion share the
    // overlay name only with their own family, and a plain potion's bottle is one texture.
    out.base = loadCandidates("potion/base/" + base,
                              { "assets/minecraft/textures/item/" + base + ".png" });
    // The liquid is a separate, greyscale overlay the game tints. A pack may ship only the
    // bottle, in which case the plain bottle still draws (a 0 overlay is skipped by the caller).
    out.overlay = loadCandidates("potion/overlay/" + base,
                                 { "assets/minecraft/textures/item/" + base + "_overlay.png" });
    out.tint = potionTint(potionId, customTint);
    return out;
}

void shutdown() {
    if (tyr::glx::g_glexReady) {
        for (auto& pair : g_cache)
            if (pair.second.tex) {
                GLuint tex = (GLuint)(ImU64)pair.second.tex;
                glDeleteTextures(1, &tex);
            }
    }
    g_cache.clear();
    g_jdkMissing = false;
    g_loadedLogged = false;
    // The cached class references live as long as this DLL is loaded; the last
    // frame may still be using them, so they are intentionally kept.
}

} // namespace itemicons
} // namespace tyr
