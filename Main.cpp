#include <list>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <cstring>
#include <string>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "Menu/Menu.hpp"
#include "Menu/Jni.hpp"
#include "Includes/Macros.h"
#include "dobby.h"

int scoreMul = 1, coinsMul = 1;
// Unity Math Structures
struct Vector3 { float x, y, z; };
struct Vector2 { float x, y; };

// Boolean states for features
bool Toggle_Aimbot = false;
bool Toggle_ESP_Line = false;

// Original function pointers to save game loops
struct Vector3Int { int x, y, z; }; // get_position returns Vector3Int in this dump
Vector3Int (*orig_get_position)(void* transform);
Vector2 (*orig_WorldToScreenPoint)(void* camera, Vector3 worldPoint);

// Runtime cached pointers
void* activeCamera = nullptr;
void* currentEnemyTransform = nullptr;

// Hook callback functions
Vector3Int hook_get_position(void* transform) {
    Vector3Int position = orig_get_position(transform);
    if (transform != nullptr && Toggle_Aimbot) {
        currentEnemyTransform = transform; // Safely track active coordinates
    }
    return position;
}

Vector2 hook_WorldToScreenPoint(void* camera, Vector3 worldPoint) {
    if (camera != nullptr) {
        activeCamera = camera; // Dynamic runtime camera override
    }
    return orig_WorldToScreenPoint(camera, worldPoint);
}

// Do not change or translate the first text unless you know what you are doing
// Assigning feature numbers is optional. Without it, it will automatically count for you, starting from 0
// Assigned feature numbers can be like any numbers 1,3,200,10... instead in order 0,1,2,3,4,5...
// ButtonLink, Category, RichTextView and RichWebView is not counted. They can't have feature number assigned
// Toggle, ButtonOnOff and Checkbox can be switched on by default, if you add True_. Example: CheckBox_True_The Check Box
// To learn HTML, go to this page: https://www.w3schools.com/

jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;

    const char *features[] = {
    OBFUSCATE("Category_Combat Features"), // Category Name
    OBFUSCATE("Toggle_Aimbot"),           // Feature 0
    
    OBFUSCATE("Category_Visual Features"), // Category Name
    OBFUSCATE("Toggle_ESP Line"),         // Feature 1
    
    OBFUSCATE("Category_Movement Features"), // Category Name
    OBFUSCATE("Toggle_Speed Hack")         // Feature 2
};



    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

bool btnPressed = false;

//Target main lib here
#define targetLibName OBFUSCATE("libil2cpp.so")

void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, jint value, jlong Lvalue, jboolean boolean, jstring text) {

        switch (featNum) {
        case 0: // Toggle_Aimbot (Feature 0)
            Toggle_Aimbot = !Toggle_Aimbot;
            static bool aimbotHooked = false;
            if (!aimbotHooked && Toggle_Aimbot) {
                // get_position offset 0xA6B74A0 ko hook kar rahe hain
                DobbyHook((void*)(getAbsoluteAddress(targetLibName, 0xA6B74A0)), (void*)hook_get_position, (void**)&orig_get_position);
                aimbotHooked = true;
            }
            break;

        case 1: // Toggle_ESP_Line (Feature 1)
            Toggle_ESP_Line = !Toggle_ESP_Line;
            static bool espHooked = false;
            if (!espHooked && Toggle_ESP_Line) {
                // WorldToScreenPoint offset 0x9A15D44 ko hook kar rahe hain
                DobbyHook((void*)(getAbsoluteAddress(targetLibName, 0x9A15D44)), (void*)hook_WorldToScreenPoint, (void**)&orig_WorldToScreenPoint);
                espHooked = true;
            }
            break;

        case 2: // Toggle_Speed_Hack (Feature 2)
            // Future speed hack configuration yahan aayega
            break;
            
        default:
            break;
    }


        

//CharacterPlayer
void (*StartInvcibility)(void *instance, float duration);

void (*old_Update)(void *instance);
void Update(void *instance) {
    if (instance != nullptr) {
        if (btnPressed) {
            StartInvcibility(instance, 30);
            btnPressed = false;
        }
    }
    return old_Update(instance);
}

/*
 void (*old_AddScore)(void *instance, int score);
 void AddScore(void *instance, int score) {
    //default any actions
    return old_AddScore(instance, score * scoreMul);
 }
*/
// === This function was completely replaced with `install_hook_name` from dobby.h ===
// (base name, return type, ... args)
install_hook_name(AddScore, void *, void *instance, int score) {
    // default any actions

    // use orig_ for call original function
    return orig_AddScore(instance, score + scoreMul);
}

void (*old_AddCoins)(void *instance, int count);
void AddCoins(void *instance, int count) {
    return old_AddCoins(instance, count * coinsMul);
}


// we will run our hacks in a new thread so our while loop doesn't block process main thread
void hack_thread() {
    // This loop should be always enabled in unity game
    // because libil2cpp.so is not loaded into memory immediately.
    while (!isLibraryLoaded(targetLibName)) {
        sleep(1); // Wait for target lib be loaded.
    }

    // In Android Studio, to switch between arm64-v8a and armeabi-v7a syntax highlighting,
    // You can modify the "Active ABI" in "Build Variants" to switch to another architecture for parsing.
#if defined(__aarch64__)
    // IL2Cpp: Use RVA offset
    StartInvcibility = (void (*)(void *, float))getAbsoluteAddress(targetLibName, 0x1070A2C);
    HOOK(targetLibName, "0x107A2FC", AddCoins, old_AddCoins);
    HOOK(targetLibName, "0x1078C44", Update, old_Update);
    INST(targetLibName, "0x23558C", "AnyNameForDetect", true);
#endif

    LOGI(OBFUSCATE("Done"));
}


// Functions with `__attribute__((constructor))` are executed immediately when System.loadLibrary("lib_name") is called.
// If there are multiple such functions at the same time, `constructor(priority)` (the priority is an integer)
// will determine the execution priority, otherwise the execution order is undefined behavior.
__attribute__((constructor))
void lib_main() {
    // Create a new thread so it does not block the main thread, means the game would not freeze
    // In modern C++, you should use std::thread(yourFunction).detach() instead of pthread_create
    std::thread(hack_thread).detach();
}