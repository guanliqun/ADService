
#include <stdlib.h>
#include <random>
#include <array>
#include <algorithm>
#include <iostream>
#include "../utility/utility.h"

using namespace std;

static const char cypherMap[4][16] = {{'e','S','U','s','K','n','M','O','[','C','l','-','Q','c','E','b'},
									  {'u','v','z','X','f','R','x','Y','V','+','_','@','M','L','B','m'},
									  {']','w','T','#','@','a','k','I','d','j','G','J','Z','q','N','o'},
									  {'[','A','p','t','=','F','P','r','|','_','H','i','g','y','h','D'}};



int main(int argc,char** argv){
	using namespace adservice::utility::time;
	int timeDiff = getCurrentTimeSinceMtty();
	printf("%d\n",timeDiff);
	return 0;
}