VERSION3.00  // This MUST be the first line of the file

PCLBASE: 0x320  old   // This must come before any counterboards
TIO10_ADDRESS1: 0x1a8 // This must come before motors plugged into 1-8
TIO10_ADDRESS2: 0x1a0 // this must come before motors plugged into 9-16

// The tio10 wiring entry is case sensitive. Only "C" (clock), "U"
// (unused), and an integer from 1 to 10 are acceptable entries.
// The user will never change the connector number in Alvin's box
// for a particluar tio10 channel because that is determined
// by soldered wires and connectors. What the user may do is
// mark these as unused or use them for a clock.

// Revisions:
//
// 12/5/97 Added N2 laser, dye laser and ND filter wheels
//         connection and motors.
//                                            Richard Ford

TIO10_WIRING: CHANNEL    CARD_1 CARD_2
								1         1       9
								2         2       10
								3         3       11
								4         C       U
								5         C       U
								6         4       12
								7         5       13
								8         6       14
								9         7       15
								10        8       16
END;





//------------------------------------------------------
// Rope and Umbilical Counter Boards





// Side Ropes


xCOUNTERBOARD: northrope
	DIGITAL:    0XF4   // Hex address of counter board
	DCSLOT:     1      // Slot in DC Chassis (1-8)
	ANALOG:     1      // Analog Channel on Data Concentrator Card (1-4)
END;

xCOUNTERBOARD: southrope
	DIGITAL:    0XDC        // Hex address of counter board
	DCSLOT:     6           // Slot in DC chassis
	ANALOG:     4           // Analog Channel on Data Concentrator Card
END;


xCOUNTERBOARD: eastROPE
	DIGITAL:    0XF8   // Hex address of counter board
	DCSLOT:     8      // Slot in DC Chassis (1-8)
	ANALOG:     4      // Analog Channel on Data Concentrator Card (1-4)
END;

xCOUNTERBOARD: westROPE
	DIGITAL:    0XD0        // Hex address of counter board
	DCSLOT:     6           // Slot in DC chassis
	ANALOG:     1           // Analog Channel on Data Concentrator Card
END;




// URM-1

xCOUNTERBOARD: urm1rope
	DIGITAL:    0XAC        // Hex address of counter board
	DCSLOT:     6           // Slot in DC chassis
	ANALOG:     3           // Analog Channel on Data Concentrator Card
END;

xCOUNTERBOARD: urm1umbilical
	DIGITAL:    0XA0        // Hex address of counter board
	DCSLOT:     6           // Slot in DC chassis
	ANALOG:     2           // Analog Channel on Data Concentrator Card
END;




// URM-2

xCOUNTERBOARD: urm2rope
	DIGITAL:    0XAC        // Hex address of counter board
	DCSLOT:     5           // Slot in DC chassis
	ANALOG:     4           // Analog Channel on Data Concentrator Card
END;


xCOUNTERBOARD: urm2umbilical
	DIGITAL:    0XF4        // Hex address of counter board
	DCSLOT:     1           // Slot in DC chassis
	ANALOG:     2           // Analog Channel on Data Concentrator Card
END;





// Test URM

xCOUNTERBOARD: testUmbilical
	DIGITAL:    0Xf4  // Hex address of counter board
	DCSLOT:     1           // Slot in DC chassis
	ANALOG:     3        // Analog Channel on Data Concentrator Card
END;

xCOUNTERBOARD: testRope
	DIGITAL:    0XC4   // Hex address of counter board
	DCSLOT:     1           // Slot in DC chassis
	ANALOG:     4        // Analog Channel on Data Concentrator Card
END;


//-------------------------------------------------
// motors

//xMOTOR: northropemotor
//  CONNECTOR:    8        // a number from 1 to 16
//END;

//xMOTOR: southropemotor
//  CONNECTOR:    6       // a number from 1 to 16
//END;

//xMOTOR: dyelasermotor
	//CONNECTOR:    5
//END;

//xMOTOR: filterwheelamotor
//  CONNECTOR:    1
//END;

//xMOTOR: filterwheelbmotor
//  CONNECTOR:    2
//END;

//MOTOR: eastropemotor
//	CONNECTOR:    8       // a number from 1 to 16
//END;


//xMOTOR: westropemotor
//  CONNECTOR:    8        // a number from 1 to 16
//END;

//xMOTOR: urm1ropemotor
//  CONNECTOR:    9        // a number from 1 to 16
//END;

//MOTOR: urm1UmbilicalMotor
//  CONNECTOR:   10
//END;


//xMOTOR: urm2ropemotor
//  CONNECTOR:   11        // a number from 1 to 16
//END;

//xMOTOR: urm2umbilicalmotor
//  CONNECTOR:   12
//END;

//xMOTOR: centralrope2motor
//  CONNECTOR:   13        // a number from 1 to 16
//END;

//xMOTOR: thetaMotor
//        CONNECTOR:    11
//END;

//xMOTOR: phiMotor
//        MASTER:   thetaMotor
//        CONNECTOR:  14
//END;

//xMOTOR: testRopeMotor
//  CONNECTOR:    15       // a number from 1 to 16
//END;

//xMOTOR: testUmbilicalMotor
//  CONNECTOR:    16
//END;


// -------------------------------------------------------
// Connections and control for the N2 LASER

CBLASERPOWER: N2LASERPOWERSWITCH
	DCSLOT:     8
	DIGITAL:    0xEC
END;

CBLASERTRIGGER: N2LASERTRIGGERSWITCH
	DCSLOT:     8
	DIGITAL:    0xEC
END;

COUNTERBOARD: N2LASERDEBUG
	DCSLOT:  8
	DIGITAL: 0xEC
END;

COUNTERBOARD: N2LASERSTATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0700
END;

COUNTERBOARD: N2LASERPOWERSTATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0800
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERLOCKSTATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0040
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERTRIGGERSTATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0080
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERWARMUPSTATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0100
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERREADYSTATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0200
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERRUNNINGSTATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0400
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASER_40V_STATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0010
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASER_12V_STATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0020
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERDYEMOTOR1STATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x8000
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERDYEMOTOR2STATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x4000
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERDYEMOTOR3STATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x2000
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERDYEMOTOR4STATUS
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x1000
	SHARED_WITH: N2LASERSTATUS
END;

COUNTERBOARD: N2LASERHIPRESSURE
	DCSLOT:  8
	ANALOG:     3
END;

COUNTERBOARD: N2LASERLOWPRESSURE
	DCSLOT:     8
	ANALOG:     1
END;

// Connections and control for the DYE LASER

COUNTERBOARD: DYELASERENCODER
	DCSLOT:     8
	DIGITAL:    0XE4
END;

COUNTERBOARD: DYELASERLIMITSWITCH1
	DCSLOT:     8
	DIGITAL:    0xEC
	MASK:       0x0008
END;

COUNTERBOARD: DYELASERLIMITSWITCH2
	DCSLOT:     8
	DIGITAL:    0xEC
	MASK:       0x0004
	SHARED_WITH: DYELASERLIMITSWITCH1
END;

MOTOR: dyeLaserMotor
	CONNECTOR:    1       // a number from 1 to 16
END;

PULSER: laserTriggerPulser
	CONNECTOR:        4
END;

LOCKOUT: lockout
	PULSER:     laserTriggerPulser
	CHANNEL:    0
	MAX_LENGTH: 100
END;


PULSER: ledPulser
	CONNECTOR:    6
END;


// Connections and control for the ND filter wheels

COUNTERBOARD: FILTERWHEELAENCODER
	DCSLOT:     8
	DIGITAL:    0XE8
END;

COUNTERBOARD: FILTERWHEELBENCODER
	DCSLOT:     8
	DIGITAL:    0XE0
END;

COUNTERBOARD: FILTERWHEELASWITCH
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0001
END;

COUNTERBOARD: FILTERWHEELBSWITCH
	DCSLOT:  8
	DIGITAL: 0xEC
	MASK: 0x0002
END;

MOTOR: filterWheelAMotor
	CONNECTOR:    2       // a number from 1 to 16
END;

MOTOR: filterWheelBMotor
	CONNECTOR:    3       // a number from 1 to 16
END;


// The AV stuff

xSENSEROPE: SROPE1
	DIGITAL: 0x80
	DCSLOT:  1
END;
xSENSEROPE: SROPE2
	DIGITAL: 0x84
	DCSLOT:  2
END;
xSENSEROPE: SROPE3
	DIGITAL: 0x88
	DCSLOT:  2
END;
xSENSEROPE: SROPE4
	DIGITAL: 0x8c
	DCSLOT:  2
END;
xSENSEROPE: SROPE5
	DIGITAL: 0x90
	DCSLOT:  2
END;
xSENSEROPE: SROPE6
DIGITAL: 0x94
	DCSLOT:  1
END;
xSENSEROPE: SROPE7
	DIGITAL: 0x98
	DCSLOT:  1
END;

