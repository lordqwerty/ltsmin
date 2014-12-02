#include <stdio.h>

#include "probwrapperconfig.h"
#include "bprovider.h"

int main (int argc, char *argv[])
{	
		printf("Loading B \n");
		BProvider b;
		b.load_b_machine("/home/jorden/Code/UoS/COM3001_ProfessionalProject/ProBWrapper/machines/opMachine/opMachine.mch");
		int initState = b.get_initial_state();
		printf("Initial State is: %d \n", initState);
		int varCount = b.get_variable_count();
		printf("Variable count of B Machine: %d \n", varCount);
		b.get_next_state_long(initState);
		return 1;
}
