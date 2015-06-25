#include "pulser.h"
#include "display.h"
#include "avr32.h"

// we can reload the counter before it runs to zero,
const unsigned short  kMinimumCount = 300;
// and the maximum number of retries while reading
// the counter in order to achieve the specified minimum counts
const int             kMaxRetries   = 100;

const unsigned        kMaxDivisor   = 60000U;
const unsigned        kMinDivisor   = 2;
const int             kBuffSize     = 64;

// static member declarations
Pulser::Stop      Pulser::cmd_stop;
Pulser::Run       Pulser::cmd_run;
Pulser::Rate      Pulser::cmd_rate;

//  11-15 F1-f5 F1 is the oscillator, 2-5 are divided by 10,100,1000,10000
//        or 16,256,4096,65536 depending on MM15
static double freq_list[] = { 0, 0, 0, 0,
                              0, 5e6, 0, 0,
                              0, 0, 0, 1e6,
                              1e6/16, 1e6/256, 1e6/4096, 1e6/65536.0 };


Command *Pulser::commandList[PULSER_COMMAND_NUMBER] = {
  &cmd_stop,
  &cmd_run,
  &cmd_rate,
};


Pulser::Pulser(char* name,      // pulser name
             AVR32 *avr,        // timer card for source clock
             char *aChan):      // AVR device channel
             Controller("Pulser",name), AVRChannel(avr,aChan)
{
  running = 0;

  InputFile in("PULSER",name,1.00);

  source  = in.nextFloat("SOURCE:");
  minRate = in.nextFloat("MIN_RATE:");
  maxRate = in.nextFloat("MAX_RATE:");
  curRate = in.nextFloat("RATE:");

  if (source<0 || source>15 || !freq_list[source]) {
    qprintf("Illegal source channel for Pulser %s\n",name);
    quit();
  }
  if (curRate<minRate || curRate>maxRate) {
    qprintf("Pulser %s rate is outside range\n",name);
    quit();
  }

  // frequency of a full square wave cycle (hence the divide by 2)
  clock_freq = 0.5 * freq_list[source];

  if (maxRate > clock_freq/kMinDivisor) {
    qprintf("Pulser %s maximum rate is too high for specified source\n",name);
    quit();
  }
  if (minRate < clock_freq/kMaxDivisor) {
    qprintf("Pulser %s minimum rate is too low for specified source\n",name);
    quit();
  }
}


void Pulser::monitor(char *outStr)
{
  outStr += sprintf(outStr,"Status: %s\n", running ? "RUNNING" : "STOPPED");
  outStr += sprintf(outStr,"Rate:   %.2f Hz\n", curRate);
}

void Pulser::stop()
{/*
  if (running) {
    avrCommand("hold");
    running = 0;
    display->message("Pulser stopped");
  } else {
    display->message("Pulser was not running");
  }*/
}

void Pulser::run()
{
/*  if (running) {
    display->message("Pulser is already running");
  } else {
    char buff[kBuffSize];
    sprintf(buff,"%s ramp %d",mChannel,(int)curRate);
    mAVR->sendCommand(buff);
    running = 1;
    sprintf(display->outString,"Pulser started with rate %.2f Hz",curRate);
    display->message();
  }*/
}

void Pulser::setRate(float newRate)
{/*
  if (newRate<minRate || newRate>maxRate) {
    display->message("Pulser rate out of range");
  } else {
    curRate = newRate;
    OutputFile  out("PULSER",getObjectName(),1.00); // create output file object
    out.updateFloat("RATE:", newRate);
    if (running) {
      for (int retries=0; retries<kMaxRetries; ++retries) {
        unsigned left = am9513->gethold();
        if (left >= kMinimumCount) break;
      }
      am9513->setload((unsigned)(clock_freq/curRate));
      sprintf(display->outString,"Pulser rate changed to %.2f Hz while running",curRate);
      display->message();
    } else {
      sprintf(display->outString,"Rate set to %.2f Hz",curRate);
      display->message();
    }
  }*/
}



void Pulser::setDigitalOutput(int channel, int value)
{
/*#ifdef NO_ALVINS_BOX
  switch (channel) {
    case 0:
      if (value) {
        port->maskoff(channel0Mask);
      } else {
        port->maskon(channel0Mask);
      }
      break;
    case 1:
      if (value) {
        port->maskoff(channel1Mask);
      } else {
        port->maskon(channel1Mask);
      }
      break;
  }
#else
  switch (channel) {
    case 0:
      if (value) {
        port->maskon(channel0Mask);
      } else {
        port->maskoff(channel0Mask);
      }
      break;
    case 1:
      if (value) {
        port->maskon(channel1Mask);
      } else {
        port->maskoff(channel1Mask);
      }
      break;
  }
#endif */
}
