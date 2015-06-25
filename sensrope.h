/*****************************************************************/
//
// The purpose of a sense rope class is to find the length of a
// rope in centimeters. The senserope class is a third-level
// hardware derived class. It is analagous to an encoder in many
// ways although much simpler. There are no database updates.
//
// The objects are members of the AV class. It is used to find
// the position of the neck ring.
//
// Chris Jillings, September, 1996
//
/*****************************************************************/

#ifndef __SENSROPE_H
#define __SENSROPE_H

#include "controll.h"
#include "hardware.h"
#include "hwio.h"
#include "threevec.h"

#define SENSEROPE_COMMAND_NUMBER 3 // number of commands in list

class SenseRope: public Controller
{
private:
	unsigned 		uncalibrated; // raw ADC reading
	float 			length;				// calibrated read value
	float 			deltaL;       // length from zero position
	float 			zeroOffset;		// for calibrating to centimeters
	float 			deltaADC;			// ADC-zeroValue
	float				ADCZero;			// for getting deltaL
	float 			slope;				//     ditto
	float 			twelveInch;   // for calibration
	float       twentyfourInch; //  ditto
	ThreeVector snout;				// the position in global co-ords of the rope at the deck
//	ThreeVector neckAttach;		// the position in AV co-ords of the neck attachment point
	ThreeVector delta;      	// the vector from score to the srope.
	char*       mName;        // the hardware name
	float 			originalLength; // from survey data

public:
	SenseRope(char *name);  // Database constructor a la TJR
																				// both names the same.
	void poll();	// puts length in centimeters into length above.
	ThreeVector getSnout();    // returns snout (see above)
//	ThreeVector getNeckAttach(); // returns neckAttach (see above)
	ThreeVector getDelta();  // returns delta
	float 			getDeltaADC();
	float       getDeltaL();
	float 			getLength(); // returns the length
	void				setLength(float newLength);

	float 			getSlope();
	float 			getADCZero();
	unsigned    getUncalibrated();
	void 				null(void) {return;}
	float 			getZeroOffset();
	void        setZeroOffset(ThreeVector a);
	void				setSnout(ThreeVector aSnout);

	void 				rawADC(char* aString);
	float       getRawADC(float *outSigma=(float *)0);

	void        displaySlope(char* aString);
	void        displayOffset(char* aString);

private:

		static class RawADC: public Command
		{public:
			RawADC(void):Command("rawadc") { ; }
			char* subCommand(Hardware *hw, char* inString)
			{
				((SenseRope*) hw)->rawADC(inString);
				return "HW_ERR_OK";
			}
		} cmd_rawADC;

		static class DisplaySlope: public Command
		{public:
			DisplaySlope(void):Command("DisplaySlope") { ; }
			char* subCommand(Hardware *hw, char* inString)
			{
				((SenseRope*) hw)->displaySlope(inString);
				return "HW_ERR_OK";
			}
		} cmd_slope;

		static class DisplayOffset: public Command
		{public:
			DisplayOffset(void):Command("DisplayOffset") { ; }
			char* subCommand(Hardware *hw, char* inString)
			{
				((SenseRope*) hw)->displayOffset(inString);
				return "HW_ERR_OK";
			}
		} cmd_offset;

//	static class Null: public Command	// dummy command class
//	{public:
//		Null(void):Command("null"){ ; }
//		char* subCommand(Hardware* hw, char* inString)
//		{
//			((SenseRope*) hw)->null();
//			return "HW_ERR_OK";
//		}
//	} cmd_null;

protected:
	virtual Command *getCommand(int num)	{ return commandList[num]; }
	virtual int 		getNumCommands() 			{ return SENSEROPE_COMMAND_NUMBER; }

private:
	static Command	*commandList[SENSEROPE_COMMAND_NUMBER];	// list of commands

}; // end of declaration for SenseRope class


#endif





