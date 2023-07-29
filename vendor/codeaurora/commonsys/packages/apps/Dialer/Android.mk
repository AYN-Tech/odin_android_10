# Local modifications:
# * removed com.google.android.geo.API_KEY key. This should be added to
#      the manifest files in java/com/android/incallui/calllocation/impl/
#      and /java/com/android/incallui/maps/impl/
# * b/62417801 modify translation string naming convention:
#      $ find . -type d | grep 262 | rename 's/(values)\-([a-zA-Z\+\-]+)\-(mcc262-mnc01)/$1-$3-$2/'
# * b/37077388 temporarily disable proguard with javac
# * b/62875795 include manually generated GRPC service class:
#      $ protoc --plugin=protoc-gen-grpc-java=prebuilts/tools/common/m2/repository/io/grpc/protoc-gen-grpc-java/1.0.3/protoc-gen-grpc-java-1.0.3-linux-x86_64.exe \
#               --grpc-java_out=lite:"packages/apps/Dialer/java/com/android/voicemail/impl/" \
#               --proto_path="packages/apps/Dialer/java/com/android/voicemail/impl/transcribe/grpc/" "packages/apps/Dialer/java/com/android/voicemail/impl/transcribe/grpc/voicemail_transcription.proto"
ifeq ($(TARGET_FWK_SUPPORTS_FULL_VALUEADDS), true)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# The base directory for Dialer sources.
BASE_DIR := java/com/android

# Exclude files incompatible with AOSP.
EXCLUDE_FILES := \
	$(BASE_DIR)/incallui/calllocation/impl/AuthException.java \
	$(BASE_DIR)/incallui/calllocation/impl/CallLocationImpl.java \
	$(BASE_DIR)/incallui/calllocation/impl/CallLocationModule.java \
	$(BASE_DIR)/incallui/calllocation/impl/DownloadMapImageTask.java \
	$(BASE_DIR)/incallui/calllocation/impl/GoogleLocationSettingHelper.java \
	$(BASE_DIR)/incallui/calllocation/impl/HttpFetcher.java \
	$(BASE_DIR)/incallui/calllocation/impl/LocationFragment.java \
	$(BASE_DIR)/incallui/calllocation/impl/LocationHelper.java \
	$(BASE_DIR)/incallui/calllocation/impl/LocationPresenter.java \
	$(BASE_DIR)/incallui/calllocation/impl/LocationUrlBuilder.java \
	$(BASE_DIR)/incallui/calllocation/impl/ReverseGeocodeTask.java \
	$(BASE_DIR)/incallui/calllocation/impl/TrafficStatsTags.java \
	$(BASE_DIR)/incallui/maps/impl/MapsImpl.java \
	$(BASE_DIR)/incallui/maps/impl/MapsModule.java \
	$(BASE_DIR)/incallui/maps/impl/StaticMapFragment.java \

# Exclude testing only class, not used anywhere here
EXCLUDE_FILES += \
	$(BASE_DIR)/contacts/common/format/testing/SpannedTestUtils.java

# Exclude rootcomponentgenerator
EXCLUDE_FILES += \
	$(call all-java-files-under, $(BASE_DIR)/dialer/rootcomponentgenerator) \
	$(call all-java-files-under, $(BASE_DIR)/dialer/inject/demo)

# Exclude build variants for now
EXCLUDE_FILES += \
	$(BASE_DIR)/dialer/constants/googledialer/ConstantsImpl.java \
	$(BASE_DIR)/dialer/binary/google/GoogleStubDialerRootComponent.java \
	$(BASE_DIR)/dialer/binary/google/GoogleStubDialerApplication.java \

# * b/62875795
ifneq ($(wildcard packages/apps/Dialer/java/com/android/voicemail/impl/com/google/internal/communications/voicemailtranscription/v1/VoicemailTranscriptionServiceGrpc.java),)
$(error Please remove file packages/apps/Dialer/java/com/android/voicemail/impl/com/google/internal/communications/voicemailtranscription/v1/VoicemailTranscriptionServiceGrpc.java )
endif

EXCLUDE_RESOURCE_DIRECTORIES := \
	java/com/android/incallui/maps/impl/res \

# All Dialers resources.
RES_DIRS := $(call all-subdir-named-dirs,res,.)
RES_DIRS := $(filter-out $(EXCLUDE_RESOURCE_DIRECTORIES),$(RES_DIRS))

EXCLUDE_MANIFESTS := \
	$(BASE_DIR)/dialer/binary/aosp/testing/AndroidManifest.xml \
	$(BASE_DIR)/dialer/binary/google/AndroidManifest.xml \
	$(BASE_DIR)/incallui/calllocation/impl/AndroidManifest.xml \
	$(BASE_DIR)/incallui/maps/impl/AndroidManifest.xml \

# Dialer manifest files to merge.
DIALER_MANIFEST_FILES := $(call all-named-files-under,AndroidManifest.xml,.)
DIALER_MANIFEST_FILES := $(filter-out $(EXCLUDE_MANIFESTS),$(DIALER_MANIFEST_FILES))

# Merge all manifest files.
LOCAL_FULL_LIBS_MANIFEST_FILES := \
	$(addprefix $(LOCAL_PATH)/, $(DIALER_MANIFEST_FILES))

LOCAL_SRC_FILES := $(call all-java-files-under, $(BASE_DIR))
LOCAL_SRC_FILES += $(call all-proto-files-under, $(BASE_DIR))
LOCAL_SRC_FILES += $(call all-Iaidl-files-under, $(BASE_DIR))
LOCAL_SRC_FILES := $(filter-out $(EXCLUDE_FILES),$(LOCAL_SRC_FILES))

LOCAL_AIDL_INCLUDES := $(call all-Iaidl-files-under, $(BASE_DIR))

LOCAL_PROTOC_FLAGS := --proto_path=$(LOCAL_PATH)

LOCAL_RESOURCE_DIR := $(addprefix $(LOCAL_PATH)/, $(RES_DIRS))

EXCLUDE_EXTRA_PACKAGES := \
	com.android.dialer.binary.aosp.testing \
	com.android.dialer.binary.google \
	com.android.incallui.calllocation.impl \
	com.android.incallui.maps.impl \

# We specify each package explicitly to glob resource files.
include ${LOCAL_PATH}/packages.mk

LOCAL_AAPT_FLAGS := $(filter-out $(EXCLUDE_EXTRA_PACKAGES),$(LOCAL_AAPT_FLAGS))
LOCAL_AAPT_FLAGS := $(addprefix --extra-packages , $(LOCAL_AAPT_FLAGS))
LOCAL_AAPT_FLAGS += \
	--auto-add-overlay \
	--extra-packages me.leolin.shortcutbadger \
        --rename-manifest-package org.codeaurora.dialer \

LOCAL_STATIC_JAVA_LIBRARIES := \
	android-common \
	android-support-dynamic-animation \
	com.android.vcard \
	qti-dialer-animal-sniffer-annotations-target \
	qti-dialer-commons-io-target \
	qti-dialer-dagger2-target \
	qti-dialer-disklrucache-target \
	qti-dialer-gifdecoder-target \
	qti-dialer-glide-target \
	qti-dialer-grpc-all-target \
	qti-dialer-grpc-context-target \
	qti-dialer-grpc-core-target \
	qti-dialer-grpc-okhttp-target \
	qti-dialer-grpc-protobuf-lite-target \
	qti-dialer-grpc-stub-target \
	qti-dialer-j2objc-annotations-target \
	qti-dialer-javax-annotation-api-target \
	qti-dialer-javax-inject-target \
	qti-dialer-libshortcutbadger-target \
	qti-dialer-mime4j-core-target \
	qti-dialer-mime4j-dom-target \
	qti-dialer-okhttp-target \
	qti-dialer-okio-target \
	qti-dialer-error-prone-target \
	qti-dialer-guava-target \
	qti-dialer-glide-annotation-target \
	qti-dialer-zxing-target \
	jsr305 \
	libbackup \
	libphonenumber \
	volley \

LOCAL_STATIC_ANDROID_LIBRARIES := \
	android-support-core-ui \
	$(ANDROID_SUPPORT_DESIGN_TARGETS) \
	android-support-transition \
	android-support-v13 \
	android-support-v4 \
	android-support-v7-appcompat \
	android-support-v7-cardview \
	android-support-v7-recyclerview \

LOCAL_JAVA_LIBRARIES := \
	qti-dialer-auto-value-target \
	org.apache.http.legacy \
	ims-ext-common \
        ims-common \
	telephony-ext \

LOCAL_ANNOTATION_PROCESSORS := \
	qti-dialer-auto-value \
	qti-dialer-javapoet-prebuilt-jar \
	qti-dialer-dagger2 \
	qti-dialer-dagger2-compiler \
	qti-dialer-dagger2-producers \
	qti-dialer-glide-annotation \
	qti-dialer-glide-compiler \
	qti-dialer-guava \
	qti-dialer-javax-annotation-api \
	qti-dialer-javax-inject \
	qti-dialer-rootcomponentprocessor

LOCAL_ANNOTATION_PROCESSOR_CLASSES := \
  com.google.auto.value.processor.AutoValueProcessor,dagger.internal.codegen.ComponentProcessor,com.bumptech.glide.annotation.compiler.GlideAnnotationProcessor,com.android.dialer.rootcomponentgenerator.RootComponentProcessor

# Proguard includes
LOCAL_PROGUARD_FLAG_FILES := proguard.flags $(call all-named-files-under,proguard.*flags,$(BASE_DIR))
#LOCAL_PROGUARD_ENABLED := custom

#LOCAL_PROGUARD_ENABLED += optimization

LOCAL_PROGUARD_ENABLED := disabled
LOCAL_MODULE_TAGS := optional
LOCAL_PRIVATE_PLATFORM_APIS := true
LOCAL_PACKAGE_NAME := QtiDialer
LOCAL_CERTIFICATE := shared
LOCAL_PRIVILEGED_MODULE := true
LOCAL_USE_AAPT2 := true
LOCAL_PRODUCT_MODULE := true

# b/37483961 - Jack Not Compiling Dagger Class Properly
LOCAL_JACK_ENABLED := javac_frontend

include $(BUILD_PACKAGE)

# Cleanup local state
BASE_DIR :=
EXCLUDE_FILES :=
RES_DIRS :=
DIALER_MANIFEST_FILES :=
EXCLUDE_MANIFESTS :=
EXCLUDE_EXTRA_PACKAGES :=

# Create references to prebuilt libraries.
include $(CLEAR_VARS)

LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := \
    qti-dialer-auto-value:../../../../../../prebuilts/tools/common/m2/repository/com/google/auto/value/auto-value/1.5.2/auto-value-1.5.2.jar \
    qti-dialer-dagger2-compiler:../../../../../../prebuilts/tools/common/m2/repository/com/google/dagger/dagger-compiler/2.7/dagger-compiler-2.7.jar \
    qti-dialer-dagger2:../../../../../../prebuilts/tools/common/m2/repository/com/google/dagger/dagger/2.7/dagger-2.7.jar \
    qti-dialer-dagger2-producers:../../../../../../prebuilts/tools/common/m2/repository/com/google/dagger/dagger-producers/2.7/dagger-producers-2.7.jar \
    qti-dialer-glide-annotation:../../../../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/annotation/SNAPSHOT/annotation-SNAPSHOT.jar \
    qti-dialer-glide-compiler:../../../../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/compiler/SNAPSHOT/compiler-SNAPSHOT.jar \
    qti-dialer-grpc-all:../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-all/1.0.3/grpc-all-1.0.3.jar \
    qti-dialer-grpc-core:../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-core/1.0.3/grpc-core-1.0.3.jar \
    qti-dialer-grpc-okhttp:../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-okhttp/1.0.3/grpc-okhttp-1.0.3.jar \
    qti-dialer-grpc-protobuf-lite:../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-protobuf-lite/1.0.3/grpc-protobuf-lite-1.0.3.jar \
    qti-dialer-grpc-stub:../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-stub/1.0.3/grpc-stub-1.0.3.jar \
    qti-dialer-guava:../../../../../../prebuilts/tools/common/m2/repository/com/google/guava/guava/23.0/guava-23.0.jar \
    qti-dialer-javax-annotation-api:../../../../../../prebuilts/tools/common/m2/repository/javax/annotation/javax.annotation-api/1.2/javax.annotation-api-1.2.jar \
    qti-dialer-javax-inject:../../../../../../prebuilts/tools/common/m2/repository/javax/inject/javax.inject/1/javax.inject-1.jar \
    qti-dialer-auto-service:../../../../../../prebuilts/tools/common/m2/repository/com/google/auto/service/auto-service/1.0-rc2/auto-service-1.0-rc2.jar \
    qti-dialer-auto-common:../../../../../../prebuilts/tools/common/m2/repository/com/google/auto/auto-common/0.9/auto-common-0.9.jar \
    qti-dialer-javapoet-prebuilt-jar:../../../../../../prebuilts/tools/common/m2/repository/com/squareup/javapoet/1.8.0/javapoet-1.8.0.jar \

include $(BUILD_HOST_PREBUILT)

# Enumerate target prebuilts to avoid linker warnings like
# Dialer (java:sdk) should not link to dialer-guava (java:platform)
include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-guava-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/com/google/guava/guava/23.0/guava-23.0.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-error-prone-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/com/google/errorprone/error_prone_annotations/2.0.18/error_prone_annotations-2.0.18.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-dagger2-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/com/google/dagger/dagger/2.7/dagger-2.7.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-disklrucache-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/disklrucache/SNAPSHOT/disklrucache-SNAPSHOT.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-gifdecoder-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/gifdecoder/SNAPSHOT/gifdecoder-SNAPSHOT.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-glide-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/glide/SNAPSHOT/glide-SNAPSHOT.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-glide-annotation-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/annotation/SNAPSHOT/annotation-SNAPSHOT.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-javax-annotation-api-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/javax/annotation/javax.annotation-api/1.2/javax.annotation-api-1.2.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-libshortcutbadger-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/me/leolin/ShortcutBadger/1.1.13/ShortcutBadger-1.1.13.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-javax-inject-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/javax/inject/javax.inject/1/javax.inject-1.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-commons-io-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/commons-io/commons-io/2.4/commons-io-2.4.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-mime4j-core-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/org/apache/james/apache-mime4j-core/0.7.2/apache-mime4j-core-0.7.2.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-mime4j-dom-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/org/apache/james/apache-mime4j-dom/0.7.2/apache-mime4j-dom-0.7.2.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-grpc-core-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-core/1.0.3/grpc-core-1.0.3.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-grpc-okhttp-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-okhttp/1.0.3/grpc-okhttp-1.0.3.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-grpc-protobuf-lite-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-protobuf-lite/1.0.3/grpc-protobuf-lite-1.0.3.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-grpc-stub-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-stub/1.0.3/grpc-stub-1.0.3.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-grpc-all-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-all/1.0.3/grpc-all-1.0.3.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-grpc-context-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/io/grpc/grpc-context/1.0.3/grpc-context-1.0.3.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-auto-value-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/com/google/auto/value/auto-value/1.5.2/auto-value-1.5.2.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-zxing-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../external/zxing/core/core.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-okhttp-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/com/squareup/okhttp/okhttp/2.7.4/okhttp-2.7.4.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-okio-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/com/squareup/okio/okio/1.9.0/okio-1.9.0.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-j2objc-annotations-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/com/google/j2objc/j2objc-annotations/1.1/j2objc-annotations-1.1.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := qti-dialer-animal-sniffer-annotations-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../../../../prebuilts/tools/common/m2/repository/org/codehaus/mojo/animal-sniffer-annotations/1.14/animal-sniffer-annotations-1.14.jar
LOCAL_UNINSTALLABLE_MODULE := true

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE := qti-dialer-rootcomponentprocessor
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_IS_HOST_MODULE := true
BASE_DIR := java/com/android

LOCAL_SRC_FILES := \
	$(call all-java-files-under, $(BASE_DIR)/dialer/rootcomponentgenerator) \
        $(BASE_DIR)/dialer/inject/DialerRootComponent.java \
        $(BASE_DIR)/dialer/inject/DialerVariant.java \
        $(BASE_DIR)/dialer/inject/HasRootComponent.java \
        $(BASE_DIR)/dialer/inject/IncludeInDialerRoot.java \
        $(BASE_DIR)/dialer/inject/InstallIn.java \
        $(BASE_DIR)/dialer/inject/RootComponentGeneratorMetadata.java

LOCAL_STATIC_JAVA_LIBRARIES := \
	qti-dialer-guava \
	qti-dialer-dagger2 \
        qti-dialer-javapoet-prebuilt-jar \
	qti-dialer-auto-service \
	qti-dialer-auto-common \
	qti-dialer-javax-annotation-api \
	qti-dialer-javax-inject

LOCAL_JAVA_LANGUAGE_VERSION := 1.8

include $(BUILD_HOST_JAVA_LIBRARY)

include $(CLEAR_VARS)
endif
