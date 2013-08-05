/* -------------------------------------------------------------
 *
 *  file:        command.cpp
 *
 *  description: command line interpreter for Chip/Wafer tester
 *
 *  author:      Beat Meier
 *  modified:    31.8.2007
 *
 *  rev:
 *
 * -------------------------------------------------------------
 */


#include <math.h>
#include <time.h>
#include <fstream>
#include "psi46test.h"
#include "plot.h"

#include "command.h"

#include <vector>

using namespace std;


#ifndef _WIN32
#define _int64 long long
#endif

#define DO_FLUSH  if (par.isInteractive()) tb.Flush();

using namespace std;

#define FIFOSIZE 8192

// =======================================================================
//  connection, communication, startup commands
// =======================================================================

CMD_PROC(scan)
{
	CTestboard *tb = new CTestboard;

	unsigned int nDev;
	char name[64];

	if (tb->EnumFirst(nDev))
	{
		if (nDev == 0)
			printf("No devices connected.\n");
		for (unsigned int i=0; i<nDev; i++)
		{
			if (tb->EnumNext(name))
			{
				printf("%2u: %s", i, name);
				if (tb->Open(name,true))
				{
					try {
						unsigned int bid = tb->GetBoardId();
						printf("  BID=%2u;", bid);
						tb->Close();
					}
					catch (...){
						printf(" -> Device not identifiable\n");
						tb->Close();
					}
				}
				else printf(" - in use\n");
			}
		}
	}
	else puts("error\n");

	delete tb;

	return true;
}

CMD_PROC(open)
{
	char name[80];
	PAR_STRING(name,79);
	bool status;
	status = tb.Open(name,false);
	if (!status) {
		printf("USB error: %s\n", tb.ConnectionError());
		return false;
	}
	return true;
}

CMD_PROC(close)
{
	tb.Close();
	return true;
}

CMD_PROC(welcome)
{
	tb.Welcome();
	DO_FLUSH
	return true;
}

CMD_PROC(setled)
{
	int value;
	PAR_INT(value, 0, 0x3f);
	tb.SetLed(value);
	DO_FLUSH
	return true;
}


bool UpdateDTB(const char *filename)
{
	fstream src;

	if (tb.UpgradeGetVersion() == 0x0100)
	{
		// open file
		src.open(filename);
		if (!src.is_open())
		{
			printf("ERROR UPGRADE: Could not open \"%s\"!\n", filename);
			return false;
		}

		// check if upgrade is possible
		printf("Start upgrading DTB.\n");
		if (tb.UpgradeStart(0x0100) != 0)
		{
			string msg;
			tb.UpgradeErrorMsg(msg);
			printf("ERROR UPGRADE: %s!\n", msg.data());
			return false;
		}

		// download data
		printf("Download running ...\n");
		string rec;
		uint16_t recordCount = 0;
		while (true)
		{
			getline(src, rec);
			if (src.good())
			{
				if (rec.size() == 0) continue;
				recordCount++;
				if (tb.UpgradeData(rec) != 0)
				{
					string msg;
					tb.UpgradeErrorMsg(msg);
					printf("ERROR UPGRADE: %s!\n", msg.data());
					return false;
				}
			}
			else if (src.eof()) break;
			else
			{
				printf("ERROR UPGRADE: Error reading \"%s\"!\n", filename);
				return false;
			}
		}

		if (tb.UpgradeError() != 0)
		{
			string msg;
			tb.UpgradeErrorMsg(msg);
			printf("ERROR UPGRADE: %s!\n", msg.data());
			return false;
		}
		
		// write EPCS FLASH
		printf("DTB download complete.\n");
		tb.mDelay(200);
		printf("FLASH write start (LED 1..4 on)\n"
			   "DO NOT INTERUPT DTB POWER !\n"
			   "Wait till LEDs goes off.\n"
		       "Restart the DTB.\n");
		tb.UpgradeExec(recordCount);

		return true;
	}

	printf("ERROR UPGRADE: Could not upgrade this DTB version!\n");
	return false;
}


CMD_PROC(upgrade)
{
	char filename[256];
	PAR_STRING(filename, 255);
	if (UpdateDTB(filename))
		printf("DTB download complete.\n"
		       "FLASH write starts. LED 1..4 on\n"
			   "Wait till LEDs goes off. Don't interrupt DTB power if LEDs are on!\n"
		       "Restart the DTB.\n");
	return true;
}


CMD_PROC(info)
{
	string s;
	tb.GetInfo(s);
	printf("--- DTB info ----------------------------\n%s"
		   "-----------------------------------------\n", s.c_str());
	return true;
}

CMD_PROC(ver)
{
	string hw;
	tb.GetHWVersion(hw);
	int fw = tb.GetFWVersion();
	int sw = tb.GetSWVersion();
	printf("%s: FW=%i.%02i SW=%i.%02i\n", hw.c_str(), fw/256, fw%256, sw/256, sw%256);
	return true;
}

CMD_PROC(version)
{
	string hw;
	tb.GetHWVersion(hw);
	int fw = tb.GetFWVersion();
	int sw = tb.GetSWVersion();
	printf("%s: FW=%i.%02i SW=%i.%02i\n", hw.c_str(), fw/256, fw%256, sw/256, sw%256);
	return true;
}

CMD_PROC(boardid)
{
	int id = tb.GetBoardId();
	printf("\nBoard Id = %i\n", id);
	return true;
}

CMD_PROC(init)
{
	tb.Init();
	DO_FLUSH
	return true;
}

CMD_PROC(flush)
{
	tb.Flush();
	return true;
}

CMD_PROC(clear)
{
	tb.Clear();
	return true;
}




// =======================================================================
//  delay commands
// =======================================================================

CMD_PROC(udelay)
{
	int del;
	PAR_INT(del, 0, 1000);
	if (del) tb.uDelay(del);
	DO_FLUSH
	return true;
}

CMD_PROC(mdelay)
{
	int ms;
	PAR_INT(ms,1,10000)
	tb.mDelay(ms);
	return true;
}


// =======================================================================
//  test board commands
// =======================================================================


/*
CMD_PROC(clock)
{
	if (tb.isClockPresent())
		printf("clock ok\n");
	else
		printf("clock missing\n");
	return true;
}

CMD_PROC(fsel)
{
	int div;
	PAR_INT(div, 0, 5)

	tb.SetClock(div);
	DO_FLUSH
	return true;
}

CMD_PROC(stretch)
{
	int src, delay, width;
	PAR_INT(src,   0,      3)
	PAR_INT(delay, 0,   1023);
	PAR_INT(width, 0, 0xffff);
	tb.SetClockStretch(src,delay,width);
	DO_FLUSH
	return true;
}
*/

CMD_PROC(clk)
{
	int ns, duty;
	PAR_INT(ns,0,400);
	if (!PAR_IS_INT(duty, -8, 8)) duty = 0;
	tb.Sig_SetDelay(SIG_CLK, ns, duty);
	DO_FLUSH
	return true;
}

CMD_PROC(sda)
{
	int ns, duty;
	PAR_INT(ns,0,400);
	if (!PAR_IS_INT(duty, -8, 8)) duty = 0;
	tb.Sig_SetDelay(SIG_SDA, ns, duty);
	DO_FLUSH
	return true;
}

/*
CMD_PROC(rda)
{
	int ns;
	PAR_INT(ns,0,400);
	tb.SetDelay(DELAYSIG_RDA, ns);
	DO_FLUSH
	return true;
}
*/

CMD_PROC(ctr)
{
	int ns, duty;
	PAR_INT(ns,0,400);
	if (!PAR_IS_INT(duty, -8, 8)) duty = 0;
	tb.Sig_SetDelay(SIG_CTR, ns, duty);
	DO_FLUSH
	return true;
}

CMD_PROC(tin)
{
	int ns, duty;
	PAR_INT(ns,0,400);
	if (!PAR_IS_INT(duty, -8, 8)) duty = 0;
	tb.Sig_SetDelay(SIG_TIN, ns, duty);
	DO_FLUSH
	return true;
}


CMD_PROC(clklvl)
{
	int lvl;
	PAR_INT(lvl,0,15);
	tb.Sig_SetLevel(SIG_CLK, lvl);
	DO_FLUSH
	return true;
}

CMD_PROC(sdalvl)
{
	int lvl;
	PAR_INT(lvl,0,15);
	tb.Sig_SetLevel(SIG_SDA, lvl);
	DO_FLUSH
	return true;
}

CMD_PROC(ctrlvl)
{
	int lvl;
	PAR_INT(lvl,0,15);
	tb.Sig_SetLevel(SIG_CTR, lvl);
	DO_FLUSH
	return true;
}

CMD_PROC(tinlvl)
{
	int lvl;
	PAR_INT(lvl,0,15);
	tb.Sig_SetLevel(SIG_TIN, lvl);
	DO_FLUSH
	return true;
}


CMD_PROC(clkmode)
{
	int mode;
	PAR_INT(mode,0,3);
	if (mode == 3)
	{
		int speed;
		PAR_INT(speed,0,31);
		tb.Sig_SetPRBS(SIG_CLK, speed);
	}
	else
		tb.Sig_SetMode(SIG_CLK, mode);
	DO_FLUSH
	return true;
}

CMD_PROC(sdamode)
{
	int mode;
	PAR_INT(mode,0,3);
	if (mode == 3)
	{
		int speed;
		PAR_INT(speed,0,31);
		tb.Sig_SetPRBS(SIG_SDA, speed);
	}
	else
		tb.Sig_SetMode(SIG_SDA, mode);
	DO_FLUSH
	return true;
}

CMD_PROC(ctrmode)
{
	int mode;
	PAR_INT(mode,0,3);
	if (mode == 3)
	{
		int speed;
		PAR_INT(speed,0,31);
		tb.Sig_SetPRBS(SIG_CTR, speed);
	}
	else
		tb.Sig_SetMode(SIG_CTR, mode);
	DO_FLUSH
	return true;
}

CMD_PROC(tinmode)
{
	int mode;
	PAR_INT(mode,0,3);
	if (mode == 3)
	{
		int speed;
		PAR_INT(speed,0,31);
		tb.Sig_SetPRBS(SIG_TIN, speed);
	}
	else
		tb.Sig_SetMode(SIG_TIN, mode);
	DO_FLUSH
	return true;
}


CMD_PROC(sigoffset)
{
	int offset;
	PAR_INT(offset, 0, 15);
	tb.Sig_SetOffset(offset);
	DO_FLUSH
	return true;
}

CMD_PROC(lvds)
{
	tb.Sig_SetLVDS();
	DO_FLUSH
	return true;
}

CMD_PROC(lcds)
{
	tb.Sig_SetLCDS();
	DO_FLUSH
	return true;
}

/*
CMD_PROC(tout)
{
	int ns;
	PAR_INT(ns,0,450);
	tb.SetDelay(SIGNAL_TOUT, ns);
	DO_FLUSH
	return true;
}

CMD_PROC(trigout)
{
	int ns;
	PAR_INT(ns,0,450);
	tb.SetDelay(SIGNAL_TRGOUT, ns);
	DO_FLUSH
	return true;
}
*/


CMD_PROC(pon)
{
	tb.Pon();
	DO_FLUSH
	return true;
}

CMD_PROC(poff)
{
	tb.Poff();
	DO_FLUSH
	return true;
}

CMD_PROC(va)
{
	int value;
	PAR_INT(value, 0, 4000);
	tb._SetVA(value);
	DO_FLUSH
	return true;
}

CMD_PROC(vd)
{
	int value;
	PAR_INT(value, 0, 4000);
	tb._SetVD(value);
	DO_FLUSH
	return true;
}


CMD_PROC(ia)
{
	int value;
	PAR_INT(value, 0, 1200);
	tb._SetIA(value*10);
	DO_FLUSH
	return true;
}

CMD_PROC(id)
{
	int value;
	PAR_INT(value, 0, 1200);
	tb._SetID(value*10);
	DO_FLUSH
	return true;
}

CMD_PROC(getva)
{
	double v = tb.GetVA();
	printf("\n VA = %1.3fV\n", v);
	return true;
}

CMD_PROC(getvd)
{
	double v = tb.GetVD();
	printf("\n VD = %1.3fV\n", v);
	return true;
}

CMD_PROC(getia)
{
	double i = tb.GetIA();
	printf("\n IA = %1.1fmA\n", i*1000.0);
	return true;
}

CMD_PROC(getid)
{
	double i = tb.GetID();
	printf("\n ID = %1.1fmA\n", i*1000.0);
	return true;
}


CMD_PROC(hvon)
{
	tb.HVon();
	DO_FLUSH
	return true;
}

CMD_PROC(hvoff)
{
	tb.HVoff();
	DO_FLUSH
	return true;
}

CMD_PROC(reson)
{
	tb.ResetOn();
	DO_FLUSH
	return true;
}

CMD_PROC(resoff)
{
	tb.ResetOff();
	DO_FLUSH
	return true;
}


CMD_PROC(status)
{
	uint8_t status = tb.GetStatus();
	printf("SD card detect: %c\n", (status&8) ? '1' : '0');
	printf("CRC error:      %c\n", (status&4) ? '1' : '0');
	printf("Clock good:     %c\n", (status&2) ? '1' : '0');
	printf("CLock present:  %c\n", (status&1) ? '1' : '0');
	return true;
}

CMD_PROC(rocaddr)
{
	int addr;
	PAR_INT(addr, 0, 15)
	tb.SetRocAddress(addr);
	DO_FLUSH
	return true;
}

CMD_PROC(d1)
{
	int sig;
	PAR_INT(sig, 0, 31)
	tb.SignalProbeD1(sig);
	DO_FLUSH
	return true;
}


CMD_PROC(d2)
{
	int sig;
	PAR_INT(sig, 0, 31)
	tb.SignalProbeD2(sig);
	DO_FLUSH
	return true;
}

CMD_PROC(a1)
{
	int sig;
	PAR_INT(sig, 0, 7)
	tb.SignalProbeA1(sig);
	DO_FLUSH
	return true;
}

CMD_PROC(a2)
{
	int sig;
	PAR_INT(sig, 0, 7)
	tb.SignalProbeA2(sig);
	DO_FLUSH
	return true;
}

CMD_PROC(probeadc)
{
	int sig;
	PAR_INT(sig, 0, 7)
	tb.SignalProbeADC(sig);
	DO_FLUSH
	return true;
}

CMD_PROC(pgset)
{
	int addr, delay, bits;
	PAR_INT(addr,0,255)
	PAR_INT(bits,0,63)
	PAR_INT(delay,0,255)
	tb.Pg_SetCmd(addr, (bits<<8) + delay);
	DO_FLUSH
	return true;
}

CMD_PROC(pgstop)
{
	tb.Pg_Stop();
	DO_FLUSH
	return true;
}

CMD_PROC(pgsingle)
{
	tb.Pg_Single();
	DO_FLUSH
	return true;
}

CMD_PROC(pgtrig)
{
	tb.Pg_Trigger();
	DO_FLUSH
	return true;
}

CMD_PROC(pgloop)
{
	int period;
	PAR_INT(period,0,65535)
	tb.Pg_Loop(period);
	DO_FLUSH
	return true;
}


// === DAQ ==================================================================

CMD_PROC(dopen)
{
	int buffersize;
	PAR_INT(buffersize, 0, 60000000);
	buffersize = tb.Daq_Open(buffersize);
	printf("%i words allocated for data buffer\n", buffersize);
	if (buffersize == 0) printf("error\n");
	return true;
}

CMD_PROC(dclose)
{
	tb.Daq_Close();
	DO_FLUSH
	return true;
}


CMD_PROC(dstart)
{
	tb.Daq_Start();
	DO_FLUSH
	return true;
}

CMD_PROC(dstop)
{
	tb.Daq_Stop();
	DO_FLUSH
	return true;
}


CMD_PROC(dsize)
{
	unsigned int size = tb.Daq_GetSize();
	printf("size = %u\n", size);
	return true;
}

CMD_PROC(dread)
{
	uint32_t words_remaining;
	vector<uint16_t> data;
	tb.Daq_Read(data, words_remaining);
	int size = data.size();
	printf("#samples: %i  remaining: %i\n", size, int(words_remaining));

	for (int i=0; i<size; i++)
	{
		int x = data[i] & 0x0fff;
		printf(" %03X", x);
		if (i%10 == 9) printf("\n");
	}
	printf("\n");

	for (int i=0; i<size; i++)
	{
		int x = data[i] & 0x0fff;
		Log.printf("%03X", x);
		if (i%100 == 9) printf("\n");
	}
	printf("\n");

	return true;
}

CMD_PROC(dreada)
{
	uint32_t words_remaining;
	vector<uint16_t> data;
	tb.Daq_Read(data, words_remaining);
	int size = data.size();
	printf("#samples: %i remaining: %i\n", size, int(words_remaining));

	for (int i=0; i<size; i++)
	{
		int x = data[i] & 0x0fff;
		if (x & 0x0800) x |= 0xfffff000;
		printf(" %6i", x);
		if (i%10 == 9) printf("\n");
	}
	printf("\n");

	for (int i=0; i<size; i++)
	{
		int x = data[i] & 0x0fff;
		if (x & 0x0800) x |= 0xfffff000;
		Log.printf("%6i", x);
		if (i%100 == 9) printf("\n");
	}
	printf("\n");

	return true;
}

CMD_PROC(takedata)
{
	FILE *f = fopen("daqdata.bin", "wb");
	if (f == 0)
	{
		printf("Could not open data file\n");
		return true;
	}

	uint8_t status = 0;
	uint32_t n;
	vector<uint16_t> data;

	unsigned int sum = 0;
	double mean_n = 0.0;
	double mean_size = 0.0;

	clock_t t = clock();
	unsigned int memsize = tb.Daq_Open(10000000);
	tb.Daq_Start();
	clock_t t_start = clock();
	while (!keypressed())
	{
		// read data and status from DTB
		status = tb.Daq_Read(data, n, 40000);

		// make statistics
		sum += data.size();
		mean_n    = /* 0.2*mean_n    + 0.8* */ n;
		mean_size = /* 0.2*mean_size + 0.8* */ data.size();

		// write statistics every second
//		printf(".");
		if ((clock() - t) > CLOCKS_PER_SEC/4)
		{
			printf("%5.1f%%  %5.0f  %u\n", mean_n*100.0/memsize, mean_size, sum);
			t = clock();
		}

		// write data to file
		if (fwrite(data.data(), sizeof(uint16_t), data.size(), f) != data.size())
		{ printf("\nFile write error"); break; }

		// abort after overflow error
//		if (((status & 1) == 0) && (n == 0)) break;
		if (status & 0x6) break;

//		tb.mDelay(5);
	}
	tb.Daq_Stop();
	double t_run = double(clock() - t_start)/CLOCKS_PER_SEC; 

	tb.Daq_Close();

	if (keypressed()) getchar();

	printf("\n");
	if      (status & 4) printf("FIFO overflow\n");
	else if (status & 2) printf("Memory overflow\n");
	printf("Data taking aborted\n%u samples in %0.1f s read (%0.0f samples/s)\n", sum, t_run, sum/t_run);

	fclose(f);
	return true;
}



#define DECBUFFERSIZE 2048

class Decoder
{
	int printEvery;

	int nReadout;
	int nPixel;

	FILE *f;
	int nSamples;
	uint16_t *samples;

	int x, y, ph;
	void Translate(unsigned long raw);
public:
    Decoder() : printEvery(0), nReadout(0), nPixel(0), f(0), nSamples(0), samples(0) {}
	~Decoder() { Close(); }
	bool Open(const char *filename);
	void Close() { if (f) fclose(f); f = 0; delete[] samples; }
	bool Sample(uint16_t sample);
	void AnalyzeSamples();
	void DumpSamples(int n);
};

bool Decoder::Open(const char *filename)
{
	samples = new uint16_t[DECBUFFERSIZE];
	f = fopen(filename, "wt");
	return f != 0;
}

void Decoder::Translate(unsigned long raw)
{
	ph = (raw & 0x0f) + ((raw >> 1) & 0xf0);
	raw >>= 9;
	int c =    (raw >> 12) & 7;
	c = c*6 + ((raw >>  9) & 7);
	int r =    (raw >>  6) & 7;
	r = r*6 + ((raw >>  3) & 7);
	r = r*6 + ( raw        & 7);
	y = 80 - r/2;
	x = 2*c + (r&1);
}

void Decoder::AnalyzeSamples()
{
	if (nSamples < 1) { nPixel = 0; return; }
	fprintf(f, "%5i: %03X: ", nReadout, (unsigned int)(samples[0] & 0xfff));
	nPixel = (nSamples-1)/2;
	int pos = 1;
	for (int i=0; i<nPixel; i++)
	{
		unsigned long raw = (samples[pos++] & 0xfff) << 12;
		raw += samples[pos++] & 0xfff;
		Translate(raw);
		fprintf(f, " %2i", x);
	}

//	for (pos = 1; pos < nSamples; pos++) fprintf(f, " %03X", int(samples[pos]));
	fprintf(f, "\n");

}

void Decoder::DumpSamples(int n)
{
	if (nSamples < n) n = nSamples;
	for (int i=0; i<n; i++) fprintf(f, " %04X", (unsigned int)(samples[i]));
	fprintf(f, " ... %04X\n", (unsigned int)(samples[nSamples-1]));
}

bool Decoder::Sample(uint16_t sample)
{
	if (sample & 0x8000) // start marker
	{
		if (nReadout && printEvery >= 1000)
		{
			AnalyzeSamples();
			printEvery = 0;
		} else printEvery++;
		nReadout++;
		nSamples = 0;
	}
	if (nSamples < DECBUFFERSIZE)
	{
		samples[nSamples++] = sample;
		return true;
	}
	return false;
}



CMD_PROC(takedata2)
{
	Decoder dec;
	if (!dec.Open("daqdata2.txt"))
	{
		printf("Could not open data file\n");
		return true;
	}

	uint8_t status = 0;
	uint32_t n;
	vector<uint16_t> data;

	unsigned int sum = 0;
	double mean_n = 0.0;
	double mean_size = 0.0;

	clock_t t = clock();
	unsigned long memsize = tb.Daq_Open(1000000);
	tb.Daq_Select_Deser160(4);
	tb.Daq_Start();
	clock_t t_start = clock();
	while (!keypressed())
	{
		// read data and status from DTB
		status = tb.Daq_Read(data, n, 20000);

		// make statistics
		sum += data.size();
		mean_n    = /* 0.2*mean_n    + 0.8* */ n;
		mean_size = /* 0.2*mean_size + 0.8* */ data.size();

		// write statistics every second
//		printf(".");
		if ((clock() - t) > CLOCKS_PER_SEC)
		{
			printf("%5.1f%%  %5.0f  %u\n", mean_n*100.0/memsize, mean_size, sum);
			t = clock();
		}

		// decode file
		for (unsigned int i=0; i<data.size(); i++) dec.Sample(data[i]);

		// abort after overflow error
		if (((status & 1) == 0) && (n == 0)) break;

		tb.mDelay(5);
	}
	tb.Daq_Stop();
	double t_run = double(clock() - t_start)/CLOCKS_PER_SEC; 

	tb.Daq_Close();

	if (keypressed()) getchar();

	printf("\nstatus: %02X ", int(status));
	if      (status & 4) printf("FIFO overflow\n");
	else if (status & 2) printf("Memory overflow\n");
	printf("Data taking aborted\n%u samples in %0.1f s read (%0.0f samples/s)\n", sum, t_run, sum/t_run);

	return true;
}

CMD_PROC(showclk)
{
	const unsigned int nSamples = 20;
	const int gain = 1;
//	PAR_INT(gain,1,4);

	unsigned int i, k;
	uint32_t buffersize;
	vector<uint16_t> data[20];

	tb.Pg_Stop();
	tb.Pg_SetCmd( 0, PG_SYNC +  5);
	tb.Pg_SetCmd( 1, PG_CAL  +  6);
	tb.Pg_SetCmd( 2, PG_TRG  +  6);
	tb.Pg_SetCmd( 3, PG_RESR +  6);
	tb.Pg_SetCmd( 4, PG_REST +  6);
	tb.Pg_SetCmd( 5, PG_TOK);

	tb.SignalProbeD1(9);
	tb.SignalProbeD2(17);
	tb.SignalProbeA2(PROBEA_CLK);
	tb.SignalProbeADC(PROBEA_CLK, gain-1);

	tb.Daq_Select_ADC(nSamples, 1, 1);
	tb.uDelay(1000);
	tb.Daq_Open(1024);
	for (i=0; i<20; i++)
	{
		tb.Sig_SetDelay(SIG_CLK, 26-i);
		tb.uDelay(10);
		tb.Daq_Start();
		tb.Pg_Single();
		tb.uDelay(1000);
		tb.Daq_Stop();
		tb.Daq_Read(data[i], buffersize, 1024);
		if (data[i].size() != nSamples)
		{
			printf("Data size %i: %i\n", i, int(data[i].size()));
			return true;
		}
	}
	tb.Daq_Close();
	tb.Flush();

	int n = 20*nSamples;
	vector<double> values(n);
	int x = 0;
	for (k=0; k<nSamples; k++) for (i=0; i<20; i++)
	{
		int y = (data[i])[k] & 0x0fff;
		if (y & 0x0800) y |= 0xfffff000;
		values[x++] = y;
	}
	Scope("CLK", values);

	return true;
}

CMD_PROC(showctr)
{
	const unsigned int nSamples = 60;
	const int gain = 1;
//	PAR_INT(gain,1,4);

	unsigned int i, k;
	uint32_t buffersize;
	vector<uint16_t> data[20];

	tb.Pg_Stop();
	tb.Pg_SetCmd( 0, PG_SYNC +  5);
	tb.Pg_SetCmd( 1, PG_CAL  +  6);
	tb.Pg_SetCmd( 2, PG_TRG  +  6);
	tb.Pg_SetCmd( 3, PG_RESR +  6);
	tb.Pg_SetCmd( 4, PG_REST +  6);
	tb.Pg_SetCmd( 5, PG_TOK);

	tb.SignalProbeD1(9);
	tb.SignalProbeD2(17);
	tb.SignalProbeA2(PROBEA_CTR);
	tb.SignalProbeADC(PROBEA_CTR, gain-1);

	tb.Daq_Select_ADC(nSamples, 1, 1);
	tb.uDelay(1000);
	tb.Daq_Open(1024);
	for (i=0; i<20; i++)
	{
		tb.Sig_SetDelay(SIG_CTR, 26-i);
		tb.uDelay(10);
		tb.Daq_Start();
		tb.Pg_Single();
		tb.uDelay(1000);
		tb.Daq_Stop();
		tb.Daq_Read(data[i], buffersize, 1024);
		if (data[i].size() != nSamples)
		{
			printf("Data size %i: %i\n", i, int(data[i].size()));
			return true;
		}
	}
	tb.Daq_Close();
	tb.Flush();

	int n = 20*nSamples;
	vector<double> values(n);
	int x = 0;
	for (k=0; k<nSamples; k++) for (i=0; i<20; i++)
	{
		int y = (data[i])[k] & 0x0fff;
		if (y & 0x0800) y |= 0xfffff000;
		values[x++] = y;
	}
	Scope("CTR", values);

/*
	FILE *f = fopen("X:\\developments\\adc\\data\\adc.txt", "wt");
	if (!f) { printf("Could not open File!\n"); return true; }
	double t = 0.0;
	for (k=0; k<100; k++) for (i=0; i<20; i++)
	{
		int x = (data[i])[k] & 0x0fff;
		if (x & 0x0800) x |= 0xfffff000;
		fprintf(f, "%7.2f %6i\n", t, x);
		t += 1.25;
	}
	fclose(f);
*/
	return true;
}


CMD_PROC(showsda)
{
	const unsigned int nSamples = 52;
	unsigned int i, k;
	uint32_t buffersize;
	vector<uint16_t> data[20];

	tb.SignalProbeD1(9);
	tb.SignalProbeD2(17);
	tb.SignalProbeA2(PROBEA_SDA);
	tb.SignalProbeADC(PROBEA_SDA, 0);

	tb.Daq_Select_ADC(nSamples, 2, 7);
	tb.uDelay(1000);
	tb.Daq_Open(1024);
	for (i=0; i<20; i++)
	{
		tb.Sig_SetDelay(SIG_SDA, 26-i);
		tb.uDelay(10);
		tb.Daq_Start();
		tb.roc_Pix_Trim(12, 34, 5);
		tb.uDelay(1000);
		tb.Daq_Stop();
		tb.Daq_Read(data[i], buffersize, 1024);
		if (data[i].size() != nSamples)
		{
			printf("Data size %i: %i\n", i, int(data[i].size()));
			return true;
		}
	}
	tb.Daq_Close();
	tb.Flush();

	int n = 20*nSamples;
	vector<double> values(n);
	int x = 0;
	for (k=0; k<nSamples; k++) for (i=0; i<20; i++)
	{
		int y = (data[i])[k] & 0x0fff;
		if (y & 0x0800) y |= 0xfffff000;
		values[x++] = y;
	}
	Scope("SDA", values);

	return true;
}




CMD_PROC(adcena)
{
	int datasize;
	PAR_INT(datasize, 1, 2047);
	tb.Daq_Select_ADC(datasize, 1, 4, 6);
	DO_FLUSH
	return true;
}

CMD_PROC(adcdis)
{
	tb.Daq_Select_Deser160(2);
	DO_FLUSH
	return true;
}

CMD_PROC(deser)
{
	int shift;
	PAR_INT(shift,0,7);
	tb.Daq_Select_Deser160(shift);
	DO_FLUSH
	return true;
}



void decoding_show2(vector<uint16_t> &data)
{
	int size = data.size();
	if (size > 6) size = 6;
	for (int i=0; i<size; i++)
	{
		uint16_t x = data[i];
		for (int k=0; k<12; k++)
		{
			Log.puts((x & 0x0800)? "1" : "0");
			x <<= 1;
			if (k == 3 || k == 7 || k == 11) Log.puts(" ");
		}
	}
}

void decoding_show(vector<uint16_t> *data)
{
	int i;
	for (i=1; i<16; i++) if (data[i] != data[0]) break;
	decoding_show2(data[0]);
	Log.puts("  ");
	if (i<16) decoding_show2(data[i]); else Log.puts(" no diff");
	Log.puts("\n");
}

CMD_PROC(decoding)
{
	unsigned short t;
	uint32_t buffersize;
	vector<uint16_t> data[16];
	tb.Pg_SetCmd(0, PG_TOK + 0);
	tb.Daq_Select_Deser160(2);
	tb.uDelay(10);
	Log.section("decoding");
	tb.Daq_Open(100);
	for (t=0; t<44; t++)
	{
		tb.Sig_SetDelay(SIG_CLK, t);
		tb.Sig_SetDelay(SIG_TIN, t+5);
		tb.uDelay(10);
		for (int i=0; i<16; i++)
		{
			tb.Daq_Start();
			tb.Pg_Single();
			tb.uDelay(200);
			tb.Daq_Stop();
			tb.Daq_Read(data[i], buffersize, 200);
		}
		Log.printf("%3i ", int(t));
		decoding_show(data);
	}
	tb.Daq_Close();
	Log.flush();
	return true;
}



// -- Wafer Test Adapter commands ----------------------------------------
/*
CMD_PROC(vdreg)    // regulated VD
{
	double v = tb.GetVD_Reg();
	printf("\n VD_reg = %1.3fV\n", v);
	return true;
}

CMD_PROC(vdcap)    // unregulated VD for contact test
{
	double v = tb.GetVD_CAP();
	printf("\n VD_cap = %1.3fV\n", v);
	return true;
}

CMD_PROC(vdac)     // regulated VDAC
{
	double v = tb.GetVDAC_CAP();
	printf("\n V_dac = %1.3fV\n", v);
	return true;
}
*/


// =======================================================================
//  tbm commands
// =======================================================================
/*
CMD_PROC(tbmdis)
{
	tb.tbm_Enable(false);
	DO_FLUSH
	return true;
}

CMD_PROC(tbmsel)
{
	int hub, port;
	PAR_INT(hub,0,31);
	PAR_INT(port,0,3);
	tb.tbm_Enable(true);
	tb.tbm_Addr(hub,port);
	DO_FLUSH
	return true;
}

CMD_PROC(modsel)
{
	int hub;
	PAR_INT(hub,0,31);
	tb.tbm_Enable(true);
	tb.mod_Addr(hub);
	DO_FLUSH
	return true;
}

CMD_PROC(tbmset)
{
	int reg, value;
	PAR_INT(reg,0,255);
	PAR_INT(value,0,255);
	tb.tbm_Set(reg,value);

	DO_FLUSH
	return true;
}

CMD_PROC(tbmget)
{
	int reg;
	unsigned char value;
	PAR_INT(reg,0,255);
	if (tb.tbm_Get(reg,value))
	{
		printf(" reg 0x%02X = %3i (0x%02X)\n", reg, (int)value, (int)value);
	} else puts(" error\n");
	
	return true;
}

CMD_PROC(tbmgetraw)
{
	int reg;
	long value;
	PAR_INT(reg,0,255);
	if (tb.tbm_GetRaw(reg,value))
	{
		printf("value=0x%02X (Hub=%2i; Port=%i; Reg=0x%02X; inv=0x%X; stop=%c)\n",
			value & 0xff, (value>>19)&0x1f, (value>>16)&0x07,
			(value>>8)&0xff, (value>>25)&0x1f, (value&0x1000)?'1':'0');
	} else puts("error\n");
	
	return true;
}


void PrintTbmData(int reg, int value)
{
	if (value < 0)
	{
		printf("02X: ?|        |\n", reg);
		return;
	}
	printf("%02X:%02X", reg, value);
	switch (reg & 0x0e)
	{
	case 0:
		printf("|        |");
		if (value&0x40) printf("TrigOut |");   else printf("        |");
		if (value&0x20) printf("Stack rd|");   else printf("        |");
		if (value&0x20) printf("        |");   else printf("Trig In |");
		if (value&0x10) printf("Pause RO|");   else printf("        |");
		if (value&0x08) printf("Stack Fl|");   else printf("        |");
		if (value&0x02) printf("        |");   else printf("TBM Clk |");
		if (value&0x01) printf(" Full RO|\n"); else printf("Half RO |\n");
		break;

	case 2:
		if (value&0x80)                printf("| Mode:Sync       |");
		else if ((value&0xc0) == 0x80) printf("| Mode:Clear EvCtr|");
		else                           printf("| Mode:Pre-Cal    |");

		printf(" StackCount: %2i                                      |\n",
			value&0x3F);
		break;

	case 4:
		printf("| Event Number: %3i             "
			"                                        |\n", value&0xFF);
		break;

	case 6:
		if (value&0x80) printf("|        |");   else printf("|Tok Pass|");
		if (value&0x40) printf("TBM Res |");   else printf("        |");
		if (value&0x20) printf("ROC Res |");   else printf("        |");
		if (value&0x10) printf("Sync Err|");   else printf("        |");
		if (value&0x08) printf("Sync    |");   else printf("        |");
		if (value&0x04) printf("Clr TCnt|");   else printf("        |");
		if (value&0x02) printf("PreCalTr|");   else printf("        |");
		if (value&0x01) printf("Stack Fl|\n"); else printf("        |\n");
		break;

	case 8:
		printf("|        |        |        |        |        |");
		if (value&0x04) printf("Force RO|");   else printf("        |");
		if (value&0x02) printf("        |");   else printf("Tok Driv|");
		if (value&0x01) printf("        |\n"); else printf("Ana Driv|\n");
		break;
	}
}

const char regline[] =
"+--------+--------+--------+--------+--------+--------+--------+--------+";

CMD_PROC(tbmregs)
{
	const int reg[13] = 
	{
		0xE1,0xE3,0xE5,0xE7,0xE9,
		0xEB,0xED,0xEF,
		0xF1,0xF3,0xF5,0xF7,0xF9
	};
	int i;
	int value[13];
	unsigned char data;

	for (i=0; i<13; i++)
		if (tb.tbm_Get(reg[i],data)) value[i] = data; else value[i] = -1;
	printf(" reg  TBMA   TBMB\n");
	printf("TBMA %s\n", regline);
	for (i=0; i<5;  i++) PrintTbmData(reg[i],value[i]);
	printf("TBMB %s\n", regline);
	for (i=8; i<13; i++) PrintTbmData(reg[i],value[i]);
	printf("     %s\n", regline);
	for (i=5; i<8; i++)
	{
		printf("%02X:", reg[i]);
		if (value[i]>=0) printf("%02X\n", value[i]);
		else printf(" ?\n");

	}
	return true;
}

CMD_PROC(modscan)
{
	int hub;
	unsigned char data;
	printf(" hub:");
	for (hub=0; hub<32; hub++)
	{
		tb.mod_Addr(hub);
		if (tb.tbm_Get(0xe1,data)) printf(" %2i", hub);
	}
	puts("\n");
	return true;
}
*/

// =======================================================================
//  roc commands
// =======================================================================
int roclist[16] = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};


CMD_PROC(select)
{
	int rocmin, rocmax, i;
	PAR_RANGE(rocmin,rocmax, 0,15)

	for (i=0; i<rocmin;  i++) roclist[i] = 0;
	for (;    i<=rocmax; i++) roclist[i] = 1;
	for (;    i<16;      i++) roclist[i] = 0;

	tb.roc_I2cAddr(rocmin);
	return true;
}

CMD_PROC(dac)
{
	int addr, value;
	PAR_INT(addr,1,255)
	PAR_INT(value,0,255)

	for (int i=0; i<16; i++) if (roclist[i])
	{ tb.roc_I2cAddr(i); tb.roc_SetDAC(addr, value); }

	DO_FLUSH
	return true;
}

CMD_PROC(vana)
{
	int value;
	PAR_INT(value,0,255)
	for (int i=0; i<16; i++) if (roclist[i])
	{ tb.roc_I2cAddr(i); tb.roc_SetDAC(Vana, value); }

	DO_FLUSH
	return true;
}

CMD_PROC(vtrim)
{
	int value;
	PAR_INT(value,0,255)
	for (int i=0; i<16; i++) if (roclist[i])
	{ tb.roc_I2cAddr(i); tb.roc_SetDAC(Vtrim, value); }

	DO_FLUSH
	return true;
}

CMD_PROC(vthr)
{
	int value;
	PAR_INT(value,0,255)
	for (int i=0; i<16; i++) if (roclist[i])
	{ tb.roc_I2cAddr(i); tb.roc_SetDAC(VthrComp, value); }

	DO_FLUSH
	return true;
}

CMD_PROC(vcal)
{
	int value;
	PAR_INT(value,0,255)
	for (int i=0; i<16; i++) if (roclist[i])
	{ tb.roc_I2cAddr(i); tb.roc_SetDAC(Vcal, value); }

	DO_FLUSH
	return true;
}

CMD_PROC(wbc)
{
	int value;
	PAR_INT(value,0,255)
	for (int i=0; i<16; i++) if (roclist[i])
	{ tb.roc_I2cAddr(i);  tb.roc_SetDAC(WBC, value); }

	DO_FLUSH
	return true;
}

CMD_PROC(ctl)
{
	int value;
	PAR_INT(value,0,255)
	for (int i=0; i<16; i++) if (roclist[i])
	{ tb.roc_I2cAddr(i); tb.roc_SetDAC(CtrlReg, value); }

	DO_FLUSH
	return true;
}

CMD_PROC(cole)
{
	int col, colmin, colmax;
	PAR_RANGE(colmin,colmax, 0,ROC_NUMCOLS-1)
	for (int i=0; i<16; i++) if (roclist[i])
	{
		tb.roc_I2cAddr(i);
		for (col=colmin; col<=colmax; col+=2)
			tb.roc_Col_Enable(col, 1);
	}

	DO_FLUSH
	return true;
}

CMD_PROC(cold)
{
	int col, colmin, colmax;
	PAR_RANGE(colmin,colmax, 0,ROC_NUMCOLS-1)
	for (int i=0; i<16; i++) if (roclist[i])
	{
		tb.roc_I2cAddr(i);
		for (col=colmin; col<=colmax; col+=2)
			tb.roc_Col_Enable(col, 0);
	}

	DO_FLUSH
	return true;
}

CMD_PROC(pixe)
{
	int col,colmin,colmax, row,rowmin,rowmax, value;
	PAR_RANGE(colmin,colmax, 0,ROC_NUMCOLS-1)
	PAR_RANGE(rowmin,rowmax, 0,ROC_NUMROWS-1)
	PAR_INT(value,0,15)
	for (int i=0; i<16; i++) if (roclist[i])
	{
		tb.roc_I2cAddr(i);
		for(col=colmin; col<=colmax; col++) for (row=rowmin; row<=rowmax; row++)
			tb.roc_Pix_Trim(col,row, 15-value);
	}

	DO_FLUSH
	return true;
}

CMD_PROC(pixd)
{
	int col,colmin,colmax, row,rowmin,rowmax;
	PAR_RANGE(colmin,colmax, 0,ROC_NUMCOLS-1)
	PAR_RANGE(rowmin,rowmax, 0,ROC_NUMROWS-1)
	for (int i=0; i<16; i++) if (roclist[i])
	{
		tb.roc_I2cAddr(i);
		for(col=colmin; col<=colmax; col++) for (row=rowmin; row<=rowmax; row++)
			tb.roc_Pix_Mask(col,row);
	}

	DO_FLUSH
	return true;
}

CMD_PROC(cal)
{
	int col,colmin,colmax, row,rowmin,rowmax;
	PAR_RANGE(colmin,colmax, 0,ROC_NUMCOLS-1)
	PAR_RANGE(rowmin,rowmax, 0,ROC_NUMROWS-1)
	for (int i=0; i<16; i++) if (roclist[i])
	{
		tb.roc_I2cAddr(i);
		for(col=colmin; col<=colmax; col++) for (row=rowmin; row<=rowmax; row++)
			tb.roc_Pix_Cal(col,row,false);
	}

	DO_FLUSH
	return true;
}

CMD_PROC(cals)
{
	int col,colmin,colmax, row,rowmin,rowmax;
	PAR_RANGE(colmin,colmax, 0,ROC_NUMCOLS-1)
	PAR_RANGE(rowmin,rowmax, 0,ROC_NUMROWS-1)
	for (int i=0; i<16; i++) if (roclist[i])
	{
		tb.roc_I2cAddr(i);
		for(col=colmin; col<=colmax; col++) for (row=rowmin; row<=rowmax; row++)
			tb.roc_Pix_Cal(col,row,true);
	}

	DO_FLUSH
	return true;
}

CMD_PROC(cald)
{
	for (int i=0; i<16; i++) if (roclist[i])
	{
		tb.roc_I2cAddr(i);
		tb.roc_ClrCal();
	}

	DO_FLUSH
	return true;
}

CMD_PROC(mask)
{ 
	for (int i=0; i<16; i++) if (roclist[i])
	{
		tb.roc_I2cAddr(i);
		tb.roc_Chip_Mask();
	}

	DO_FLUSH
	return true;
}



// =======================================================================
//  experimential ROC test commands
// =======================================================================
/*
void PrintScale(FILE *f, int min, int max)
{
	int x;
	fputs("     |", f);
	for (x=min; x<max; x++) fprintf(f, "%i", (x/100)%10);
	fputs("\n     |", f);
	for (x=min; x<max; x++) fprintf(f, "%i", (x/10)%10);
	fputs("\n     |", f);
	for (x=min; x<max; x++) fprintf(f, "%i", x%10);
	fputs("\n-----+", f);
	for (x=min; x<max; x++) fputs("-", f);
	fputs("\n", f);
}
*/

// --- module test functions (Peter) -------------------------------------
/*
CMD_PROC(chipeff)
{
	int x, y;
	short trim[ROC_NUMROWS*ROC_NUMCOLS];
	double res[ROC_NUMROWS*ROC_NUMCOLS];

	for (x=0; x<ROC_NUMROWS*ROC_NUMCOLS; x++) trim[x] = 0;
//	tb.ChipEfficiency(9,trim,res);

	for (y=ROC_NUMROWS-1; y>=0; y--)
	{
		for (x=0; x<ROC_NUMCOLS; x++)
			Log.printf("%i", res[x*ROC_NUMROWS + y]);
		Log.puts("/n");
	}

	for (y=0; y<40; y++) printf("%i", int(res[y]));
	puts("/n");

	return true;
}
*/

// =======================================================================
//  chip/wafer test commands
// =======================================================================

/*
int chipPos = 0;

char chipPosChar[] = "ABCD";


void GetTimeStamp(char datetime[])
{
	time_t t;
	struct tm *dt;
	time(&t);
	dt = localtime(&t);
	strcpy(datetime, asctime(dt));
}
*/


// =======================================================================


void cmdHelp()
{
	if (settings.port_prober >= 0)
	{
	 fputs("\n"
	 "+-- control commands ------------------------------------------+\n"
	 "| h                  display this text                         |\n"
	 "| exit               exit commander                            |\n"
	 "+-- wafer test ------------------------------------------------+\n"
	 "| go                 start wafer test (press <cr> to stop)     |\n"
	 "| test               run chip test                             |\n"
	 "| pr <command>       send command to prober                    |\n"
	 "| sep                prober z-axis separation                  |\n"
	 "| contact            prober z-axis contact                     |\n"
	 "| first              go to first die and clear wafer map       |\n"
	 "| next               go to next die                            |\n"
	 "| goto <x> <y>       go to specifed die                        |\n"
	 "| chippos <ABCD>     move to chip A, B, C or D                 |\n"
	 "+--------------------------------------------------------------+\n",
	 stdout);
	}
	else
	{
	 fputs("\n"
	 "+-- control commands ------------------------------------------+\n"
	 "| h                  display this text                         |\n"
	 "| exit               exit commander                            |\n"
	 "+-- chip test -------------------------------------------------+\n"
	 "| test <chip id>     run chip test                             |\n"
	 "+--------------------------------------------------------------+\n",
	 stdout);
	}
}


CMD_PROC(h)
{
	cmdHelp();
	return true;
}



void cmd()
{
	// --- connection, startup commands ----------------------------------
	CMD_REG(scan,     "scan                          enumerate USB devices");
	CMD_REG(open,     "open <name>                   open connection to testboard");
	CMD_REG(close,    "close                         close connection to testboard");
	CMD_REG(welcome,  "welcome                       blinks with LEDs");
	CMD_REG(setled,   "setled                        set atb LEDs"); //give 0-15 as parameter for four LEDs
	CMD_REG(upgrade,  "upgrade <filename>            upgrade DTB");
	CMD_REG(ver,      "ver                           shows DTB software version number");
	CMD_REG(version,  "version                       shows DTB software version");
	CMD_REG(info,     "info                          shows detailed DTB info");
	CMD_REG(boardid,  "boardid                       get board id");
	CMD_REG(init,     "init                          inits the testboard");
	CMD_REG(flush,    "flush                         flushes usb buffer");
	CMD_REG(clear,    "clear                         clears usb data buffer");
	CMD_REG(hvon,     "hvon                          switch HV on");
	CMD_REG(hvoff,    "hvoff                         switch HV off");
	CMD_REG(reson,    "reson                         activate reset");
	CMD_REG(resoff,   "resoff                        deactivate reset");
	CMD_REG(status,   "status                        shows testboard status");
	CMD_REG(rocaddr,  "rocaddr                       set ROC address");


	// --- delay commands ------------------------------------------------
	CMD_REG(udelay,   "udelay <us>                   waits <us> microseconds");
	CMD_REG(mdelay,   "mdelay <ms>                   waits <ms> milliseconds");

	// --- test board commands -------------------------------------------
	CMD_REG(clk,      "clk <delay>                   clk delay");
	CMD_REG(ctr,      "ctr <delay>                   ctr delay");
	CMD_REG(sda,      "sda <delay>                   sda delay");
	CMD_REG(tin,      "tin <delay>                   tin delay");
	CMD_REG(clklvl,   "clklvl <level>                clk signal level");
	CMD_REG(ctrlvl,   "ctrlvl <level>                ctr signel level");
	CMD_REG(sdalvl,   "sdalvl <level>                sda signel level");
	CMD_REG(tinlvl,   "tinlvl <level>                tin signel level");
	CMD_REG(clkmode,  "clkmode <mode>                clk mode");
	CMD_REG(ctrmode,  "ctrmode <mode>                ctr mode");
	CMD_REG(sdamode,  "sdamode <mode>                sda mode");
	CMD_REG(tinmode,  "tinmode <mode>                tin mode");

	CMD_REG(sigoffset,"sigoffset <offset>            output signal offset");
	CMD_REG(lvds,     "lvds                          LVDS inputs");
	CMD_REG(lcds,     "lcds                          LCDS inputs");

	CMD_REG(pon,      "pon                           switch ROC power on");
	CMD_REG(poff,     "poff                          switch ROC power off");
	CMD_REG(va,       "va <mV>                       set VA in mV");
	CMD_REG(vd,       "vd <mV>                       set VD in mV");
	CMD_REG(ia,       "ia <mA>                       set IA in mA");
	CMD_REG(id,       "id <mA>                       set ID in mA");

	CMD_REG(getva,    "getva                         set VA in V");
	CMD_REG(getvd,    "getvd                         set VD in V");
	CMD_REG(getia,    "getia                         set IA in mA");
	CMD_REG(getid,    "getid                         set ID in mA");

	CMD_REG(d1,       "d1 <signal>                   assign signal to D1 output");
	CMD_REG(d2,       "d2 <signal>                   assign signal to D2 outout");
	CMD_REG(a1,       "a1 <signal>                   assign analog signal to A1 output");
	CMD_REG(a2,       "a2 <signal>                   assign analog signal to A2 outout");
	CMD_REG(probeadc, "probeadc <signal>             assign analog signal to ADC");

	CMD_REG(pgset,    "pgset <addr> <bits> <delay>   set pattern generator entry");
	CMD_REG(pgstop,   "pgstop                        stops pattern generator");
	CMD_REG(pgsingle, "pgsingle                      send single pattern");
	CMD_REG(pgtrig,   "pgtrig                        enable external pattern trigger");
	CMD_REG(pgloop,   "pgloop <period>               start patterngenerator in loop mode");

	CMD_REG(dopen,    "dopen <buffer size>           Open DAQ and allocate memory");
	CMD_REG(dclose,   "dclose                        Close DAQ");
	CMD_REG(dstart,   "dstart                        Enable DAQ");
	CMD_REG(dstop,    "dstop                         Disable DAQ");
	CMD_REG(dsize,    "dsize                         Show DAQ buffer fill state");
	CMD_REG(dread,    "dread                         Read Daq buffer and show as digital data");
	CMD_REG(dreada,   "dreada                        Read Daq buffer and show as analog data");
	CMD_REG(showclk,  "showclk                       show CLK signal");
	CMD_REG(showctr,  "showctr                       show CTR signal");
	CMD_REG(showsda,  "showsda                       show SDA signal");
	CMD_REG(takedata, "takedata                      Continous DTB readout (to stop press any key)");
	CMD_REG(takedata2,"takedata2                     Continous DTB readout and decoding");
	CMD_REG(deser,    "deser <value>                 controls deser160");

	CMD_REG(adcena,   "adcena                        enable ADC");
	CMD_REG(adcdis,   "adcdis                        disable ADC");

	// --- ROC commands --------------------------------------------------
	CMD_REG(select,   "select <addr range>           set i2c address");
	CMD_REG(dac,      "dac <address> <value>         set DAC");
	CMD_REG(vana,     "vana <value>                  set Vana");
	CMD_REG(vtrim,    "vtrim <value>                 set Vtrim");
	CMD_REG(vthr,     "vthr <value>                  set VthrComp");
	CMD_REG(vcal,     "vcal <value>                  set Vcal");
	CMD_REG(wbc,      "wbc <value>                   set WBC");
	CMD_REG(ctl,      "ctl <value>                   set control register");
	CMD_REG(cole,     "cole <range>                  enable column");
	CMD_REG(cold,     "cold <range>                  disable columns");
	CMD_REG(pixe,     "pixe <range> <range> <value>  trim pixel");
	CMD_REG(pixd,     "pixd <range> <range>          kill pixel");
	CMD_REG(cal,      "cal <range> <range>           calibrate pixel");
	CMD_REG(cals,     "cals <range> <range>          sensor calibrate pixel");
	CMD_REG(cald,     "cald                          clear calibrate");
	CMD_REG(mask,     "mask                          mask all pixel and cols");

	CMD_REG(decoding, "decoding");

	CMD_REG(h,        "h                             simple help");

	cmdHelp();
	
	cmd_intp.SetScriptPath(settings.path);

	// command loop
	while (true)
	{
		try
		{
			CMD_RUN(stdin);
			return;
		}
		catch (CRpcError e)
		{
			e.What();
		}
	}
}

