/*
	This file is part of FreeIntv.

	FreeIntv is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	FreeIntv is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FreeIntv.  If not, see http://www.gnu.org/licenses/
*/
#include <stdio.h>
#include <stdlib.h>
#include "intv.h"
#include "memory.h"
#include "cp1610.h"
#include "stic.h"
#include "psg.h"
#include "controller.h"
#include "cart.h"
#include "osd.h"

int SR1;

int exec(void);

void LoadGame(const char* path) // load cart rom //
{
	if(LoadCart(path))
	{
		OSD_drawText(3, 3, "LOAD CART: OKAY");
	}
	else
	{
		OSD_drawText(3, 3, "LOAD CART: FAIL");
	}
}

void loadExec(const char* path)
{
	// EXEC lives at 0x1000-0x1FFF
	int i;
	unsigned char word[2];
	FILE *fp;
	if((fp = fopen(path,"rb"))!=NULL)
	{
		for(i=0x1000; i<=0x1FFF; i++)
		{
			fread(word,sizeof(word),1,fp);
			Memory[i] = (word[0]<<8) | word[1];
		}

		fclose(fp);
		OSD_drawText(3, 1, "LOAD EXEC: OKAY");
		printf("[INFO] [FREEINTV] Succeeded loading Executive BIOS from: %s\n", path);		
	}
	else
	{
		OSD_drawText(3, 1, "LOAD EXEC: FAIL");
		printf("[ERROR] [FREEINTV] Failed loading Executive BIOS from: %s\n", path);
	}
}

void loadGrom(const char* path)
{
	// GROM lives at 0x3000-0x37FF
	int i;
	unsigned char word[1];
	FILE *fp;
	if((fp = fopen(path,"rb"))!=NULL)
	{
		for(i=0x3000; i<=0x37FF; i++)
		{
			fread(word,sizeof(word),1,fp);
			Memory[i] = word[0];
		}

		fclose(fp);
		OSD_drawText(3, 2, "LOAD GROM: OKAY");
		printf("[INFO] [FREEINTV] Succeeded loading Graphics BIOS from: %s\n", path);
		
	}
	else
	{
		OSD_drawText(3, 2, "LOAD GROM: FAIL");
		printf("[ERROR] [FREEINTV] Failed loading Graphics BIOS from: %s\n", path);
	}
}

void Reset()
{
	SR1 = 0;
	CP1610Reset();
	MemoryInit();
	STICReset();
	PSGInit();
}

void Init()
{
	CP1610Init();
	MemoryInit();
}

void Run()
{
	// run for one frame 
	// exec will call drawFrame for us only when needed
	while(exec()) { }
}

int exec(void) // Run one instruction 
{
	int ticks = CP1610Tick(0); // Tick CP-1610 CPU, runs one instruction, returns used cycles
	Cycles = Cycles + ticks; 

	if(ticks==0)
	{
		// Halt Instruction found! //
		printf("\n\n[ERROR] [FREEINTV] HALT! at %i\n", Cycles);
		exit(0);
		return 0;
	}

	// Tick PSG
	PSGTick(ticks);

	if(Cycles>=14934) // STIC generates an interput every 14934 cycles
	{
		Cycles = Cycles - 14934;
		SR1 = 2907 - Cycles; // hold  SR1 output low for 2901 cycles
		DisplayEnabled = 0;
		VBlank1 = 2900 - Cycles;
	}
	if(SR1>0) 
	{
		SR1 = SR1 - ticks;
		if(SR1<0) { SR1 = 0; }
	}
	if(VBlank1>0) 
	{
		VBlank1 = VBlank1 - ticks;
		if(VBlank1<0)
		{
			VBlank2 = 3796 + VBlank1;
			VBlank1 = 0;
		}
	}
	if(VBlank2>0)
	{
		VBlank2 = VBlank2 - ticks;
		if(VBlank2<=0)
		{
			VBlank2 = 0;
			if(DisplayEnabled==1)
			{
				//STIC steals cycles on busreq-- 57 + 110*12 + (44 when vertical delay = 0)
				Cycles += 1377;
				PSGTick(1377);
				if(VerticalDelay==0)
				{
					Cycles += 44;
					PSGTick(44);
				}
				
				// Render Frame //
				STICDrawFrame();
			}
			return 0;
		}
	}
	return 1;
}
