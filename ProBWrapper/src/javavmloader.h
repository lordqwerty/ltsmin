#ifndef JAVAVMLOADER_H
#define JAVAVMLOADER_H

#include <stdio.h>
#include <jni.h>

#include "probwrapperconfig.h"

#ifndef null
    #define null NULL
#endif

#define PATH_SEPARATOR ';'
#define USER_CLASSPATH "." 

class JavaVMLoader
{

public:

    JavaVM* jvm;
    JNIEnv* env;

	typedef struct JavaVMCreationResult {
    JavaVM* jvm;
    JNIEnv* env;
	} JVMCreationResult;

	JVMCreationResult* createJavaVM() {

        JavaVMInitArgs args;

        JavaVMOption opts[3];
        opts[0].optionString = "-Djava.class.path=/opt/ProB-StateExtractor-1.0.jar";
        opts[1].optionString = "-Djava.library.path=/opt/ProB-StateExtractor-1.0.jar";
        opts[2].optionString = "-Xcheck:jni";

        args.version = JNI_VERSION_1_6;
        args.options = opts;
        args.nOptions = 3;    
        args.ignoreUnrecognized = JNI_TRUE;

        JNI_GetDefaultJavaVMInitArgs(&args);

        JNI_CreateJavaVM(&jvm, (void **) &env, &args);

        JVMCreationResult* vm = new JVMCreationResult();
        vm->jvm = jvm;
        vm->env = env;

        if(vm < 0) 
        {
            printf("NO JVM LOADED! \n");
            return 0;
        } else 
        {
            return vm;
        }

	}

	JavaVMLoader();

private:
	
};

#endif