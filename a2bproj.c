/*****************************************************************************
 * a2bproj.c
 *****************************************************************************/

#include "adi_initialize.h"
#include "a2bproj.h"
#include "post_debug.h"
#include "post_common.h"

#include "adi_a2b_datatypes.h"
#include "adi_a2b_types.h"
#include "adi_a2b_framework.h"
#include "adi_a2b_externs.h"
#include "adi_a2b_driverprototypes.h"
#include "adiA2B.h"

#include "string.h"

void handleCodecData(unsigned int);
void initBeamforming(void);

extern volatile int inputReady;
extern volatile int inputReady1;
extern volatile int buffer_cntr;
extern volatile int recv_num;
extern unsigned char recv_array[100];

bool g_a2bMasterPLLLocked = false;

volatile bool config_mode = false;
volatile bool config_timeout = false;
volatile bool beamforming_enable = false;

// output L volume, output R volume, output filter (0-none, 4-highpass, 2-bandpass, 1-lowpass)
volatile unsigned char output_config[5][3] = {	{0,0,0},
												{0,0,0},
												{0,0,0},
												{0,0,0},
												{0,0,0},
											  };
unsigned char a2b_routing_table[3][5] = {	{0,0,0,0,0},
											{0,0,0,0,0},
											{0,0,0,0,0}
										};

unsigned char Volume_Converter(unsigned char vol)
{
	unsigned char temp;
	temp = vol * 6;
	return temp;
}

bool Check_Input_Table(unsigned char *pData)
{
	if ( (pData[2] == 0x03) && (pData[1] == 0x00) && (pData[0] == 0x10) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool Check_Output_Table(unsigned char *pData)
{
	if ( (pData[2] == 0x03) && (pData[1] == 0x00) && (pData[0] == 0x20) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool Check_Routing_Table(unsigned char *pData)
{
	if ( (pData[1] == 0x00) && (pData[0] == 0x30) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool deciper_message (unsigned char array[], int num)
{
	int i,j;
	int row, col;

	for(i = 0; i < 5; i++)
	{
		for(j = 0; j < 3; j++)
		{
			output_config[i][j] = 0;
		}
	}

	for(i = 0; i < 3; i++)
	{
		for(j = 0; j < 5; j++)
		{
			a2b_routing_table[i][j] = 0;
		}
	}

	if ((num % 3) != 0 )
	{
		return false;
	}
	else
	{
		// Get input table
		if( Check_Input_Table(&array[0]))
		{
			if (array[11] == 1)
			{
				beamforming_enable = true;
			}
			else
			{
				beamforming_enable = false;
			}
		}
		else
		{
			return false;
		}

		// Get Output table
		if( Check_Output_Table(&array[12]))
		{
			for (i = 0; i < 3; i++)
			{
				output_config[i][0] = Volume_Converter( (array[16+i*3] & 0xF0) >> 4);
				output_config[i][1] = Volume_Converter( array[16+i*3] & 0x0F);
				output_config[i][2] = array[17+i*3];
			}
		}
		else
		{
			return false;
		}

		// Get Routing table
		if( Check_Routing_Table(&array[24]))
		{
			for(i = 0; i < array[26]; i++)
			{
				row = (array[28+i*3] & 0xF0) >> 4;
				col = array[28+i*3] & 0x0F;
				a2b_routing_table[row-1][col-1] = 1;
			}
		}
		else
		{
			return false;
		}


		return true;
	}
}

void a2b_config(void)
{
	/*
	int i,j;

	for (i=0; i<4; i++)
	{
		Set_DAC_Volume(i, output_config[i][0], output_config[i][1]);
	}

	Set_HP_Volume(output_config[4][0], output_config[4][1]);
*/
	Set_HP_Volume(0, output_config[0][0], output_config[0][1]);
	Set_HP_Volume(1, output_config[1][0], output_config[1][1]);
}


int main(void)
{
    ADI_A2B_RESULT eResult = ADI_A2B_SUCCESS;
    ADI_A2B_GRAPH *pgraph;
    ADI_A2B_FRAMEWORK_HANDLER_PTR pFrameworkHandle = &oFramework;
    u32 nResult  = (u32)ADI_A2B_SUCCESS;
    u8 bFailure = 0x0u , i;
    bool check_recv = false;

    unsigned char temp;
	/**
	 * Initialize managed drivers and/or services that have been added to 
	 * the project.
	 * @return zero on success 
	 */

	adi_initComponents();
	
	/* Begin adding your custom code here */
	Init_PLL();

	clearDAIpins();
	Init_UART();
	Init_LEDs();

	Init_I2C();
    init1939viaSPI();
	init_I2S();

	DEBUG_HEADER( "A2B Master Multiplexer Test" );
	adi_a2b_TwiWrite8(0x20, 0x00, 0x00);
	DEBUG_STATEMENT("\n Delay between write and read");
	adi_a2b_TwiWrite8(0x20, 0x06, 0x20);
	DEBUG_STATEMENT("\n Delay between write and read");
	adi_a2b_TwiWrite8(0x20, 0x09, 0x60);
	DEBUG_STATEMENT("\n Delay between write and read");
	adi_a2b_TwiRead8(0x20, 0x09, &temp);
	DEBUG_PRINT("\nA2B Master Register is: 0x%x", temp);

	//I2C_Test();

    eResult = adi_a2b_FrameworkInit(pFrameworkHandle);
    pgraph  = pFrameworkHandle->pgraph;

    if (eResult == ADI_A2B_SUCCESS)
    {
        eResult = adi_a2b_EnablePeripherals(pFrameworkHandle);
    }

    if (eResult == ADI_A2B_SUCCESS)
    {
        // Validate whether the hardware A2B network is same as the schematic drawn
         eResult = adi_a2b_ValidateNetwork(pgraph,pFrameworkHandle);
    }

    initBeamforming();

    printf("Hello World Chong!");
	adi_a2b_TimerClose(0);
	adi_a2b_TimerOpen(1, NULL);
	//ClearSet_LED_Bank(nState);
	Init_I2S_Interrupt();

	while(1)
	{
		if (config_mode)
		{
			disable_sync();
			adi_a2b_TimerStart(1, 10000);

			while (config_mode && !config_timeout)
			{
				;
			}

			if (config_timeout)
			{
				printf("Wrong configuration timeout!");
				config_timeout = false;
				config_mode = false;
			}
			else
			{
				adi_a2b_TimerStop(1);
				check_recv = deciper_message (recv_array, recv_num);
				if (check_recv)
				{
					a2b_config();
				}
				else
				{
					printf("Wrong configuration parameter!");
				}
			}
			enable_sync();

			disable_sync();
			enable_sync();
		}
		else
		{
			if((inputReady == 1) && (inputReady1 == 1))
			{
				handleCodecData(buffer_cntr);
			}
		}
	}

}

