#include "fixeddeque.h"
#include <assert.h>
#include <iostream>
#include <stdexcept>
using namespace std;

int main()
{
	FixedDeque<int> d;
	int cap = 5;
	d.SetBufferSize(cap);
	int s = 0;
        assert(d.Size()==s);
        assert(d.Size()+d.AvailableSpace()==cap);

	//Fill the deque
	for(int i=0;i<cap;i++)
	{
		d.PushBack(i);
		s++;
		//cout << d.Size() << "," << s << endl;
		assert(d.Size()==s);
		assert(d.Size()+d.AvailableSpace()==cap);
	}

	//Check data
	for(int i=0;i<cap;i++)
		assert(d[i] == i);

	//Check we can't push again
	try
	{
		d.PushBack(-1);
		assert(0);//Not allowed
	}
	catch (runtime_error &err){}

	for(int rep =0; rep < cap ; rep ++)
	{
	cout << "rep " << rep << endl;

	//Remove all but one
	for(int i=0;i<cap-1;i++)
	{
		int val = d.PopFront();
		if (rep == 0)
			assert(val == i);
		s--;
		assert(d.Size()==s);
                assert(d.Size()+d.AvailableSpace()==cap);
	}

	//Fill the deque
        for(int i=0;i<cap-1;i++)
        {
                d.PushBack(i);
                s++;
                //cout << d.Size() << "," << s << endl;
		//d.Debug();
                assert(d.Size()==s);
                assert(d.Size()+d.AvailableSpace()==cap);
        }

	//for(int i=0;i<d.Size();i++)
	//	cout << "val"<<i<<"=" << d[i] << endl;

        //Check data
        for(int i=1;i<cap;i++)
                assert(d[i] == i-1);

        //Check we can't push again
        try
        {
                d.PushBack(-1);
                assert(0);//Not allowed
        }
        catch (runtime_error &err){}

	}

	cout << "All done!" << endl;
	return 0;
}
