#include <android/log.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "SNFix/JNI", __VA_ARGS__)

class SafetyNetFix : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        auto process = env->GetStringUTFChars(args->nice_name, nullptr);

        if (process != nullptr && strncmp(process, "com.google.android.gms", 22) == 0) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (strcmp(process, "com.google.android.gms.unstable") == 0) {

                int fd = api->connectCompanion();
                read(fd, &bufferSize, sizeof(long));

                if (bufferSize > 0) {
                    buffer = static_cast<char *>(calloc(1, bufferSize));
                    read(fd, buffer, bufferSize);
                }

                close(fd);
            }
        }

        env->ReleaseStringUTFChars(args->nice_name, process);

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (buffer == nullptr || bufferSize < 1) return;

        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto buf = env->NewDirectByteBuffer(buffer, bufferSize);
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

        free(buffer);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    char *buffer = nullptr;
    long bufferSize = 0;
};

static void companion(int fd) {
    char *buffer = nullptr;
    long size = 0;

    FILE *file = fopen("/data/adb/modules/safetynet-fix/classes.dex", "rb");

    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);

        buffer = static_cast<char *>(calloc(1, size));
        fread(buffer, 1, size, file);

        fclose(file);
    }

    write(fd, &size, sizeof(long));
    write(fd, buffer, size);

    free(buffer);
}

REGISTER_ZYGISK_MODULE(SafetyNetFix)

REGISTER_ZYGISK_COMPANION(companion)