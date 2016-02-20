
#include <iostream>
#include <fstream>
#include "o5m.h"
using namespace std;

void BoundingBox(double x1, double y1, double x2, double y2)
{
	cout << x1 << "," << y1 << "," << x2 << "," << y2 << endl;
}

int main()
{
	std::ifstream infi("o5mtest.o5m");

	class O5mDecode dec(infi);
	dec.funcStoreBounds = BoundingBox;
	dec.DecodeHeader();

	while (!infi.eof())
		dec.DecodeNext();
}

