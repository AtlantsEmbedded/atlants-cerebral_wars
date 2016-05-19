



#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
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
#define PLAYER_2 1


#define UPDATE_PERIOD_MIN 2
#define UPDATE_PERIOD_SPAN 6
#define DEFAULT_UPDATE_PERIOD 9

typedef struct pixel_s{
	
	uint8_t red;
	uint8_t green;
	uint8_t blue;
}pixel_t;

pixel_t BLACK_PIXEL = {0,0,0};
const unsigned char particle_kernel[PARTICLE_LENGTH] = {0, 15, 30, 255};
const unsigned char player_mask[NB_PLAYERS][NB_COLORS] = {{1, 0, 0},
														  {0, 0, 1}};
#define EXPLOSION_SIZE 8
const unsigned char explosion_kernel[EXPLOSION_SIZE] = {15, 30, 75, 150, 150, 75, 30, 15};
const float explosion_animation_kernel[EXPLOSION_SIZE] = {0.1, 0.3, 0.5, 0.7, 0.7, 0.5, 0.3, 0.1};

void copy_pixel(pixel_t* dest, pixel_t* src);
void copy_explosion_pixel(pixel_t* dest, int intensity);
void paint_explosion(pixel_t* buffer);
char is_exploding(pixel_t* buffer, int explosion_location);

void* cereb_strip_loop(void* param);
void* cereb_train_loop(void* param);

double player_rate[NB_PLAYERS] = {0.5,0.5};
int player_period[NB_PLAYERS] = {DEFAULT_UPDATE_PERIOD,DEFAULT_UPDATE_PERIOD};
int explosion_location = NB_LEDS/2;
char alive = 0x01;

/**
 * int cerebral_wars_winner_mode()
 * @brief starts cerebral wars in normal mode
 */
int start_cerebral_wars(){
	
	pthread_t cereb_loop;
	pthread_attr_t attr;
	char res;

	/*configure threads*/
	res = pthread_attr_init(&attr);
	if (res != 0)
		return EXIT_FAILURE;
	
	alive = 0x01;
	
	/*configure threads*/
	pthread_create(&cereb_loop, &attr,
				   cereb_strip_loop, NULL);
	
	return EXIT_SUCCESS;
}


/**
 * int cerebral_wars_winner_mode()
 * @brief starts cerebral wars in training mode
 */
int cerebral_wars_training_mode(){
	
	pthread_t cereb_loop;
	pthread_attr_t attr;
	char res;

	/*configure threads*/
	res = pthread_attr_init(&attr);
	if (res != 0)
		return EXIT_FAILURE;
	
	alive = 0x01;
	
	/*configure threads*/
	pthread_create(&cereb_loop, &attr,
				   cereb_train_loop, NULL);
	
	return EXIT_SUCCESS;
}

/**
 * int cerebral_wars_winner_mode()
 * @brief starts cerebral wars in winner mode
 */
int cerebral_wars_winner_mode(){
	
	pthread_t cereb_loop;
	pthread_attr_t attr;
	char res;

	/*configure threads*/
	res = pthread_attr_init(&attr);
	if (res != 0)
		return EXIT_FAILURE;
	
	alive = 0x01;
	
	/*configure threads*/
	pthread_create(&cereb_loop, &attr,
				   cereb_train_loop, NULL);
	
	return EXIT_SUCCESS;
	
}

/**
 * void stop_cerebral_wars()
 * @brief Kills any on-going cerebral wars thread
 */
void stop_cerebral_wars(){
	/*stop currently running thread*/
	alive = 0x00;
}


void set_player_rate(double rate, int player){
	player_period[player] = round((1-rate)*UPDATE_PERIOD_SPAN)+UPDATE_PERIOD_MIN;
	
}

void set_explosion_location(double relative_position){
	explosion_location = (NB_LEDS*relative_position);
}

/**
 * void paint_explosion(pixel_t* buffer)
 * @brief Paint the explosion over the LED strip, overwriting particles
 * @param buffer, LED strip
 */
void paint_explosion(pixel_t* buffer){
	
	int i=0;
	int cur_pix=0;
	
	/*paint explosion on top*/
	for(i=0;i<EXPLOSION_SIZE;i++){
		
		/*compute pixel location*/
		cur_pix = explosion_location-EXPLOSION_SIZE/2 + i;
		
		/*make sure inside LED strip*/
		if(cur_pix>=0 && cur_pix<NB_LEDS){
			/*check if pixel is painted*/
			if(((float)rand()/(float)RAND_MAX)>explosion_animation_kernel[i]){
				copy_explosion_pixel(&(buffer[cur_pix]), explosion_kernel[i]);
			}else{
				copy_pixel(&(buffer[cur_pix]), &(BLACK_PIXEL));
			}
		}
	}
}


/**
 * char is_exploding(pixel_t* buffer, int explosion_location)
 * @brief Check if the LED strip is exploding. For this at least one particle must
 *        be in range of the explosion site
 * @param buffer, LED strip
 * @param explosion_location, explosion location in LED strip
 * @return 0x00, if exploding, 0x01 otherwise 
 */
char is_exploding(pixel_t* buffer, int explosion_location){

	int i=0;
	char exploding = 0x00;
	
	/*detect particles in explosion range*/
	for(i=0;i<EXPLOSION_SIZE/2;i++){
		if(explosion_location+i>=0 && explosion_location+i < NB_LEDS){
			if(buffer[explosion_location+i].red || buffer[explosion_location+i].blue){
				exploding=0x01;
			}
		}
		
		if(explosion_location-i>=0 && explosion_location-i < NB_LEDS){
			if(buffer[explosion_location-i].red || buffer[explosion_location-i].blue){
				exploding=0x01;
			}
		}
	}
	
	return exploding;
}

/**
 * void copy_pixel(pixel_t* dest, pixel_t* src)
 * @brief Copy a pixel into another
 * @param dest, destination pixel
 * @param src, source pixel
 */
void copy_pixel(pixel_t* dest, pixel_t* src){
	dest->red = src->red;
	dest->green = src->green;
	dest->blue = src->blue;
}


void copy_particle_pixel(pixel_t* dest, int player, int particle_counter){

	/*set pixel at player color*/
	dest->red = player_mask[player][RED]*particle_kernel[particle_counter];
	dest->green = player_mask[player][GREEN]*particle_kernel[particle_counter];
	dest->blue = player_mask[player][BLUE]*particle_kernel[particle_counter];
}

void copy_explosion_pixel(pixel_t* dest, int intensity){

	/*set pixel at player color*/
	dest->red = intensity;
	dest->green = intensity;
	dest->blue = intensity;
}

/**
 * void* cereb_strip_loop
 * @brief Thread that runs cerebral wars in loop
 * @param unused
 */
void* cereb_strip_loop(void* param __attribute__((unused))){
	
	/*define buffer*/
	pixel_t buffer[NB_LEDS];
	int i;
	int spi_driver;
	unsigned char particle_counter[2] = {0x00,0x00};
	static uint32_t speed = 1000000;
	int red_update_counter = 0;
	int blue_update_counter = 0;
	int res=0;
	
	/*configure spi driver*/
	spi_driver = open("/dev/spidev0.0",O_RDWR);
	ioctl(spi_driver, SPI_IOC_WR_MAX_SPEED_HZ, &speed);	
	
	/*initialise LED_strip with 0's*/
	memset(buffer,0,sizeof(pixel_t)*NB_LEDS);
	
	/*loop while alive*/
	while(alive){
		
		/************************/
		/* RED PLAYER SIDE      */
		/************************/
		if(red_update_counter<=0){
			red_update_counter = player_period[PLAYER_1];
			
			/*from the start to explosion*/
			for(i=explosion_location;i>=0;i--){
				copy_pixel(&(buffer[i+1]),&(buffer[i]));
			}
			
			/*check if a particle is being placed at the beginning*/
			if(particle_counter[BEGIN]>0){
				
				/*set pixel at player color*/
				copy_particle_pixel(&(buffer[0]), PLAYER_1, particle_counter[BEGIN]);
				
				/*update particle counter*/
				particle_counter[BEGIN]--;
				
			}else{
		
				/*set pixel black*/
				copy_pixel(&(buffer[0]),&(BLACK_PIXEL));
				
				/*else roll a dice to determine if a new particule needs to be spawned*/
				if(((float)rand()/(float)RAND_MAX)>player_rate[PLAYER_1]){
					particle_counter[BEGIN] = (PARTICLE_LENGTH-1);
					
				}
			}	
		}else{
			red_update_counter--;
		}
		
		/************************/
		/* BLUE PLAYER SIDE     */
		/************************/
		if(blue_update_counter<=0){
			blue_update_counter = player_period[PLAYER_2];
			
			/*from the end to explosion*/
			/*roll back by bringing encountered values forward*/
			for(i=explosion_location;i<NB_LEDS;i++){
				copy_pixel(&(buffer[i-1]),&(buffer[i]));
			}
			
			/*check if a particle is being placed at the end*/
			if(particle_counter[END]>0){
				
				/*set pixel at player color*/
				copy_particle_pixel(&(buffer[NB_LEDS-1]), PLAYER_2, particle_counter[END]);
				
				/*update particle counter*/
				particle_counter[END]--;
			}else{
		
				/*set pixel black*/
				copy_pixel(&(buffer[NB_LEDS-1]),&(BLACK_PIXEL));
				
				/*else roll a dice to determine if a new particule needs to be spawned*/
				if(((float)rand()/(float)RAND_MAX)>player_rate[PLAYER_2]){
					particle_counter[END] = (PARTICLE_LENGTH-1);
					
				}
			}	
		}else{
			blue_update_counter--;
		}
		
		/*paint the explosion, if it's exploding*/
		if(is_exploding(buffer, explosion_location))
			paint_explosion(buffer);
			
		/*push it down the SPI*/
		res = write(spi_driver, buffer, NB_LEDS*sizeof(pixel_t));
		
		if(res<0){
			perror("SPI write failed");
			fflush(stdout);
			exit(1);
		}
		
		usleep(5000);	
	}
	
	/*Turn off the LED strip*/
	memset(buffer,0,sizeof(pixel_t)*NB_LEDS);
	res = write(spi_driver, buffer, NB_LEDS*sizeof(pixel_t));
	
	if(res<0){
		perror("SPI write failed");
		fflush(stdout);
		exit(1);
	}
	
	
	usleep(1500);	
	
	close(spi_driver);
	return NULL;
}



void* cereb_train_loop(void* param __attribute__((unused))){
	
	
	/*define buffer*/
	pixel_t buffer[NB_LEDS];
	int i;
	int spi_driver;
	unsigned char particle_counter[2] = {0x00,0x00};
	static uint32_t speed = 1000000;
	int red_update_counter = DEFAULT_UPDATE_PERIOD;
	int blue_update_counter = DEFAULT_UPDATE_PERIOD;
	int res;
	
	/*configure spi driver*/
	spi_driver = open("/dev/spidev0.0",O_RDWR);
	ioctl(spi_driver, SPI_IOC_WR_MAX_SPEED_HZ, &speed);	
	
	memset(buffer,0,sizeof(pixel_t)*NB_LEDS);
	
	while(alive){
		
		/************************/
		/* RED PLAYER SIDE      */
		/************************/
		if(red_update_counter<=0){
			red_update_counter = DEFAULT_UPDATE_PERIOD;
			
			/*from the start to explosion*/
			for(i=NB_LEDS-2;i>=explosion_location;i--){
				copy_pixel(&(buffer[i+1]),&(buffer[i]));
			}
			
			/*check if a particle is being placed at the beginning*/
			if(particle_counter[END]>0){
				
				buffer[explosion_location+1].red = 0;
				buffer[explosion_location+1].green = particle_kernel[particle_counter[END]];
				buffer[explosion_location+1].blue = 0;
				
				particle_counter[END]--;
				
			}else{
		
				copy_pixel(&(buffer[0]),&(BLACK_PIXEL));
				
				/*else roll a dice to determine if a new particule needs to be spawned*/
				if(((float)rand()/(float)RAND_MAX)>0.66){
					particle_counter[END] = (PARTICLE_LENGTH-1);
					
				}
			}	
		}else{
			red_update_counter--;
		}
		
		/************************/
		/* BLUE PLAYER SIDE     */
		/************************/
		if(blue_update_counter<=0){
			blue_update_counter = DEFAULT_UPDATE_PERIOD;
			
			/*from the end to explosion*/
			/*roll back by bringing encountered values forward*/
			for(i=1;i<explosion_location;i++){
				copy_pixel(&(buffer[i-1]),&(buffer[i]));
			}
			
			/*check if a particle is being placed at the end*/
			if(particle_counter[BEGIN]>0){
				
				buffer[explosion_location-1].red = 0;
				buffer[explosion_location-1].green = particle_kernel[particle_counter[BEGIN]];
				buffer[explosion_location-1].blue = 0;
				
				particle_counter[BEGIN]--;
			}else{
		
				copy_pixel(&(buffer[0]),&(BLACK_PIXEL));
				
				/*else roll a dice to determine if a new particule needs to be spawned*/
				if(((float)rand()/(float)RAND_MAX)> 0.66){
					particle_counter[BEGIN] = (PARTICLE_LENGTH-1);
				}
			}	
		}else{
			blue_update_counter--;
		}
		
		/*push it down the SPI*/
		res = write(spi_driver, buffer, NB_LEDS*sizeof(pixel_t));
		
		if(res<0){
			perror("SPI write failed");
			fflush(stdout);
			exit(1);
		}
		
		
		usleep(5000);	
	}
	
	/*Turn off the LED strip*/
	memset(buffer,0,sizeof(pixel_t)*NB_LEDS);
	res = write(spi_driver, buffer, NB_LEDS*sizeof(pixel_t));

	if(res<0){
		perror("SPI write failed");
		fflush(stdout);
		exit(1);
	}
	
	usleep(1500);	
	
	close(spi_driver);
	
	return NULL;
	
}





