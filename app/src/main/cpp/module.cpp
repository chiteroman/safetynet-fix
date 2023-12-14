#include <android/log.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

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

        auto name = env->GetStringUTFChars(args->nice_name, nullptr);

        if (name && (strncmp(name, "com.google.android.gms", 22) == 0)) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (strcmp(name, "com.google.android.gms.unstable") == 0) {

                int fd = api->connectCompanion();

                read(fd, &bufferSize, sizeof(bufferSize));

                if (bufferSize > 0) {
                    buffer = new unsigned char[bufferSize];
                    read(fd, buffer, bufferSize);
                } else {
                    LOGD("Couldn't read classes.dex from file descriptor!");
                }

                close(fd);
            }
        }

        env->ReleaseStringUTFChars(args->nice_name, name);
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (buffer == nullptr || bufferSize < 1) return;

        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create buffer");
        auto buf = env->NewDirectByteBuffer(buffer, bufferSize);
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

        delete[] buffer;
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    long bufferSize = 0;
    unsigned char *buffer = nullptr;
};

static void companion(int fd) {
    long size = 0;
    unsigned char *buffer = nullptr;

    FILE *file = fopen("/data/adb/modules/safetynet-fix/classes.dex", "rb");

    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);

        buffer = new unsigned char[size];
        fread(buffer, 1, size, file);

        fclose(file);
    }

    write(fd, &size, sizeof(size));
    write(fd, buffer, size);

    delete[] buffer;
}

REGISTER_ZYGISK_MODULE(SafetyNetFix)

REGISTER_ZYGISK_COMPANION(companion)