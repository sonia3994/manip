#ifndef __LPM16_H
#define __LPM16_H

#define LPM_base 0x260     // some addresses -- don't know from whence
#define LPM_board_number 1

class lpm16
{
public:
	int board;
	int status;
	lpm16(int argboard=LPM_board_number);

};

#endif

