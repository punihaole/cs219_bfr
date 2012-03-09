#include <stdio.h>
#include <stdlib.h>

#include "net_lib.h"

int main()
{
	double d = -1.0;

	uint64_t packed = pack_ieee754_64(d);

	double unpacked = unpack_ieee754_64(packed);

	printf("%10.2f => %lu => %10.2f\n", d, packed, unpacked);


}
