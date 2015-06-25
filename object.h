#ifndef __OBJECT_H__
#define __OBJECT_H__
//
// Base object class - PH 01/31/97
//
// This class was separated from Hardware to give more
// flexibility to the hierarchy.  Specifically, objects can
// now claim ownership of a Hardware object without having
// to be derived from the Hardware class.
//

class Object {
public:
					Object(char *cName="Object", char *oName="phil");
					~Object();

	char	* getClassName()  { return className;  }  // returns derived class name
	// returns derived object name or alias
	char	*	getObjectName() { return alias ? alias : objectName; }
	char  * getTrueName()   { return objectName; }
  char  * getAlias()      { return alias; }
	virtual void    setAlias(char *str);
  
private:
	char	*	className; 	// the *class* name of a class
	char	*	objectName;	// name of a particular *object*
	char  * alias;      // object alias name
};


#endif // __OBJECT_H__
