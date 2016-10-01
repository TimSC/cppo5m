
#include <string>
#include <vector>
#include <iostream>
#include <stdlib.h>
using namespace std;

inline signed long WrapAround(signed long val, size_t limit)
{
	div_t chk = div(val, limit);
	if (chk.rem >= 0) return chk.rem;
	return limit + chk.rem;
}

///FixedDeque is a ring buffer of fixed size. Objects can be pushed and
///popped from both ends of the buffer. The index operator provides random
///access. This hopefully will be faster than std::deque which dynamically
///resizes.
template <class T> class FixedDeque
{
protected:
	vector<T> buffer;
	signed long frontCursor; //Get items from here using pop_front
	signed long backCursor; //Insert items here using push_back

public:
	FixedDeque();
	virtual ~FixedDeque();
	void SetBufferSize(size_t si);
	size_t AvailableSpace();
	size_t Size();
	void Debug();
	size_t Fc();
	size_t Bc();

	void PushBack(const T &obj);
	void PushFront(const T &obj);
	T PopFront();
	T PopBack();
	void Clear();
	const T& operator[] (int index);
	size_t Index(const T &needle, bool &indexFoundOut, bool fromEnd);
};

template <class T> FixedDeque<T>::FixedDeque()
{
	this->frontCursor = 0;
	this->backCursor = 0;
}

template <class T> FixedDeque<T>::~FixedDeque()
{

}

template <class T> void FixedDeque<T>::SetBufferSize(size_t si)
{
	if(si+1 == this->buffer.size()) return;
	size_t currentSize = this->Size();
	if(si < currentSize)
		throw runtime_error("insufficient space to hold current content");
	if(currentSize > 0)
	{
		//Copy content to temporary vector
		vector<T> tmp;
		tmp.resize(currentSize);
		size_t count = 0;
		while (this->frontCursor != this->backCursor)
		{
			tmp[count] = this->buffer[this->frontCursor];
			T empty;
			this->buffer[this->frontCursor] = empty; //Clear old data
			this->frontCursor = WrapAround(this->frontCursor+1, this->buffer.size());
			count ++;
		}
		
		//Resize and copy back in
		this->buffer.resize(si+1);
		for(size_t i=0 ; i < currentSize; i++)
		{
			this->buffer[i] = tmp[i];
		}
		this->frontCursor = 0;
		this->backCursor = currentSize;
	}
	else
	{
		this->buffer.resize(si+1);
		this->frontCursor = 0;
		this->backCursor = 0;
	}
}

template <class T> size_t FixedDeque<T>::AvailableSpace()
{
	if (backCursor == frontCursor)
		return this->buffer.size()-1;
	if (backCursor < frontCursor)
		return frontCursor - backCursor - 1;
	return (this->buffer.size() - backCursor) + frontCursor-1;
}

template <class T> size_t FixedDeque<T>::Size()
{
	if (backCursor == frontCursor)
		return 0;
	if (backCursor < frontCursor)
		return (this->buffer.size() - frontCursor) + backCursor;
	return backCursor - frontCursor;
}

template <class T> void FixedDeque<T>::Debug()
{
	cout << "debug" << this->frontCursor << "," << this->backCursor << endl;
}

template <class T> size_t FixedDeque<T>::Fc()
{
	return this->frontCursor;
}

template <class T> size_t FixedDeque<T>::Bc()
{
	return this->backCursor;
}

template <class T> void FixedDeque<T>::PushBack(const T &obj)
{
	if(this->AvailableSpace() == 0)
		throw runtime_error("deque overflow");
	this->buffer[this->backCursor] = obj;
	this->backCursor = WrapAround(this->backCursor+1, this->buffer.size());
}

template <class T> void FixedDeque<T>::PushFront(const T &obj)
{
	if(this->AvailableSpace() == 0)
		throw runtime_error("deque overflow");
	this->frontCursor = WrapAround(this->frontCursor-1, this->buffer.size());
	this->buffer[this->frontCursor] = obj;
}

template <class T> T FixedDeque<T>::PopFront()
{
	if(this->Size() == 0)
		throw runtime_error("deque underflow");
	T obj = this->buffer[this->frontCursor];
	T empty;
	this->buffer[this->frontCursor] = empty; //Clear old data
	this->frontCursor = WrapAround(this->frontCursor+1, this->buffer.size());
	return obj;
}

template <class T> T FixedDeque<T>::PopBack()
{
	if(this->Size() == 0)
		throw runtime_error("deque underflow");
	this->backCursor = WrapAround(this->backCursor-1, this->buffer.size());
	T obj = this->buffer[this->backCursor];
	T empty;
	this->buffer[this->backCursor] = empty; //Clear old data
	return obj;
}

template <class T> void FixedDeque<T>::Clear()
{
	while (this->frontCursor != this->backCursor)
	{
		//Pop everything
		T empty;
		this->buffer[this->frontCursor] = empty; //Clear old data
		this->frontCursor = WrapAround(this->frontCursor+1, this->buffer.size());
	}
	this->frontCursor = 0;
	this->backCursor = 0;
}

template <class T> const T& FixedDeque<T>::operator[] (int index)
{
	if (index < 0 || (size_t)index >= this->Size())
		throw range_error("Invalid index");
	signed long cursor = WrapAround(this->frontCursor + index, this->buffer.size());
	return this->buffer[cursor];
}

template <class T> size_t FixedDeque<T>::Index(const T &needle, bool &indexFoundOut, bool fromEnd)
{
	indexFoundOut = false;
	size_t currentSize = this->Size();
	for(size_t i=0; i< currentSize; i++)
	{
		signed long cursor = WrapAround(this->frontCursor + i, this->buffer.size());
		const T &obj2 = this->buffer[cursor];

		if(obj2 != needle) continue;
		indexFoundOut = true;
		if (fromEnd)
			return currentSize - i;
		return i;
	}
	return 0;
}

