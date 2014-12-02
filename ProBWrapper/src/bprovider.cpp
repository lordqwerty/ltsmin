#include "probwrapperconfig.h"
#include "javavmloader.h"

#include "bprovider.h"

BProvider::BProvider() 
{
    vm.createJavaVM();
	cache_load_b_machine();
}

/*
 * Caches all methods and fields used in the Java.
 * Yeilds speed improvments significantly
 */
void BProvider::cache_load_b_machine()
{
	clazz = vm.env->FindClass("uk/ac/surrey/pins/b/BProvider");
	constructor = vm.env->GetMethodID(clazz, "<init>", "(Ljava/lang/String;)V");
	sendVertexDestinationsFromSource = vm.env->GetMethodID(clazz, "sendVertexDestinationsFromSource", "(I)[I");
	sendInitialState = vm.env->GetMethodID(clazz, "sendInitialState", "()I");
	sendVariableCount = vm.env->GetMethodID(clazz, "sendVariableCount", "()I");	   
}

void BProvider::load_b_machine(char* file)
{
    jstring machine = vm.env->NewStringUTF(file);
    bMachine = vm.env->NewObject(clazz, constructor, machine);
}

int BProvider::get_initial_state()
{
    jint result;
	result = vm.env->CallIntMethod(bMachine, sendInitialState);
	return (int) result;
}

int BProvider::get_variable_count()
{
    jint result;
    result = vm.env->CallIntMethod(bMachine, sendVariableCount);
    return (int) result;
}

/**
    getNextState
 */
int* BProvider::get_next_state_long(int id)
{
    jintArray arr = (jintArray) vm.env->CallObjectMethod(bMachine, sendVertexDestinationsFromSource, id);
    int count = vm.env->GetArrayLength(arr);
    int * vals = vm.env->GetIntArrayElements(arr, 0);

    return vals;
}
