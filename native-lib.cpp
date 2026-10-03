#include <jni.h>
#include <string>
#include <android/log.h>
#include <dlfcn.h>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <sys/mman.h>

#define LOG_TAG "FFNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static bool g_AimEnabled = false;
static bool g_AimHead = true;
static bool g_AimLock = true;
static float g_Sensitivity = 1.0f;
static float g_AimSpeed = 0.5f;
static float g_AimRange = 100.0f;
static float g_AimFOV = 90.0f;
static int g_CrosshairX = 0;
static int g_CrosshairY = 0;
static bool g_CrosshairEnabled = true;

static uintptr_t g_BaseAddr = 0;
static uintptr_t g_WorldOffset = 0;
static uintptr_t g_LocalPlayerOffset = 0;
static uintptr_t g_EntityListOffset = 0;

uintptr_t getModuleBase(const char* moduleName) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, moduleName)) {
            base = strtoul(line, nullptr, 16);
            break;
        }
    }
    fclose(fp);
    return base;
}

template<typename T>
T safeRead(uintptr_t addr) {
    if (addr == 0) return T{};
    T value;
    memcpy(&value, (void*)addr, sizeof(T));
    return value;
}

template<typename T>
bool safeWrite(uintptr_t addr, T value) {
    if (addr == 0) return false;
    uintptr_t pageStart = addr & ~0xFFF;
    mprotect((void*)pageStart, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC);
    return memcpy((void*)addr, &value, sizeof(T)) != nullptr;
}

uintptr_t getLocalPlayer() {
    if (g_BaseAddr == 0) return 0;
    uintptr_t world = safeRead<uintptr_t>(g_BaseAddr + g_WorldOffset);
    if (world == 0) return 0;
    return safeRead<uintptr_t>(world + g_LocalPlayerOffset);
}

struct Vec3 { float x, y, z; };

Vec3 getEntityHeadPosition(uintptr_t entity) {
    Vec3 head{0, 0, 0};
    if (entity == 0) return head;
    uintptr_t boneHead = safeRead<uintptr_t>(entity + 0x2A0);
    if (boneHead == 0) return head;
    head.x = safeRead<float>(boneHead + 0x50);
    head.y = safeRead<float>(boneHead + 0x54);
    head.z = safeRead<float>(boneHead + 0x58);
    return head;
}

uintptr_t findNearestEnemy(float screenCenterX, float screenCenterY) {
    uintptr_t world = safeRead<uintptr_t>(g_BaseAddr + g_WorldOffset);
    if (world == 0) return 0;
    uintptr_t entityList = safeRead<uintptr_t>(world + g_EntityListOffset);
    if (entityList == 0) return 0;
    int entityCount = safeRead<int>(entityList + 0x8);
    uintptr_t nearest = 0;
    float nearestDist = g_AimRange;
    for (int i = 0; i < entityCount; i++) {
        uintptr_t entity = safeRead<uintptr_t>(entityList + 0x10 + i * 0x8);
        if (entity == 0) continue;
        int health = safeRead<int>(entity + 0x108);
        if (health <= 0) continue;
        int team = safeRead<int>(entity + 0x10C);
        int myTeam = safeRead<int>(getLocalPlayer() + 0x10C);
        if (team == myTeam) continue;
        Vec3 head = getEntityHeadPosition(entity);
        Vec3 localPos = safeRead<Vec3>(getLocalPlayer() + 0x50);
        float dx = head.x - localPos.x;
        float dy = head.y - localPos.y;
        float dz = head.z - localPos.z;
        float dist = sqrt(dx*dx + dy*dy + dz*dz);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    return nearest;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_init(JNIEnv* env, jobject thiz) {
    g_BaseAddr = getModuleBase("libil2cpp.so");
    if (g_BaseAddr == 0) return;
    g_WorldOffset = 0xE5C3A0;
    g_LocalPlayerOffset = 0x50;
    g_EntityListOffset = 0x38;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_setAimEnabled(JNIEnv* env, jobject thiz, jboolean enabled) {
    g_AimEnabled = enabled;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_setAimHead(JNIEnv* env, jobject thiz, jboolean enabled) {
    g_AimHead = enabled;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_setAimLock(JNIEnv* env, jobject thiz, jboolean enabled) {
    g_AimLock = enabled;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_setSensitivity(JNIEnv* env, jobject thiz, jfloat value) {
    g_Sensitivity = value;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_setAimSpeed(JNIEnv* env, jobject thiz, jfloat value) {
    g_AimSpeed = value;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_setAimRange(JNIEnv* env, jobject thiz, jfloat value) {
    g_AimRange = value;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_setAimFOV(JNIEnv* env, jobject thiz, jfloat value) {
    g_AimFOV = value;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ff_native_HookNative_setCrosshair(JNIEnv* env, jobject thiz, jboolean enabled, jint x, jint y) {
    g_CrosshairEnabled = enabled;
    g_CrosshairX = x;
    g_CrosshairY = y;
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_ff_native_HookNative_getSensitivity(JNIEnv* env, jobject thiz) {
    return g_Sensitivity;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_ff_native_HookNative_isAimEnabled(JNIEnv* env, jobject thiz) {
    return g_AimEnabled;
}
