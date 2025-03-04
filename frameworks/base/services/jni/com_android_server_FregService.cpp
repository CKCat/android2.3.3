
#define LOG_TAG "CKCatFregServiceJNI"
#include "jni.h"
#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"

#include <utils/misc.h>
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/ckcatfreg.h>
#include <stdio.h>


namespace android{

    // 设置虚拟硬件设备 ckcatfreg 的寄存器的值
    static void freg_setVal(JNIEnv* env, jobject clazz, jint ptr, jint value){

        // 将参数 ptr 转换成 freg_device_t 结构体
        freg_device_t* device = (freg_device_t*)ptr;
        if (!device){
            LOGE("Device ckcatfreg is not open.");
        }

        int val = value;
        LOGI("Set value %d to device ckcatfreg.", val);
        device->set_val(device, val);
    }

    // 读取虚拟硬件设备 ckcatfreg 的寄存器的值
    static jint freg_getVal(JNIEnv* env, jobject clazz, jint ptr){

        // 将参数 ptr 转换成 freg_device_t 结构体
        freg_device_t* device = (freg_device_t*)ptr;
        if (!device){
            LOGE("Device ckcatfreg is not open.");
        }
        int val = 0;
        device->get_val(device, &val);
        LOGI("Get value %d from device ckcatfreg.", val);
        return val;
    }

    // 打开虚拟硬件设备 freg
    static inline int freg_device_open(const hw_module_t* module, 
                                        struct freg_device_t** device){
        return module->methods->open(module, FREG_HARDWARE_DEVICE_ID, 
                                    (struct hw_device_t**)device);
    }

    /* 初始化虚拟硬件设备 ckcatfreg */
    static jint freg_init(JNIEnv* env, jobject clazz){
        freg_module_t* module;
        freg_device_t* device;

        LOGI("Initializing HAL stub ckcatfreg ......");

        /* 加载硬件抽象层模块 ckcatfreg */
        if(hw_get_module(FREG_HARDWARE_MODULE_ID, 
                        (const struct hw_module_t**)&module) ==  0){
            LOGI("Device ckcatfreg found.");

            // 打开虚拟硬件设备 ckcatfreg
            if (freg_device_open(&(module->common), &device) == 0){
                LOGI("Device ckcatfreg is open.");

                // 将 freg_device_t 接口转换为整型返回
                return (jint)(device);
            }
            LOGE("Failed to open device ckcatfreg.");
            return 0;
        }
        LOGE("Failed to get HAL stub ckcatfreg.");
        return 0;
    }

    // JNI 方法表
    static const JNINativeMethod method_table[] = {
        {"init_native", "()I", (void*)freg_init},
        {"setVal_native", "(II)V", (void*)freg_setVal},
        {"getVal_native", "(I)I", (void*)freg_getVal},
    };

    // 注册 JNI 方法
    int register_android_service_FregService(JNIEnv* env){
        return jniRegisterNativeMethods(env, "com/android/server/FregService", 
                                        method_table, NELEM(method_table));
    }
}