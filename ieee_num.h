//
// IEEE floating point utility routines
//
// 					- written by PH 01/28/97
//


// return non-zero if number is NAN or infinity
// (If infinity, the low 52 bits will be zero,
//  and the highest bit is the sign)
//
inline int isNAN(double x)
{
	return((*((short *)&x+3) & 0x7ff0) == 0x7ff0);
}

