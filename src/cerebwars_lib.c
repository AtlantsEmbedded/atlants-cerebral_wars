



#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "cerebwars_lib.h"


#define NB_LEDS 157   
#define PARTICLE_LENGTH 4
#define NB_COLORS 3
#define RED 0
#define GREEN 1
#define BLUE 2

#define BEGIN 0
#define END 1

#define NB_PLAYERS 2
#define PLAYER_1 0
#define PLAYER_2 0

#define RED_UPDATE_PERIOD 4
#define BLUE_UPDATE_PERIOD 2

typedef struct pixel_s{
	
	uint8_t red;
	uint8_t green;
	uint8_t blue;
}pixel_t;


const unsigned char particle_kernel[PARTICLE_LENGTH] = {0, 25, 50, 150};
const unsigned char player_mask[NB_PLAYERS][NB_COLORS] = {{1, 0, 0},
														  {0, 0, 1}};

#define EXPLOSION_SIZE 8
const unsigned char explosion_kernel[EXPLOSION_SIZE] = {0, 10, 25, 100, 100, 25, 10, 0};
const float explosion_animation_kernel[EXPLOSION_SIZE] = {0.1, 0.3, 0.5, 0.7, 0.7, 0.5, 0.3, 0.1};

void paint_explosion(pixel_t* buffer, int location);


int cerebral_wars_testbench(){
	

	/*define buffer*/
	pixel_t buffer[NB_LEDS];
	int i;
	int spi_driver;
	unsigned char particle_counter[2] = {0x00,0x00};
	static uint32_t speed = 1000000;
	int explosion_location = NB_LEDS/2;
	int red_update_counter = RED_UPDATE_PERIOD;
	int blue_update_counter = BLUE_UPDATE_PERIOD;
	
	/*configure spi driver*/
	spi_driver = open("/dev/spidev0.0",O_RDWR);
	ioctl(spi_driver, SPI_IOC_WR_MAX_SPEED_HZ, &speed);	
	
	memset(buffer,0,sizeof(pixel_t)*NB_LEDS);
	
	while(1){
		
		if(red_update_counter<=0){
			red_update_counter = RED_UPDATE_PERIOD;
			
			/*from the start to explosion*/
			for(i=explosion_location;i>=0;i--){
				buffer[i+1].red = buffer[i].red;
				buffer[i+1].green = buffer[i].green;
				buffer[i+1].blue = buffer[i].blue;
			}
			
			/*check if a particle is being placed at the beginning*/
			if(particle_counter[BEGIN]>0){
				
				buffer[0].red = player_mask[PLAYER_1][RED]*particle_kernel[particle_counter[BEGIN]];
				buffer[0].green = player_mask[PLAYER_1][GREEN]*particle_kernel[particle_counter[BEGIN]];
				buffer[0].blue = player_mask[PLAYER_1][BLUE]*particle_kernel[particle_counter[BEGIN]];
				
				particle_counter[BEGIN]--;
				
			}else{
		
				buffer[0].red = 0;
				buffer[0].green = 0;
				buffer[0].blue = 0;
				
				/*else roll a dice to determine if a new particule needs to be spawned*/
				if(((float)rand()/(float)RAND_MAX)>0.66){
					particle_counter[BEGIN] = (PARTICLE_LENGTH-1);
					
				}
			}	
		}else{
			red_update_counter--;
		}
		
		
		if(blue_update_counter<=0){
			blue_update_counter = BLUE_UPDATE_PERIOD;
			
			/*from the end to explosion*/
			/*roll back by bringing encountered values forward*/
			for(i=explosion_location;i<NB_LEDS;i++){
				buffer[i-1].red = buffer[i].red;
				buffer[i-1].green = buffer[i].green;
				buffer[i-1].blue = buffer[i].blue;
			}
			
			
			/*check if a particle is being placed at the end*/
			if(particle_counter[END]>0){
				
				buffer[NB_LEDS-1].red = player_mask[PLAYER_2][RED]*particle_kernel[particle_counter[BEGIN]];
				buffer[NB_LEDS-1].green = player_mask[PLAYER_2][GREEN]*particle_kernel[particle_counter[BEGIN]];
				buffer[NB_LEDS-1].blue = player_mask[PLAYER_2][BLUE]*particle_kernel[particle_counter[BEGIN]];
				
				particle_counter[END]--;
			}else{
		
				buffer[NB_LEDS-1].red = 0;
				buffer[NB_LEDS-1].green = 0;
				buffer[NB_LEDS-1].blue = 0;
				
				/*else roll a dice to determine if a new particule needs to be spawned*/
				if(((float)rand()/(float)RAND_MAX)>0.66){
					particle_counter[END] = (PARTICLE_LENGTH-1);
					
				}
			}	
		}else{
			blue_update_counter--;
		}
		
		/*paint the explosion*/
		paint_explosion(buffer, explosion_location);
		
		/*push it down the SPI*/
		write(spi_driver, buffer, NB_LEDS*sizeof(pixel_t));
		
		usleep(15000);	
	}
	
	
	return EXIT_SUCCESS;
}


void paint_explosion(pixel_t* buffer, int explosion_location){
	
	/*paint explosion on top*/
	for(int i=0;i<EXPLOSION_SIZE;i++){
		
		int address = explosion_location-EXPLOSION_SIZE/2 + i;
		
		if(((float)rand()/(float)RAND_MAX)>explosion_animation_kernel[i]){
			
			buffer[address].red = explosion_kernel[i];
			buffer[address].green = explosion_kernel[i];
			buffer[address].blue = explosion_kernel[i];
		}else{
			buffer[address].red = 0x00;
			buffer[address].green = 0x00;
			buffer[address].blue = 0x00;
		}
	}
}

