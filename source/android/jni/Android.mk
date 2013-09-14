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

LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_LDLIBS    := -llog -landroid
LOCAL_CFLAGS    := -Wall -Os -std=gnu99 -I -fPIC -DDEBUG -D__ANDROID__
LOCAL_MODULE    := javagree
LOCAL_SRC_FILES := vm.c struct.c serial.c compile.c util.c sys.c variable.c interpret.c node.c file.c hal_stub.c javagree.c

include $(BUILD_SHARED_LIBRARY)
