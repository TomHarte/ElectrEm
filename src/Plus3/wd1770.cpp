#include "wd1770.h"
#include "../ProcessPool.h"
#include "../HostMachine/HostMachine.h"

#define ST_MOTORON		0x80
#define ST_WPROTECT		0x40
#define ST_SPINUP		0x20
#define ST_DELRECORD	0x20
#define ST_NOTFOUND		0x10
#define ST_CRCERROR		0x08
#define ST_LOSTDATA		0x04
#define ST_TRACK0		0x04
#define ST_DATAREQ		0x02
#define ST_BUSY			0x01

int CWD1770::Open(char *name, int drive)
{
	if(drive < 0 || drive > 1) return WDOPEN_FAIL;

	char *DSKExts[] = { "adf\a", "adl\a", "adm\a", "ssd\a", "dsd\a", "adf\a", "img\a", NULL };
	CDrive *NewDrive = NULL;

	if(GetHost() -> ExtensionIncluded(name, DSKExts))
	{
		NewDrive = new CDriveADF;
		if(!NewDrive->Open(name))
		{
			delete NewDrive;
			return WDOPEN_FAIL;
		}
	}

	char *FDIExts[] = { "fdi\a", NULL };

	if(GetHost() -> ExtensionIncluded(name, FDIExts))
	{
		NewDrive = new CDriveFDI;
		if(!NewDrive->Open(name))
		{
			delete NewDrive;
			return WDOPEN_FAIL;
		}
	}

	if(NewDrive)
	{
		/* ensure current disc is safely finished with! */
		ForceInterrupt = true;
		Update(1000, false);
		CurrentTime -= 1000;
		LastRun -= 1000;
		ForceInterrupt = false;

		/* switch drives */
		if(Drives[drive].Drive) delete Drives[drive].Drive;
		Drives[drive].Drive = NewDrive;
		return Drives[drive].Drive->GetLine(DL_DDENQUERY) ? WDOPEN_DDEN : WDOPEN_SDEN;
	}

	return WDOPEN_FAIL;
}

void CWD1770::Close(int drive)
{
	if(Drives[drive].Drive)
	{
		delete Drives[drive].Drive;
		Drives[drive].Drive = new CDriveEmpty;
	}
}

bool CWD1770::IOCtl(Uint32 Control, void *Parameter, Uint32 TimeStamp)
{
	switch(Control)
	{
		default:
		return CComponentBase::IOCtl(Control, Parameter, TimeStamp);

		case IOCTL_SETCONFIG:
			Drives[0].ReadOnly = ((ElectronConfiguration *)Parameter)->Plus3.Drive1WriteProtect;
			Drives[1].ReadOnly = ((ElectronConfiguration *)Parameter)->Plus3.Drive2WriteProtect;
		return false;
	}
}

CWD1770::CWD1770(ElectronConfiguration &cfg)
{
	Quit = false;
	GoSemaphore = SDL_CreateSemaphore(0);
	StopSemaphore = SDL_CreateSemaphore(0);
	MainEx = SDL_CreateThread(MainThreadHelper, this);
	LastRun = CurrentTime = 0;

	CurrentDrive = &Drives[0];
	Drives[0].Drive = new CDriveEmpty;
	Drives[1].Drive = new CDriveEmpty;

	NewCmmd = ForceInterrupt = IndexHoleInterrupt = false;
	MotorOn = IdleTime = 0;

	Drives[0].ReadOnly = cfg.Plus3.Drive1WriteProtect;
	Drives[1].ReadOnly = cfg.Plus3.Drive2WriteProtect;

	Status = 0;
}

CWD1770::~CWD1770()
{
	Quit = true;
	CyclesToRun = 1;

	SDL_SemPost(GoSemaphore);
	SDL_WaitThread(MainEx, NULL);
	SDL_DestroySemaphore(GoSemaphore);
	SDL_DestroySemaphore(StopSemaphore);

	delete Drives[0].Drive;
	delete Drives[1].Drive;
}

void CWD1770::AttachTo(CProcessPool &pool, Uint32 id)
{
	CComponentBase::AttachTo(pool, id);

	pool.ClaimTrapAddress(id, 0xfcc4, 0xffff);
	pool.ClaimTrapAddress(id, 0xfcc5, 0xffff);
	pool.ClaimTrapAddress(id, 0xfcc6, 0xffff);
	pool.ClaimTrapAddress(id, 0xfcc7, 0xffff);
	pool.ClaimTrapAddress(id, 0xfcc0, 0xffff);

	LastRun = 0;
}

bool CWD1770::Write(Uint16 Addr, Uint32 TimeStamp, Uint8 D8, Uint32 D32)
{
//	printf("WDw: %04x [%02x]\n", Addr, D8);
	UpdateTo(TimeStamp);

	switch(Addr)
	{
		/* WD1770 registers */
		case 0xfcc4:
			/* is this a force interrupt of any description? */
			if((D8 >> 4) == 13)
			{
				ForceInterrupt = (D8&0x08) ? true : false;
				IndexHoleInterrupt = (D8&0x04) ? true : false;
			}
			else
			{
				Command = D8; NewCmmd = true;
			}
		break;
		case 0xfcc5: if(!(Status&ST_BUSY)) Track = D8; break;
		case 0xfcc6: if(!(Status&ST_BUSY)) Sector = D8; break;
		case 0xfcc7: Data8 = D8; Status &= ~ST_DATAREQ; break;

		/* control register */
		case 0xfcc0: Control = D8;
			printf("%02x: %d\n", Control, Control&3);
			if(Control&0x01) CurrentDrive = &Drives[0];
			if(Control&0x02) CurrentDrive = &Drives[1];
			/* otherwise: a NULL drive... */

			CurrentDrive->Drive->SetLine(DL_SIDE, (Control&0x04) >> 2);
			CurrentDrive->Drive->SetLine(DL_DDEN, (Control&0x08)^0x08);
			DDen = ((Control&0x08)^0x08) ? true : false;
		break;
	}
	return false;
}

bool CWD1770::Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &D8, Uint32 &D32)
{
//	fprintf(stderr, "w+ [%d/%d - %d]\n", TimeStamp, LastRun, TimeStamp-LastRun); fflush(stdout);
	UpdateTo(TimeStamp);

	switch(Addr)
	{
		/* WD1770 registers */
		case 0xfcc4:
			D8 = Status;
		break;
		case 0xfcc5: D8 = Track; break;
		case 0xfcc6: D8 = Sector; break;
		case 0xfcc7: D8 = Data8; Status &= ~ST_DATAREQ; break;

		/* control register - reading should return bit 0 set for a one drive machine, bits 0 & 1 set for a two drive machine, etc */
		case 0xfcc0: D8 = 1; break;
	}

//	fprintf(stderr, "w-\n"); fflush(stdout);
//	printf("WDr: %04x [%02x]\n", Addr, D8);
	return false;
}

Uint32 CWD1770::Update(Uint32 TotalCycles, bool Catchup)
{
	CurrentTime += TotalCycles;
	return CYCLENO_ANY;
}

void CWD1770::UpdateTo(Uint32 CycleTime)
{
	CyclesToRun = CycleTime - LastRun;
	if(CyclesToRun >= IdleTime)
	{
//		fprintf(stderr, "To run: %d (to %d)\n", CyclesToRun, CycleTime); fflush(stderr);
		LastRun = CycleTime;

		/* signal main thread to continue for a bit */
//		fprintf(stderr, "[updateto] semaphores: %08x %08x\npost go\n", GoSemaphore, StopSemaphore); fflush(stderr);
		SDL_SemPost(GoSemaphore);
		/* wait for it to finish */
//		fprintf(stderr, "[updateto] wait stop\n"); fflush(stderr);
		SDL_SemWait(StopSemaphore);
//		fprintf(stderr, "[updateto] over\n"); fflush(stderr);
	}
//	else
//	{
//		if(!CyclesToRun)
//			fprintf(stderr, "zero runner requested\n"); fflush(stderr);
//	}
}

int CWD1770::MainThreadHelper(void *tptr)
{
	return ((CWD1770 *)tptr)->MainThread();
}

/* ACTUAL EMULATION FROM HERE DOWN */
#define Break(mincycles)	\
	IdleTime = mincycles;\
	SDL_SemPost(StopSemaphore);\
	SDL_SemWait(GoSemaphore);

#define WaitCycles(n)	\
if(!Quit && !ForceInterrupt)\
{\
	Drives[0].Drive->Update(n);\
	Drives[1].Drive->Update(n);\
	unsigned int nc = (n);\
	while((nc > CyclesToRun) && !Quit && !ForceInterrupt)\
	{\
		nc -= CyclesToRun;\
		Break(nc);\
	}\
	CyclesToRun -= nc;\
	if(Quit || ForceInterrupt) return;\
}

#define WaitMs(n) WaitCycles(n*2)
#define WaitBytes(n) WaitCycles(n << (DDen ? 6 : 7))

#define MOTOR_ON			0x08
#define UPDATE_TRACK		0x10
#define VERIFY				0x04
#define SETTLING_DELAY		0x04
#define MULTIPLE_SECTORS	0x10
#define WRITE_DELETED		0x01

/* TYPE I */
#define SeekCommand(v)		(((v) >> 4)==1)
#define RestoreCommand(v)	(((v) >> 4)==0)
#define StepCommand(v)		(((v) >> 5)==1)
#define StepInCommand(v)	(((v) >> 5)==2)
#define StepOutCommand(v)	(((v) >> 5)==3)

/* TYPE II */
#define WriteCommand(v)		((v)&0x20)

#define CheckMotor()\
	if((!(BCommand&MOTOR_ON)) && !MotorOn)\
	{\
		MotorOn = 9;\
		int IndexCount = 0;\
		while(!Quit && (IndexCount < 6))\
		{\
			GetSector();\
			if(CurSector.Type == DriveEvent::INDEXHOLE)\
				IndexCount++;\
		}\
	}

void CWD1770::GetSector()
{
	CurrentDrive->Drive->GetEvent(&CurSector);
	WaitCycles(CurSector.CyclesToStart);
}

void CWD1770::DoCommand()
{
	Uint8 BCommand = Command;
	switch(BCommand >> 4)
	{
		/* Type I commands */
		case 0: case 1: case 2: case 3:
		case 4: case 5: case 6: case 7:
		{
			int WaitTime;
			switch(BCommand&3)
			{
				default:
				case 0: WaitTime = 6000; break;
				case 1: WaitTime = 12000; break;
				case 2: WaitTime = 20000; break;
				case 3: WaitTime = 30000; break;
			}

			/* set busy, reset crc, seek error, drq, intrq */
			Status = (Status &~ (ST_CRCERROR | ST_NOTFOUND | ST_DATAREQ));
			WaitCycles(16);

			/* is h = 0? */
			Status &= ~0x20;
			CheckMotor();
			Status |= 0x20;

			/* if command is a step-in, set direction */
			if(StepInCommand(BCommand))
				CurrentDrive->Drive->SetLine(DL_DIRECTION, +1);

			/* if command is a step-out, reset direction */
			if(StepOutCommand(BCommand))
				CurrentDrive->Drive->SetLine(DL_DIRECTION, -1);

			/* is command a restore? */
			if(RestoreCommand(BCommand))
			{
				Data8 = 0;
				Track = 0xff;
			}

			WaitCycles(16);

			/* if a seek or restore, get looping */
			if(SeekCommand(BCommand) || RestoreCommand(BCommand))
			{
				while(1)
				{
					DataShift8 = Data8;
					if(Track == DataShift8)
						break;
					CurrentDrive->Drive->SetLine(DL_DIRECTION, (DataShift8 > Track) ? +1 : -1);

					Track += CurrentDrive->Drive->GetLine(DL_DIRECTION);

					if(CurrentDrive->Drive->GetLine(DL_TRACK0) && (CurrentDrive->Drive->GetLine(DL_DIRECTION) < 0))
					{
						Track = 0;
						break;
					}

					CurrentDrive->Drive->SetLine(DL_STEP, 1);
					WaitCycles(WaitTime);
				}
			}
			else
			{
				if(BCommand&UPDATE_TRACK)
					Track += CurrentDrive->Drive->GetLine(DL_DIRECTION);

				if(CurrentDrive->Drive->GetLine(DL_TRACK0) && (CurrentDrive->Drive->GetLine(DL_DIRECTION) < 0))
					Track = 0;
				else
				{
					CurrentDrive->Drive->SetLine(DL_STEP, 1);
					WaitCycles(WaitTime);
				}
			}

			if(BCommand&VERIFY)
			{
				int IndexCount = 0;
				while(IndexCount < 6)
				{
					GetSector();
					if(CurSector.Type == DriveEvent::SECTOR)
					{
						if(CurSector.Track == Track)
						{
							if(!CurSector.HeadCRCCorrect)
								Status |= ST_CRCERROR;
							else
								break;
						}
					}
					else
						IndexCount++;
				}

				if(IndexCount != 6)
					Status &= ~(ST_CRCERROR);
				else
					Status |= ST_NOTFOUND;

				WaitCycles(16);
			}

			/* set track 0 flag if appropriate? */
//			Status = (Status&~ST_TRACK0) | (CDrive->Drive->GetLine(DL_TRACK0) ? 0: ST_TRACK0);
		}
		break;

		/* Type II commands */
		case 8: case 9: case 10: case 11:
		{
			Status &= ~(ST_LOSTDATA | ST_NOTFOUND | 0x60);
			WaitCycles(5);

			/* is h = 0? */
			CheckMotor();

			/* check on e */
			if(BCommand&SETTLING_DELAY)
				WaitMs(30);

			/* loop: [4] */
			int IndexHoles = 0;
			while(!Quit)
			{
				if(WriteCommand(BCommand))
				{
					/* check if write protect is on */
					if(CurrentDrive->ReadOnly || CurrentDrive->Drive->GetLine(DL_WPROTECT))
					{
						Status |= ST_WPROTECT;
						return;
					}
				}

				/* [1] */
				if(IndexHoles == 5)
				{
					Status |= ST_NOTFOUND;
					return;
				}

				GetSector();
				switch(CurSector.Type)
				{
					case DriveEvent::INDEXHOLE:
						IndexHoles++;
						if(IndexHoleInterrupt) return;
					break;

					case DriveEvent::SECTOR:
						if((CurSector.Track == Track) && (CurSector.Sector == Sector))
						{
							unsigned int BytePtr = 0;

							if(CurSector.HeadCRCCorrect)
							{
								Status &= ~ST_CRCERROR;

								if(WriteCommand(BCommand))
								{
									// delay 2 bytes of gap
									WaitBytes(2);

									Status |= ST_DATAREQ;

									WaitBytes(9);

									if(Status&ST_DATAREQ)
									{
										Status |= ST_LOSTDATA;
										return;
									}

									/* combined stuff */
									WaitBytes(DDen ? 24 : 7);

									/* start CurSector filling */
									CurSector.Deleted = (BCommand&WRITE_DELETED) ? true : false;

									unsigned int WritePtr = 0;

									while(WritePtr < CurSector.DataLength)
									{
										CurSector.Data8[WritePtr] = Data8;
										WritePtr++;
										Status |= ST_DATAREQ;

										WaitBytes(1);

										if(Status&ST_DATAREQ)
										{
											Status |= ST_LOSTDATA;
											Data8 = 0;
										}
									}

									CurrentDrive->Drive->SetEventDirty();
								}
								else
								{
									if(CurSector.Deleted)
										Status |= ST_DELRECORD;
									else
										Status &= ~ST_DELRECORD;

									while(BytePtr < CurSector.DataLength)
									{
										Data8 = CurSector.Data8[BytePtr];
										BytePtr++;
										Status |= ST_DATAREQ;

										WaitCycles(CurSector.CyclesPerByte);

										if(Status&ST_DATAREQ)
											Status |= ST_LOSTDATA;
									}

									if(!CurSector.DataCRCCorrect)
									{
										Status |= ST_CRCERROR;
										return;
									}
								}
							}
							else
								Status |= ST_CRCERROR;

							/* [5] */
							if(BCommand&MULTIPLE_SECTORS)
								Sector++;
							else
								return;
						}
					break;
				}

			}
		}
		break;

		/* Type III commands */
		case 12: case 14: case 15:
		break;
	}
}

int CWD1770::MainThread()
{
	/* wait to be told to go for the first time */
	SDL_SemWait(GoSemaphore);

	while(!Quit)
	{
		if(NewCmmd)
		{
//			fprintf(stderr, "[%d] WD go %02x d:%02x s:%02x t:%02x\n", LastRun, Command, Data8, Sector, Track);
			ForceInterrupt = NewCmmd = false;
			Status |= ST_BUSY;
			DoCommand();
			Status &= ~ST_BUSY;
			CurrentDrive->Drive->SetLine(DL_INDEXHOLECOUNT, 0);
//			fprintf(stderr, "[%d] WD done [st:%02x s:%02x t:%02x]\n", LastRun, Status, Sector, Track); fflush(stderr);
		}
		else
		{
			if(MotorOn)
			{
//				fprintf(stderr, "spindle %d\n", CyclesToRun);
				Drives[0].Drive->Update(CyclesToRun);
				Drives[1].Drive->Update(CyclesToRun);
				MotorOn -= CurrentDrive->Drive->GetLine(DL_INDEXHOLECOUNT);
				if(MotorOn < 0) MotorOn = 0;
//				fprintf(stderr, "spindled\n");
			}
//			else
//				fprintf(stderr, "idle %d\n", CyclesToRun);
			Break(0);
		}
	}

	SDL_SemPost(StopSemaphore);
	return 0;
}
