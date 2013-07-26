#include <stdio.h>
#include "JniSample.h"

JNIEXPORT jstring JNICALL Java_JniSample_sayHello (JNIEnv *env, jobject obj, jstring inJNIStr) {
    // Step 1: Convert the JNI String (jstring) into C-String (char*)
    const char *inCStr = (*env)->GetStringUTFChars(env, inJNIStr, NULL);
    if (NULL == inCStr)
        return NULL;
    
    // Step 2: Perform its intended operations
    printf("In C, the received string is: %s\n", inCStr);
    (*env)->ReleaseStringUTFChars(env, inJNIStr, inCStr);  // release resources
    
    // Prompt user for a C-string
    char outCStr[128];
    printf("Enter a String: ");
    scanf("%s", outCStr);    // not more than 127 characters
    
    // Step 3: Convert the C-string (char*) into JNI String (jstring) and return
    return (*env)->NewStringUTF(env, outCStr);
}