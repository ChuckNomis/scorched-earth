#include <iostream>
using namespace std;

#include "bagel.h"
using namespace bagel;

#include "Pong.h"
using namespace mta;

void run_tests();

class Movement{};
class IsPlayer{};

template <> struct bagel::Storage<Movement> final : NoInstance {
	using type = PackedStorage<Movement>;
};

int main()
{
	Pong p;
	if (p.valid())
		p.run();

	return 0;
}
