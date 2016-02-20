
#include <fstream>
#include "o5m.h"

int main()
{
	std::ifstream infi("o5mtest.o5m");

	class O5mDecode dec(infi);
	dec.DecodeHeader();

	while (!infi.eof())
		dec.DecodeNext();
}

