/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	ComponentBase.cpp
	=================

	Provides some function definitions for virtuals that will not necessary
	be overridden

*/

#include "ComponentBase.h"
#include "ProcessPool.h"
#include "6502.h"

bool CComponentBase::Write(Uint16, Uint32, Uint8, Uint32) { return false; }
bool CComponentBase::Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32)
{
	((C6502 *)(PPPtr->GetWellDefinedComponent(COMPONENT_CPU)))->ReadMem(0xffff, Addr, Data8, Data32);
	return false;
}
bool CComponentBase::IOCtl(Uint32, void *, Uint32) { return false; }
Uint32 CComponentBase::Update(Uint32 t, bool Catchup) { return CYCLENO_ANY;}

void CComponentBase::AttachTo(CProcessPool &pool, Uint32 id)
{
	PPPtr = &pool;
	PPNum = id;
}
