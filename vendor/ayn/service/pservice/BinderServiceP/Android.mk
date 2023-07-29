LOCAL_PATH:=$(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:=BinderServiceP.cpp
LOCAL_SHARED_LIBRARIES:=libutils libbinder liblog
LOCAL_MODULE_TAGS:=optional
LOCAL_MODULE:=libBinderServiceP
LOCAL_PRELINK_MODULE:=false
LOCAL_LDLIBS = -llog -landroid
include $(BUILD_SHARED_LIBRARY)