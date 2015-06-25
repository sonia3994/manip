//==========================================================
// Linked list object definition and code
//
// Created by:	Phil Harvey 01/29/97
//
// Mofications:	11/25/97 - PH added 'nextItem' to LListIterator to allow the
//														current item to be deleted.
//
#ifndef __LLIST_H__
#define __LLIST_H__

// constants
const int		LIST_END	= 0x7fff;		// a large number to signify
																	// the end of the list

//----------------------------------------------------------
// LListItem support class definition
//
class LListItem
{
public:
	LListItem(void *obj) : object(obj), nextItem(0) { }

	void			*object;		// pointer to object stored in linked list
	LListItem	*nextItem;	// pointer to next LListItem in list
};

//----------------------------------------------------------
// LList class definition
//
template<class Type> class LList
{
public:
	LList()	: firstItem(0), lastItem(0), itemCount(0)	{ }
	~LList(); // free all memory allocated to ListItem's

	LList &		operator+=(Type *obj);	// add item to end of list
	LList &		operator-=(Type *obj);	// remove item from list

	void			addFirst(Type *obj);		// add item to start of list
	int				find(Type *obj);				// return index of item in list (-1 if not found)

	int				count()			{ return itemCount;	}	// number of list items
	void			clear();
	LListItem *first()		{ return firstItem;	}	// pointer first item

private:
	LListItem	*firstItem;	// pointer to first item in list (null if no items)
	LListItem	*lastItem;	// pointer to last item in list (null if no items)
	int				itemCount;	// count of number of items in list
};

//----------------------------------------------------------
// LListIterator class definition
//
// Notes: 

//

// 1) Care must be taken not to access the obj() method if 
// the current list item has been removed from the list.
//

// 2) As of 11/25/97 it is now OK to remove the current item from

// the list.  Subsequent calls to operator++() will now work.

// However, removing the item after the current item is a real

// bad thing to do.  Also, items will not be iterated if they

// are added to the end of the list after the iterator is

// stepped to the last list element.
//
template<class Type> class LListIterator
{
public:
	LListIterator() : theList(0) { rewind();	}
	LListIterator(LList<Type> &list) : theList(&list) { rewind();	}

	LListIterator &	operator++();				// step to next item
	LListIterator &	operator=(int n);		// set current item number

	// cast to integer for comparisons
	operator int()		{ return(itemNum);	}	// return item number

	// get object stored at current location in LList
	Type	*obj()			{ return((Type *)curItem->object);	}
	Type	*obj(int n);

private:
	void				rewind();		// rewind to start of list

	LListItem		*curItem;		// pointer to current LList item
	LListItem		*nextItem;	// pointer to next LList item - 11/25/97
	LList<Type>	*theList;		// pointer to host LList
	int					itemNum;		// current item number
};

//----------------------------------------------------------
// LList methods
//

// destructor
template<class Type>
LList<Type>::~LList()
{
	// free all memory allocated to ListItems
	while (firstItem) {
		LListItem *tmp = firstItem;
		firstItem = firstItem->nextItem;
		delete tmp;
	}
}

// add an item to the end of the list
template<class Type> LList<Type>&
LList<Type>::operator+=(Type *obj)
{
	if (obj) {
		LListItem	*item = new LListItem(obj);

		if (item) {
			if (lastItem) lastItem->nextItem = item;	// add to end of list
			else firstItem = item;	// this is the first item in the list
			lastItem = item;				// new item is now at the end of the list
			++itemCount;			// update counter
		}
	}

	return(*this);
}

// remove an item from the list
template<class Type> LList<Type>&
LList<Type>::operator-=(Type *obj)
{
	LListItem	*item;
	LListItem	*prevItem = 0;

	for (item=firstItem; item; item=item->nextItem) {	// search through list
		if ((Type *)item->object == obj) {	// found object in list
			// bypass this item in the list
			if (prevItem) prevItem->nextItem = item->nextItem;
			else firstItem = item->nextItem;	// just removed first item from list
			// update pointer to the last item if necessary
			if (item == lastItem) lastItem = prevItem;
			delete item;			// free memory allocate to our ListItem
			--itemCount;			// keep track of number in list
			break;						// only remove the first occurance
		}
		prevItem = item;			// save pointer to previous item in list
	}
	return(*this);
}

// clear all items from list
template<class Type> void
LList<Type>::clear()
{
	while (firstItem) {
		LListItem	*next = firstItem->nextItem;
		delete firstItem;
		firstItem = next;
	}
	lastItem  = 0;
	itemCount = 0;
}

// add an item to the start of the list
template<class Type> void
LList<Type>::addFirst(Type *obj)
{
	if (!obj) return;

	LListItem	*item = new LListItem(obj);

	if (item) {
		item->nextItem = firstItem;
		firstItem = item;		// add to start of list
		if (!lastItem) lastItem=item;
		++itemCount;			// update counter
	}
}


// find first occurance of an item in the list
// return index of item, or -1 if not found
template<class Type> int
LList<Type>::find(Type *obj)
{
	int	index = 0;
	LListItem	*item;

	for (item=firstItem; item; item=item->nextItem) {	// search through list
		if ((Type *)item->object == obj) return(index);
		++index;
	}
	return(-1);
}


//----------------------------------------------------------
// LListIterator methods
//

// rewind to start of list
template<class Type> void
LListIterator<Type>::rewind()
{
	if (theList && (curItem=theList->first())!=0) {	// start back at first item
		itemNum = 0;				// initialize item num
		nextItem = curItem->nextItem;
	} else {
		itemNum = LIST_END;	// no items in list
		nextItem = 0;
	}
}

// get next item in list
//
// - iterates through the LList items in the order
//   that they were added to the list.
template<class Type> LListIterator<Type>&
LListIterator<Type>::operator++()
{

	if (curItem) {

		// step current item

		if ((curItem=nextItem) != 0) {

			++itemNum;						// increment item number

			nextItem = curItem->nextItem;		// update next item pointer

		} else {

			itemNum = LIST_END;		// set item number to indicate end of list

		}

	}
	return(*this);
}

// move iterator to n-th list element
template<class Type> LListIterator<Type>&
LListIterator<Type>::operator=(int n)
{
	// rewind the list first.
	// this forces re-initialization in case
	// an element <= itemNum was removed
	rewind();
	while (itemNum < n) ++(*this);	// step to specified item number
	return(*this);
}

template<class Type> Type *
LListIterator<Type>::obj(int n)
{
	*this = n;
	if (curItem) return(obj());
	else return((Type *)0);
}

#endif // __LLIST_H__
