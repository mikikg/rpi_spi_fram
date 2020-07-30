#include <fcntl.h>				//Needed for SPI port
#include <sys/ioctl.h>			//Needed for SPI port
#include <linux/spi/spidev.h>	//Needed for SPI port
#include <unistd.h>			//Needed for SPI port
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <cstring>

int spi_cs0_fd;				//file descriptor for the SPI device
int spi_cs1_fd;				//file descriptor for the SPI device
unsigned char spi_mode;
unsigned char spi_bitsPerWord;
unsigned int spi_speed;


//FRAM commands
#define FR_WREN 6
#define FR_WRDI 4
#define FR_RDSR 5
#define FR_WRSR 1
#define FR_READ 3
#define FR_WRITE 2

#define FRAM_SIZE_KBITS 128 //16 Kbytes
#define FRAM_SIZE_WORDS (FRAM_SIZE_KBITS*1024/32) //4096 32bit Words

//***********************************
//***********************************
//********** SPI OPEN PORT **********
//***********************************
//***********************************
//spi_device	0=CS0, 1=CS1
int SpiOpenPort (int spi_device)
{
    int status_value = -1;
    int *spi_cs_fd;


    //----- SET SPI MODE -----
    //SPI_MODE_0 (0,0) 	CPOL = 0, CPHA = 0, Clock idle low, data is clocked in on rising edge, output data (change) on falling edge
    //SPI_MODE_1 (0,1) 	CPOL = 0, CPHA = 1, Clock idle low, data is clocked in on falling edge, output data (change) on rising edge
    //SPI_MODE_2 (1,0) 	CPOL = 1, CPHA = 0, Clock idle high, data is clocked in on falling edge, output data (change) on rising edge
    //SPI_MODE_3 (1,1) 	CPOL = 1, CPHA = 1, Clock idle high, data is clocked in on rising, edge output data (change) on falling edge
    spi_mode = SPI_MODE_0;

    //----- SET BITS PER WORD -----
    spi_bitsPerWord = 8;

    //----- SET SPI BUS SPEED -----
    spi_speed = 10e6;		//1000000 = 1MHz (1uS per bit)


    if (spi_device)
        spi_cs_fd = &spi_cs1_fd;
    else
        spi_cs_fd = &spi_cs0_fd;


    if (spi_device)
        *spi_cs_fd = open("/dev/spidev0.1", O_RDWR);
    else
        *spi_cs_fd = open("/dev/spidev0.0", O_RDWR);

    if (*spi_cs_fd < 0)
    {
        perror("Error - Could not open SPI device");
        exit(1);
    }

    status_value = ioctl(*spi_cs_fd, SPI_IOC_WR_MODE, &spi_mode);
    if(status_value < 0)
    {
        perror("Could not set SPIMode (WR)...ioctl fail");
        exit(1);
    }

    status_value = ioctl(*spi_cs_fd, SPI_IOC_RD_MODE, &spi_mode);
    if(status_value < 0)
    {
        perror("Could not set SPIMode (RD)...ioctl fail");
        exit(1);
    }

    status_value = ioctl(*spi_cs_fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bitsPerWord);
    if(status_value < 0)
    {
        perror("Could not set SPI bitsPerWord (WR)...ioctl fail");
        exit(1);
    }

    status_value = ioctl(*spi_cs_fd, SPI_IOC_RD_BITS_PER_WORD, &spi_bitsPerWord);
    if(status_value < 0)
    {
        perror("Could not set SPI bitsPerWord(RD)...ioctl fail");
        exit(1);
    }

    status_value = ioctl(*spi_cs_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
    if(status_value < 0)
    {
        perror("Could not set SPI speed (WR)...ioctl fail");
        exit(1);
    }

    status_value = ioctl(*spi_cs_fd, SPI_IOC_RD_MAX_SPEED_HZ, &spi_speed);
    if(status_value < 0)
    {
        perror("Could not set SPI speed (RD)...ioctl fail");
        exit(1);
    }
    return(status_value);
}



//************************************
//************************************
//********** SPI CLOSE PORT **********
//************************************
//************************************
int SpiClosePort (int spi_device)
{
    int status_value = -1;
    int *spi_cs_fd;

    if (spi_device)
        spi_cs_fd = &spi_cs1_fd;
    else
        spi_cs_fd = &spi_cs0_fd;


    status_value = close(*spi_cs_fd);
    if(status_value < 0)
    {
        perror("Error - Could not close SPI device");
        exit(1);
    }
    return(status_value);
}



//*******************************************
//*******************************************
//********** SPI WRITE & READ DATA **********
//*******************************************
//*******************************************
//SpiDevice		0 or 1
//TxData and RxData can be the same buffer (read of each byte occurs before write)
//Length		Max 511 (a C SPI limitation it seems)
//LeaveCsLow	1=Do not return CS high at end of transfer (you will be making a further call to transfer more data), 0=Set CS high when done
int SpiWriteAndRead (int SpiDevice, unsigned char *TxData, unsigned char *RxData, int Length, int LeaveCsLow)
{
    struct spi_ioc_transfer spi;
    int i = 0;
    int retVal = -1;
    int spi_cs_fd;

    if (SpiDevice)
        spi_cs_fd = spi_cs1_fd;
    else
        spi_cs_fd = spi_cs0_fd;

    spi.tx_buf = (unsigned long)TxData;		//transmit from "data"
    spi.rx_buf = (unsigned long)RxData;		//receive into "data"
    spi.len = Length;
    spi.delay_usecs = 0;
    spi.speed_hz = spi_speed;
    spi.bits_per_word = spi_bitsPerWord;
    spi.cs_change = LeaveCsLow;						//0=Set CS high after a transfer, 1=leave CS set low

    retVal = ioctl(spi_cs_fd, SPI_IOC_MESSAGE(1), &spi);

    if(retVal < 0)
    {
        perror("Error - Problem transmitting spi data..ioctl");
        exit(1);
    }

    return retVal;
}

//-------------------------------------------------
unsigned int FR_READ32(int adr) {

    unsigned char TRXdata[8];

    adr *= 4; //Words address mode

    //READ data
    TRXdata[0] = FR_READ;
    TRXdata[1] = adr >> 8;
    TRXdata[2] = adr;
    TRXdata[3] = 0;
    TRXdata[4] = 0;
    TRXdata[5] = 0;
    TRXdata[6] = 0;
    TRXdata[7] = 0;

    SpiWriteAndRead(0, TRXdata, TRXdata,7, 0);

    return (TRXdata[3+3]) | (TRXdata[2+3] << 8) | (TRXdata[1+3] << 16) | (TRXdata[0+3] << 24);
}

//-------------------------------------------------
void FR_WRITE32(int adr, unsigned int data) {

    unsigned char TRXdata[8];

    adr *= 4; //Words address mode

    //set WREN
    TRXdata[0]=FR_WREN;
    SpiWriteAndRead(0, TRXdata, TRXdata, 1, 0);

    //WRITE data
    TRXdata[0] = FR_WRITE;
    TRXdata[1] = adr >> 8; //adr hi
    TRXdata[2] = adr; //adr lo
    TRXdata[3] = data >> 24; //data hi
    TRXdata[4] = data >> 16; //data
    TRXdata[5] = data >> 8; //data
    TRXdata[6] = data; //data lo
    SpiWriteAndRead(0, TRXdata, TRXdata,7, 0);

    //clear WREN
    TRXdata[0]=FR_WRDI;
    SpiWriteAndRead(0, TRXdata, TRXdata, 1, 0);

}



int main(int argc, char **argv) {

    unsigned int mainBuffer[0xFFFF]; //64Kwords
    memset (mainBuffer, 0x00, sizeof(mainBuffer));

    SpiOpenPort(0);

    //increment @0
    FR_WRITE32(0, FR_READ32(0)+1);
    //increment @last
    FR_WRITE32(FRAM_SIZE_WORDS-1, FR_READ32(FRAM_SIZE_WORDS-1)+1);


    FR_WRITE32 (0x10, -1);
    FR_WRITE32 (0x20, -2);
    FR_WRITE32 (0x30, -3);
    FR_WRITE32 (0x40, -224535);

    int32_t testInt = 0;
    testInt = FR_READ32(0x10);

    //read into buffer
    for (int x=0; x < FRAM_SIZE_WORDS; x++) {
        mainBuffer[x] = FR_READ32(x);
    }


    //print buffer
    for (int x=0; x < FRAM_SIZE_WORDS; x++) {
        if (x%16==0)printf("\n0x%04X: ", x);
        printf("%08X ", mainBuffer[x]);

    }

    SpiClosePort(0);

    printf("\ntestInt=%d\n", testInt);
    printf("\nDone.\n");


}