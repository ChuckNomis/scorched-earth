#include <iostream>
using namespace std;

#include "bagel.h"
using namespace bagel;

#include "SE/ScorchedEarth.h"


int main()
{
	se::ScorchedEarth g;
	if (g.valid())
		g.run();

	return 0;
}
