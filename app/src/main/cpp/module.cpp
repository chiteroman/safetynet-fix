#include <android/log.h>
#include <unistd.h>
#include <string>
#include <vector>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "SNFix/JNI", __VA_ARGS__)

#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

class SafetyNetFix : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        std::string process(rawProcess);
        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (!process.starts_with("com.google.android.gms")) return;

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (process != "com.google.android.gms.unstable") return;

        long size = 0;
        int fd = api->connectCompanion();

        read(fd, &size, sizeof(size));

        if (size > 0) {
            vector.resize(size);
            read(fd, vector.data(), size);
        } else {
            LOGD("Couldn't read classes.dex from file descriptor!");
        }

        close(fd);
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (vector.empty()) return;

        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create buffer");
        auto buf = env->NewDirectByteBuffer(vector.data(), vector.size());
        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto dexCl = env->NewObject(dexClClass, dexClInit, buf, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.safetynetfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        LOGD("call init");
        auto entryClass = (jclass) entryClassObj;
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<unsigned char> vector;
};

static void companion(int fd) {
    long size = 0;
    std::vector<unsigned char> vector;

    FILE *file = fopen("/data/adb/modules/safetynet-fix/classes.dex", "rb");

    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);

        vector.resize(size);
        fread(vector.data(), 1, size, file);

        fclose(file);
    }

    write(fd, &size, sizeof(size));
    write(fd, vector.data(), size);
}

REGISTER_ZYGISK_MODULE(SafetyNetFix)

REGISTER_ZYGISK_COMPANION(companion)