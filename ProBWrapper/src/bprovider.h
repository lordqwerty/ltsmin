#ifndef BPROVIDER_H
#define BPROVIDER_H

#include <vector>

#include "probwrapperconfig.h"
#include "javavmloader.h"

class BProvider
{
	public:
		BProvider();
		void load_b_machine(char* file);
		int get_initial_state();
		int get_variable_count();
		int * get_next_state_long(int id);
		
	private:
		JavaVMLoader vm;
		jclass clazz;
		
		jmethodID constructor;
		jmethodID sendVertexDestinationsFromSource;
		jmethodID sendInitialState;
		jmethodID sendVariableCount;

		jobject bMachine;

		void cache_load_b_machine();
};

#endif
