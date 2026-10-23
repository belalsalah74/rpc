/*
 * JNI bootstrap loader for Pokémon GO 0.433.1 ARM64.
 * Preserves the game's libmain entrypoint and starts Frida Gadget from app-private
 * storage after Application startup, allowing PGRazor's catch/throw hooks to attach.
 */
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern char *dlerror(void);
extern int __android_log_print(int, const char *, const char *, ...);
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define JNI_VERSION_1_6 0x00010006
#define TAG "PGORPCLOADER"
#define LOGE(...) __android_log_print(6, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(4, TAG, __VA_ARGS__)
#define PATH_CAP 4096
#define COPY_BUFFER 32768

typedef jint (*jni_onload_t)(JavaVM *, void *);

static int check_exception(JNIEnv *env, const char *where) {
    if (!(*env)->ExceptionCheck(env)) return 0;
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    LOGE("Java exception while %s", where);
    return 1;
}
static int join_path(char *out, size_t cap, const char *dir, const char *name) {
    int n = snprintf(out, cap, "%s/%s", dir, name);
    return n >= 0 && n < (int)cap;
}
static int copy_asset(JNIEnv *env, jobject manager, const char *asset,
                      const char *destination, mode_t mode) {
    jclass mc = (*env)->FindClass(env, "android/content/res/AssetManager");
    if (mc == NULL || check_exception(env, "finding AssetManager")) return 0;
    jmethodID open = (*env)->GetMethodID(env, mc, "open", "(Ljava/lang/String;)Ljava/io/InputStream;");
    if (open == NULL || check_exception(env, "resolving AssetManager.open")) return 0;
    jstring name = (*env)->NewStringUTF(env, asset);
    if (name == NULL) { check_exception(env, "creating asset name"); return 0; }
    jobject in = (*env)->CallObjectMethod(env, manager, open, name);
    (*env)->DeleteLocalRef(env, name);
    if (in == NULL || check_exception(env, "opening packaged asset")) {
        LOGE("Unable to open APK asset %s", asset); return 0;
    }
    jclass sc = (*env)->FindClass(env, "java/io/InputStream");
    if (sc == NULL || check_exception(env, "finding InputStream")) { (*env)->DeleteLocalRef(env,in); return 0; }
    jmethodID read = (*env)->GetMethodID(env, sc, "read", "([B)I");
    jmethodID close = (*env)->GetMethodID(env, sc, "close", "()V");
    if (read == NULL || close == NULL || check_exception(env, "resolving stream methods")) {
        (*env)->DeleteLocalRef(env,in); return 0;
    }
    char partial[PATH_CAP];
    if (snprintf(partial,sizeof(partial),"%s.partial",destination) >= (int)sizeof(partial)) {
        (*env)->DeleteLocalRef(env,in); return 0;
    }
    remove(partial);
    FILE *out=fopen(partial,"wb");
    if (!out) {
        LOGE("Cannot create private file %s",partial);
        (*env)->CallVoidMethod(env,in,close); check_exception(env,"closing failed stream");
        (*env)->DeleteLocalRef(env,in); return 0;
    }
    jbyteArray buffer=(*env)->NewByteArray(env,COPY_BUFFER);
    int ok=buffer!=NULL;
    if (!ok) check_exception(env,"allocating read buffer");
    while(ok) {
        jint count=(*env)->CallIntMethod(env,in,read,buffer);
        if(check_exception(env,"reading packaged asset")){ok=0;break;}
        if(count<0)break;
        if(count==0)continue;
        jbyte bytes[COPY_BUFFER];
        (*env)->GetByteArrayRegion(env,buffer,0,count,bytes);
        if(check_exception(env,"reading asset bytes")){ok=0;break;}
        if(fwrite(bytes,1,(size_t)count,out)!=(size_t)count){ok=0;break;}
    }
    if(buffer!=NULL)(*env)->DeleteLocalRef(env,buffer);
    (*env)->CallVoidMethod(env,in,close);
    if(check_exception(env,"closing packaged asset"))ok=0;
    (*env)->DeleteLocalRef(env,in);
    if(fclose(out)!=0)ok=0;
    if(ok&&rename(partial,destination)==0&&chmod(destination,mode)==0){
        LOGI("Extracted %s",asset);return 1;
    }
    remove(partial); LOGE("Failed extracting APK asset %s",asset); return 0;
}
static int write_config(const char *dir,char *config,size_t cap) {
    if(!join_path(config,cap,dir,"libfrida-gadget.config"))return 0;
    FILE *f=fopen(config,"wb"); if(!f)return 0;
    int n=fprintf(f,"{\"interaction\":{\"type\":\"script\",\"path\":\"%s/agent.js\",\"parameters\":{},\"on_change\":\"ignore\"},\"runtime\":\"qjs\"}\n",dir);
    int closed=fclose(f); if(n<=0||closed!=0){remove(config);return 0;}
    chmod(config,0600);return 1;
}
static void *feature_worker(void *opaque) {
    JavaVM *vm=(JavaVM*)opaque; JNIEnv *env=NULL;
    if((*vm)->AttachCurrentThread(vm,&env,NULL)!=JNI_OK||!env){LOGE("Could not attach loader thread to ART");return NULL;}
    usleep(1500000);
    jclass at=(*env)->FindClass(env,"android/app/ActivityThread");
    if(!at||check_exception(env,"finding ActivityThread"))goto done;
    jmethodID current=(*env)->GetStaticMethodID(env,at,"currentApplication","()Landroid/app/Application;");
    if(!current||check_exception(env,"resolving currentApplication"))goto done;
    jobject app=NULL;
    for(int i=0;i<60&&!app;i++){
        app=(*env)->CallStaticObjectMethod(env,at,current);
        if(check_exception(env,"getting current Application"))goto done;
        if(!app){
            (*env)->DeleteLocalRef(env,at);usleep(500000);
            at=(*env)->FindClass(env,"android/app/ActivityThread");
            if(!at||check_exception(env,"refinding ActivityThread"))goto done;
            current=(*env)->GetStaticMethodID(env,at,"currentApplication","()Landroid/app/Application;");
            if(!current||check_exception(env,"resolving currentApplication"))goto done;
        }
    }
    if(!app){LOGE("Application unavailable; Quick Catch/Throw was not loaded");goto done;}
    jclass cc=(*env)->FindClass(env,"android/content/Context");
    if(!cc||check_exception(env,"finding Context"))goto done;
    jmethodID getFiles=(*env)->GetMethodID(env,cc,"getFilesDir","()Ljava/io/File;");
    jmethodID getAssets=(*env)->GetMethodID(env,cc,"getAssets","()Landroid/content/res/AssetManager;");
    if(!getFiles||!getAssets||check_exception(env,"resolving Context methods"))goto done;
    jobject files=(*env)->CallObjectMethod(env,app,getFiles);
    if(!files||check_exception(env,"getting app files directory"))goto done;
    jclass fc=(*env)->FindClass(env,"java/io/File");
    if(!fc||check_exception(env,"finding File"))goto done;
    jmethodID absolute=(*env)->GetMethodID(env,fc,"getAbsolutePath","()Ljava/lang/String;");
    if(!absolute||check_exception(env,"resolving getAbsolutePath"))goto done;
    jstring path=(jstring)(*env)->CallObjectMethod(env,files,absolute);
    if(!path||check_exception(env,"reading app files path"))goto done;
    const char *utf=(*env)->GetStringUTFChars(env,path,NULL);
    if(!utf||check_exception(env,"reading path string"))goto done;
    char dir[PATH_CAP],gadget[PATH_CAP],config[PATH_CAP];
    int n=snprintf(dir,sizeof(dir),"%s/pgorpc",utf);
    (*env)->ReleaseStringUTFChars(env,path,utf);
    if(n<0||n>=(int)sizeof(dir)){LOGE("Private directory path too long");goto done;}
    if(mkdir(dir,0700)!=0){struct stat st;if(stat(dir,&st)!=0||!S_ISDIR(st.st_mode)){LOGE("Could not create private feature directory");goto done;}}
    chmod(dir,0700);
    jobject am=(*env)->CallObjectMethod(env,app,getAssets);
    if(!am||check_exception(env,"getting AssetManager"))goto done;
    char ball[PATH_CAP],closejs[PATH_CAP],bridge[PATH_CAP],agent[PATH_CAP];
    if(!join_path(gadget,sizeof(gadget),dir,"libfrida-gadget.so")||
       !join_path(ball,sizeof(ball),dir,"quick-ball.js")||
       !join_path(closejs,sizeof(closejs),dir,"quick-close.js")||
       !join_path(bridge,sizeof(bridge),dir,"catch-presentation-bridge.js")||
       !join_path(agent,sizeof(agent),dir,"agent.js")){LOGE("Feature asset path too long");goto done;}
    if(!copy_asset(env,am,"pgorpc/libfrida-gadget.so",gadget,0700)||
       !copy_asset(env,am,"pgorpc/quick-ball.js",ball,0600)||
       !copy_asset(env,am,"pgorpc/quick-close.js",closejs,0600)||
       !copy_asset(env,am,"pgorpc/catch-presentation-bridge.js",bridge,0600)||
       !copy_asset(env,am,"pgorpc/agent.js",agent,0600)){LOGE("Feature asset extraction failed");goto done;}
    if(!write_config(dir,config,sizeof(config))){LOGE("Could not write Frida Gadget config");goto done;}
    {
        void *handle=dlopen(gadget,RTLD_NOW|RTLD_LOCAL);
        if(!handle){LOGE("dlopen Frida Gadget failed: %s",dlerror());goto done;}
        jni_onload_t onload=(jni_onload_t)dlsym(handle,"JNI_OnLoad");
        if(!onload){LOGE("Frida Gadget JNI_OnLoad entrypoint missing");goto done;}
        jint version=onload(vm,NULL);
        if(version<0){LOGE("Frida Gadget JNI_OnLoad failed: %d",version);goto done;}
        LOGI("Frida Gadget initialized (JNI version 0x%x)",(unsigned int)version);
    }
done:
    (*vm)->DetachCurrentThread(vm);return NULL;
}
__attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM *vm,void *reserved) {
    LOGI("Quick Catch / Enhanced Throw bootstrap entered");
    void *original=dlopen("libmain_orig.so",RTLD_NOW|RTLD_LOCAL);
    if(!original){LOGE("failed to load libmain_orig.so: %s",dlerror());return -1;}
    jni_onload_t entry=(jni_onload_t)dlsym(original,"JNI_OnLoad");
    jint version=JNI_VERSION_1_6;
    if(entry){version=entry(vm,reserved);if(version<0){LOGE("original JNI_OnLoad failed: %d",version);return version;}}
    else LOGI("original library has no JNI_OnLoad; continuing");
    void *probe=dlopen("libpgo_rpc_probe.so",RTLD_NOW|RTLD_LOCAL);
    if(!probe)LOGE("RPC diagnostic payload unavailable: %s",dlerror());else LOGI("RPC diagnostic payload loaded");
    pthread_t worker;
    if(pthread_create(&worker,NULL,feature_worker,vm)==0){pthread_detach(worker);LOGI("Quick Catch/Throw runtime worker started");}
    else LOGE("Could not start Quick Catch/Throw runtime worker");
    return version;
}
