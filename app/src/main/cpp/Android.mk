LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libcxx
LOCAL_SRC_FILES := $(LOCAL_PATH)/libcxx/$(TARGET_ARCH_ABI).a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/libcxx/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := safetynetfix
LOCAL_SRC_FILES := module.cpp
LOCAL_STATIC_LIBRARIES := libcxx
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)