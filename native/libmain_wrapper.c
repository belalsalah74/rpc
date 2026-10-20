/*
 * Standalone Morphe bootstrap loader for Pokémon GO 0.431.0 ARM64.
 * The RPC probe payload is optional and is copied into the APK only by its patch.
 */
typedef struct JavaVM JavaVM;
typedef int jint;

extern void *dlopen(const char *filename, int flags);
extern void *dlsym(void *handle, const char *symbol);
extern int __android_log_print(int, const char *, const char *, ...);

#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define JNI_VERSION_1_6 0x00010006
#define LOGE(...) __android_log_print(6, "PGORPCLOADER", __VA_ARGS__)
#define LOGI(...) __android_log_print(4, "PGORPCLOADER", __VA_ARGS__)

typedef jint (*jni_onload_t)(JavaVM *, void *);

__attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGI("RPC probe bootstrap entered");
    void *orig = dlopen("libmain_orig.so", RTLD_NOW | RTLD_LOCAL);
    if (!orig) {
        LOGE("failed to load libmain_orig.so");
        return -1;
    }

    jni_onload_t fn = (jni_onload_t)dlsym(orig, "JNI_OnLoad");
    jint version = JNI_VERSION_1_6;
    if (fn) {
        version = fn(vm, reserved);
        if (version < 0) {
            LOGE("original JNI_OnLoad failed: %d", version);
            return version;
        }
    } else {
        LOGI("original library has no JNI_OnLoad; continuing with JNI 1.6");
    }

    void *probe = dlopen("libpgo_rpc_probe.so", RTLD_NOW | RTLD_LOCAL);
    if (!probe) {
        LOGI("RPC probe payload not selected or unable to load");
    } else {
        LOGI("RPC probe payload loaded");
    }
    return version;
}
