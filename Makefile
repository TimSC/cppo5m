
all:
	g++ o5m.cpp varint.cpp selftest.cpp -o selftest
	g++ o5m.cpp varint.cpp test.cpp -o test

