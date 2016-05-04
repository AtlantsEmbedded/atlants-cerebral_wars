/**
 * @file main.c
 * @author Frederic Simard (fred.simard@atlantsembedded.com)
 * @date 6/11/2015
 * @brief This program implements a simple neurofeedback loop between the occipital
 * alpha band power and an audio pitch, emitted by a piezo buzzer.
 * 
 * It starts with a short training period to set the frame of reference and then
 * the test runs for a duration that is set in the xml file.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>

#include <wiringPi.h>
#include <softTone.h>
#include <buzzer_lib.h>

#include "app_signal.h"
#include "feature_processing.h"
#include "ipc_status_comm.h"
#include "feature_input.h"
#include "xml.h"

/*defines the frequency scale*/
#define NB_STEPS 100
#define NB_PLAYERS 2
#define PLAYER_1 0
#define PLAYER_2 1

/*temp*/
#define PLAYER_1_SHM_KEY 7805
#define PLAYER_2_SHM_KEY 6712
#define PLAYER_1_SEM_KEY 1234
#define PLAYER_2_SEM_KEY 8921


/*function prototypes*/
static void print_banner();
char *which_config(int argc, char **argv);
char task_running = 0x01;

int configure_feature_input(feature_input_t* feature_input, appconfig_t* app_config);
void* train_player(void* param);
void* get_sample(void* param);

/*default xml file path/name*/
#define CONFIG_NAME "config/braintone_app_config.xml"

/**
 * main(int argc, char *argv[])
 * @brief main program function
 * @param argc
 * @param argv - optional alternative xml path/name
 * @return 0 for success, -1 for error
 */
int main(int argc, char *argv[])
{	
	/*freq index*/
	char res;
	double cpu_time_used;
	double running_avg = 0;
	double adjusted_sample = 0;
	clock_t start, end;
	feature_input_t feature_input[NB_PLAYERS];
	ipc_comm_t ipc_comm[NB_PLAYERS];
	feat_proc_t feature_proc[NB_PLAYERS];
	
	pthread_attr_t attr;
	pthread_t threads_array[NB_PLAYERS];
	
	/*configuration structure*/
	appconfig_t* app_config;
	
	/*Set up ctrl c signal handler*/
	(void)signal(SIGINT, ctrl_c_handler);

	/*Show program banner on stdout*/
	print_banner();
	
	/*read the xml*/
	app_config = xml_initialize(which_config(argc, argv));
	
	/*setup the buzzer*/
	setup_buzzer_lib(DEFAULT_PIN);
	
	/*configure the feature input*/
	if(configure_feature_input(feature_input, app_config) == EXIT_FAILURE){
		return EXIT_FAILURE;
	}
	
	/*configure the inter-process communication channel*/
	ipc_comm[PLAYER_1].sem_key=PLAYER_1_SEM_KEY;
	ipc_comm_init(&(ipc_comm[PLAYER_1]));
	
	ipc_comm[PLAYER_2].sem_key=PLAYER_2_SEM_KEY;
	ipc_comm_init(&(ipc_comm[PLAYER_2]));
	
	/*configure threads*/
	res = pthread_attr_init(&attr);
	if (res != 0)
		return EXIT_FAILURE;
	
	/*set beep mode*/
	set_beep_mode(50, 0, 500);

	/*if required, wait for eeg hardware to be present*/
	if(app_config->eeg_hardware_required){
		if(!ipc_wait_for_harware(&(ipc_comm[PLAYER_1])) || 
		   !ipc_wait_for_harware(&(ipc_comm[PLAYER_2]))){
			exit(0);
		}
	}
	
	/*stop beep mode*/
	turn_off_beeper();
	
	printf("About to begin training\n");
	fflush(stdout);
	
	/*initialize feature processing*/
	feature_proc[PLAYER_1].nb_train_samples = app_config->training_set_size;
	feature_proc[PLAYER_1].feature_input = &(feature_input[PLAYER_1]);
	init_feat_processing(&(feature_proc[PLAYER_1]));
	
	feature_proc[PLAYER_2].nb_train_samples = app_config->training_set_size;
	feature_proc[PLAYER_2].feature_input = &(feature_input[PLAYER_2]);
	init_feat_processing(&(feature_proc[PLAYER_2]));
	
		
	/*start training*/	
	pthread_create(&(threads_array[PLAYER_1]), &attr,
				   train_player, (void*)&(feature_proc[PLAYER_1]));
	pthread_create(&(threads_array[PLAYER_1]), &attr,
				   train_player, (void*)&(feature_proc[PLAYER_2]));
				   
	pthread_join(threads_array[PLAYER_1], NULL);			   
	pthread_join(threads_array[PLAYER_2], NULL);			   
	
	
	/*little pause between training and testing*/	
	printf("About to start task\n");
	fflush(stdout);	
	sleep(3);	
		
	start = clock();
		
	/*run the test*/
	while(task_running){
	
		/*get a normalized sample*/
		pthread_create(&(threads_array[PLAYER_1]), &attr,
					   get_sample, (void*)&(feature_proc[PLAYER_1]));
		pthread_create(&(threads_array[PLAYER_2]), &attr,
					   get_sample, (void*)&(feature_proc[PLAYER_2]));
					   
		pthread_join(threads_array[PLAYER_1], NULL);		
		pthread_join(threads_array[PLAYER_2], NULL);		
		
		/*adjust the sample value to the pitch scale*/
		adjusted_sample = ((float)feature_proc[PLAYER_1].sample*100/4);
		/*compute the running average, using the defined kernel*/
		running_avg += (adjusted_sample-running_avg)/app_config->avg_kernel;
		
		/*update buzzer state*/
		set_buzzer_state(running_avg);
		
		/*show sample value on console*/
		printf("sample value: %i\n",(int)running_avg);
		
		/*get current time*/
		end = clock();
		cpu_time_used = ((double) (end - start)) / (double)CLOCKS_PER_SEC * 100;
		
		/*check if one of the stop conditions is met*/
		if(app_config->test_duration < cpu_time_used){
			task_running = 0x00;
		}
		
	}
	
	printf("Finished\n");
	
	/*clean up app*/	
	ipc_comm_cleanup(&(ipc_comm[PLAYER_1]));
	clean_up_feat_processing(&(feature_proc[PLAYER_1]));
	ipc_comm_cleanup(&(ipc_comm[PLAYER_2]));
	clean_up_feat_processing(&(feature_proc[PLAYER_2]));
	
	return EXIT_SUCCESS;
}



int configure_feature_input(feature_input_t* feature_input, appconfig_t* app_config){
	
	int i = 0;
	int nb_features = 0;
	
	/*set the keys*/
	feature_input[PLAYER_1].shm_key=PLAYER_1_SHM_KEY;
	feature_input[PLAYER_1].sem_key=PLAYER_1_SEM_KEY;
	
	feature_input[PLAYER_2].shm_key=PLAYER_2_SHM_KEY;
	feature_input[PLAYER_2].sem_key=PLAYER_2_SEM_KEY;
	
	/*compute the page size from the selected features*/
	
	/*if timeseries are present*/
	if(app_config->timeseries){
		nb_features += app_config->window_width*app_config->nb_channels;
	}
	
	/*Fourier transform*/
	if(app_config->fft){
		/*one-sided fft is half window's width+1. multiplied by number of data channels*/
		nb_features += app_config->window_width/2*app_config->nb_channels; 
	}
	
	/*EEG Power bands*/
	/*alpha*/
	if(app_config->power_alpha){
		nb_features += app_config->nb_channels;
	}
	
	/*beta*/
	if(app_config->power_beta){
		nb_features += app_config->nb_channels;
	}
	
	/*gamma*/
	if(app_config->power_gamma){
		nb_features += app_config->nb_channels;
	}
	
	/*set buffer size related fields*/
	for(i=0;i<NB_PLAYERS;i++){
		feature_input[i].nb_features = nb_features;
		feature_input[i].page_size = sizeof(frame_info_t)+nb_features*sizeof(double); 
		feature_input[i].buffer_depth = app_config->buffer_depth;
		init_feature_input(app_config->feature_source, &(feature_input[i]));
	}
	

	return EXIT_SUCCESS;
}


/**
 * print_banner()
 * @brief Prints app banner
 */
static void print_banner()
{
	printf("\nBrain Tone - EEG audio Neurofeedback\n\n");
	printf("Frederic Simard (fred.simard@atlantsembedded.com)\n");
	printf("------------------------------------------\n");
}



/**
 * which_config(int argc, char **argv)
 * @brief return which config to use
 * @param argc
 * @param argv
 * @return string of config
 */
char *which_config(int argc, char **argv)
{

	if (argc == 2) {
		return argv[1];
	} else {
		return CONFIG_NAME;
	}
}


/**
 * void* train_player(void* param)
 * @brief thread that trains a player
 * @param param, (feat_proc_t*) player to train
 * @return NULL
 */
void* train_player(void* param){
	train_feat_processing((feat_proc_t*)param);
	return NULL;
}

/**
 * void* train_player(void* param)
 * @brief thread that gets a sample for a player
 * @param param, (feat_proc_t*) player to train
 * @return NULL
 */
void* get_sample(void* param){
	get_normalized_sample((feat_proc_t*)param);
	return NULL;
}





#if 0



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


#define NB_LEDS 157   
#define PARTICLE_LENGTH 4
#define RED 0
#define GREEN 1
#define BLUE 2

#define BEGIN 0
#define END 1

#define RED_UPDATE_PERIOD 4
#define BLUE_UPDATE_PERIOD 2

typedef struct pixel_s{
	
	uint8_t red;
	uint8_t green;
	uint8_t blue;
}pixel_t;


const unsigned char particle_kernel[PARTICLE_LENGTH] = {0, 25, 50, 150};
const unsigned char explosion_kernel[8] = {0, 10, 25, 100, 100, 25, 10, 0};
const float explosion_animation_kernel[8] = {0.1, 0.3, 0.5, 0.7, 0.7, 0.5, 0.3, 0.1};


/**
 * main(int argc, char **argv)
 * @brief test the mindbx_lib
 */
int main(int argc, char **argv){
	

	/*define buffer*/
	pixel_t buffer[NB_LEDS];
	int i;
	int spi_driver;
	unsigned char particle_counter[2] = {0x00,0x00};
	unsigned char particle_color[2] = {0x00,0x00};
	static uint32_t speed = 1000000;
	int explosion_location = NB_LEDS/2;
	int address;
	int red_update_counter = RED_UPDATE_PERIOD;
	int blue_update_counter = BLUE_UPDATE_PERIOD;
	
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
				
				switch(particle_color[BEGIN]){
					
					case RED:
						buffer[0].red = particle_kernel[particle_counter[BEGIN]];
						buffer[0].green = 0;
						buffer[0].blue = 0;
						break;
					case GREEN:
						buffer[0].red = 0;
						buffer[0].green = particle_kernel[particle_counter[BEGIN]];
						buffer[0].blue = 0;
						break;
					case BLUE:
						buffer[0].red = 0;
						buffer[0].green = 0;
						buffer[0].blue = particle_kernel[particle_counter[BEGIN]];
						break;
				
				}
				particle_counter[BEGIN]--;
			}else{
		
				buffer[0].red = 0;
				buffer[0].green = 0;
				buffer[0].blue = 0;
				
				/*else roll a dice to determine if a new particule needs to be spawned*/
				if(((float)rand()/(float)RAND_MAX)>0.66){
					particle_counter[BEGIN] = (PARTICLE_LENGTH-1);
					particle_color[BEGIN] = BLUE;
					
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
				
				switch(particle_color[END]){
					
					case RED:
						buffer[NB_LEDS-1].red = particle_kernel[particle_counter[END]];
						buffer[NB_LEDS-1].green = 0;
						buffer[NB_LEDS-1].blue = 0;
						break;
					case GREEN:
						buffer[NB_LEDS-1].red = 0;
						buffer[NB_LEDS-1].green = particle_kernel[particle_counter[END]];
						buffer[NB_LEDS-1].blue = 0;
						break;
					case BLUE:
						buffer[NB_LEDS-1].red = 0;
						buffer[NB_LEDS-1].green = 0;
						buffer[NB_LEDS-1].blue = particle_kernel[particle_counter[END]];
						break;
				
				}
				particle_counter[END]--;
			}else{
		
				buffer[NB_LEDS-1].red = 0;
				buffer[NB_LEDS-1].green = 0;
				buffer[NB_LEDS-1].blue = 0;
				
				/*else roll a dice to determine if a new particule needs to be spawned*/
				if(((float)rand()/(float)RAND_MAX)>0.66){
					particle_counter[END] = (PARTICLE_LENGTH-1);
					particle_color[END] = RED;
					
					//printf("New particle up!\n");
				}
			}	
		}else{
			blue_update_counter--;
		}
		
		
		/*paint explosion on top*/
		for(i=0;i<8;i++){
			
			address = explosion_location-4 + i;
			
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
		
		
		write(spi_driver, buffer, NB_LEDS*sizeof(pixel_t));
		
		usleep(15000);	
	}
	
	
	exit(0);
}


#endif 




