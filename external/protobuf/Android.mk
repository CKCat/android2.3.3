# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#

LOCAL_PATH := $(call my-dir)

CC_LITE_SRC_FILES := \
    src/google/protobuf/stubs/common.cc                              \
    src/google/protobuf/stubs/once.cc                                \
    src/google/protobuf/stubs/hash.cc                                \
    src/google/protobuf/stubs/hash.h                                 \
    src/google/protobuf/stubs/map-util.h                             \
    src/google/protobuf/stubs/stl_util-inl.h                         \
    src/google/protobuf/extension_set.cc                             \
    src/google/protobuf/generated_message_util.cc                    \
    src/google/protobuf/message_lite.cc                              \
    src/google/protobuf/repeated_field.cc                            \
    src/google/protobuf/wire_format_lite.cc                          \
    src/google/protobuf/io/coded_stream.cc                           \
    src/google/protobuf/io/coded_stream_inl.h                        \
    src/google/protobuf/io/zero_copy_stream.cc                       \
    src/google/protobuf/io/zero_copy_stream_impl_lite.cc

JAVA_LITE_SRC_FILES := \
    java/src/main/java/com/google/protobuf/UninitializedMessageException.java \
    java/src/main/java/com/google/protobuf/MessageLite.java \
    java/src/main/java/com/google/protobuf/InvalidProtocolBufferException.java \
    java/src/main/java/com/google/protobuf/CodedOutputStream.java \
    java/src/main/java/com/google/protobuf/ByteString.java \
    java/src/main/java/com/google/protobuf/CodedInputStream.java \
    java/src/main/java/com/google/protobuf/ExtensionRegistryLite.java \
    java/src/main/java/com/google/protobuf/AbstractMessageLite.java \
    java/src/main/java/com/google/protobuf/FieldSet.java \
    java/src/main/java/com/google/protobuf/Internal.java \
    java/src/main/java/com/google/protobuf/WireFormat.java \
    java/src/main/java/com/google/protobuf/GeneratedMessageLite.java


# Java micro library (for device-side users)
# =======================================================
include $(CLEAR_VARS)

LOCAL_MODULE := libprotobuf-java-2.3.0-micro
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, java/src/main/java/com/google/protobuf/micro)

include $(BUILD_STATIC_JAVA_LIBRARY)

# Java micro library (for host-side users)
# =======================================================
include $(CLEAR_VARS)

LOCAL_MODULE := host-libprotobuf-java-2.3.0-micro
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, java/src/main/java/com/google/protobuf/micro)

include $(BUILD_HOST_JAVA_LIBRARY)

# Java lite library (for device-side users)
# =======================================================
include $(CLEAR_VARS)

LOCAL_MODULE := libprotobuf-java-2.3.0-lite
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(JAVA_LITE_SRC_FILES)

include $(BUILD_STATIC_JAVA_LIBRARY)

# Java lite library (for host-side users)
# =======================================================
include $(CLEAR_VARS)

LOCAL_MODULE := host-libprotobuf-java-2.3.0-lite
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(JAVA_LITE_SRC_FILES)

include $(BUILD_HOST_JAVA_LIBRARY)

# C++ lite library
# =======================================================
include $(CLEAR_VARS)

LOCAL_MODULE := libprotobuf-cpp-2.3.0-lite
LOCAL_MODULE_TAGS := optional

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := $(CC_LITE_SRC_FILES)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/android \
    bionic \
    $(LOCAL_PATH)/src

LOCAL_SHARED_LIBRARIES := \
    libcutils libutils

# stlport conflicts with the host stl library
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_C_INCLUDES += external/stlport/stlport
LOCAL_SHARED_LIBRARIES += libstlport
endif

# Define the header files to be copied
#LOCAL_COPY_HEADERS := \
#    src/google/protobuf/stubs/once.h \
#    src/google/protobuf/stubs/common.h \
#    src/google/protobuf/io/coded_stream.h \
#    src/google/protobuf/generated_message_util.h \
#    src/google/protobuf/repeated_field.h \
#    src/google/protobuf/extension_set.h \
#    src/google/protobuf/wire_format_lite_inl.h
#
#LOCAL_COPY_HEADERS_TO := $(LOCAL_MODULE)

LOCAL_CFLAGS := -DGOOGLE_PROTOBUF_NO_RTTI

include $(BUILD_STATIC_LIBRARY)

# C++ full library
# =======================================================
include $(CLEAR_VARS)

LOCAL_MODULE := libprotobuf-cpp-2.3.0-full
LOCAL_MODULE_TAGS := optional

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
    $(CC_LITE_SRC_FILES)                                             \
    src/google/protobuf/stubs/strutil.cc                             \
    src/google/protobuf/stubs/strutil.h                              \
    src/google/protobuf/stubs/substitute.cc                          \
    src/google/protobuf/stubs/substitute.h                           \
    src/google/protobuf/stubs/structurally_valid.cc                  \
    src/google/protobuf/descriptor.cc                                \
    src/google/protobuf/descriptor.pb.cc                             \
    src/google/protobuf/descriptor_database.cc                       \
    src/google/protobuf/dynamic_message.cc                           \
    src/google/protobuf/extension_set_heavy.cc                       \
    src/google/protobuf/generated_message_reflection.cc              \
    src/google/protobuf/message.cc                                   \
    src/google/protobuf/reflection_ops.cc                            \
    src/google/protobuf/service.cc                                   \
    src/google/protobuf/text_format.cc                               \
    src/google/protobuf/unknown_field_set.cc                         \
    src/google/protobuf/wire_format.cc                               \
    src/google/protobuf/io/gzip_stream.cc                            \
    src/google/protobuf/io/printer.cc                                \
    src/google/protobuf/io/tokenizer.cc                              \
    src/google/protobuf/io/zero_copy_stream_impl.cc                  \
    src/google/protobuf/compiler/importer.cc                         \
    src/google/protobuf/compiler/parser.cc

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/android \
    bionic \
    external/zlib \
    $(LOCAL_PATH)/src

LOCAL_SHARED_LIBRARIES := \
    libz libcutils libutils

# stlport conflicts with the host stl library
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_C_INCLUDES += external/stlport/stlport
LOCAL_SHARED_LIBRARIES += libstlport
endif

# Define the header files to be copied
#LOCAL_COPY_HEADERS := \
#    src/google/protobuf/stubs/once.h \
#    src/google/protobuf/stubs/common.h \
#    src/google/protobuf/io/coded_stream.h \
#    src/google/protobuf/generated_message_util.h \
#    src/google/protobuf/repeated_field.h \
#    src/google/protobuf/extension_set.h \
#    src/google/protobuf/wire_format_lite_inl.h
#
#LOCAL_COPY_HEADERS_TO := $(LOCAL_MODULE)

LOCAL_CFLAGS := -DGOOGLE_PROTOBUF_NO_RTTI

include $(BUILD_STATIC_LIBRARY)
