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
#include "cerebwars_lib.h"

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

#define GAME_START_DELAY 10


/*function prototypes*/
static void print_banner();
char *which_config(int argc, char **argv);
char task_running = 0x01;
char program_running = 0x01;

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
	char game_started = 0x00;
	double cpu_time_used;
	double running_avg = 0;
	double adjusted_sample[NB_PLAYERS] = {0};
	double integrated_diff = 0.5;
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
	/*setup gpios*/
	setup_gpios();
	
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
	sleep(2);
	
	while(program_running){	
			
		/*set beep mode*/
		set_beep_mode(50, 0, 500);
		
		/*wait for button pressed*/
		wait_for_start_demo();
		
		turn_off_beeper();
	
	
		printf("About to begin training\n");
		fflush(stdout);
		
		cerebral_wars_training_mode();
		
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
		pthread_create(&(threads_array[PLAYER_2]), &attr,
					   train_player, (void*)&(feature_proc[PLAYER_2]));
					   
		pthread_join(threads_array[PLAYER_1], NULL);			   
		pthread_join(threads_array[PLAYER_2], NULL);			   
		
		stop_cerebral_wars();
		
		/*little pause between training and testing*/	
		printf("About to start task\n");
		fflush(stdout);	
		sleep(3);	
			
		start = clock();
		start_cerebral_wars();
		task_running = 0x01;
			
		/*run the test*/
		while(task_running){
			
			
			if(game_started){
			
				/*get a normalized sample*/
				pthread_create(&(threads_array[PLAYER_1]), &attr,
							   get_sample, (void*)&(feature_proc[PLAYER_1]));
				pthread_create(&(threads_array[PLAYER_2]), &attr,
							   get_sample, (void*)&(feature_proc[PLAYER_2]));
							   
				pthread_join(threads_array[PLAYER_1], NULL);		
				pthread_join(threads_array[PLAYER_2], NULL);		
				
				/*show the value*/
				printf("Player1.sample: %.3f\n",feature_proc[PLAYER_1].sample);
				
				/*add little offset to allow for negative values*/
				feature_proc[PLAYER_1].sample = feature_proc[PLAYER_1].sample+0.2;
				feature_proc[PLAYER_2].sample = feature_proc[PLAYER_2].sample+0.2;
				
				/*average over recent history*/
				adjusted_sample[PLAYER_1] = (float)0.5*feature_proc[PLAYER_1].sample+0.5*adjusted_sample[PLAYER_1];
				adjusted_sample[PLAYER_2] = (float)0.5*feature_proc[PLAYER_2].sample+0.5*adjusted_sample[PLAYER_2];
				
				/*report adjusted value*/
				printf("Player1.adjsample: %.3f\n",adjusted_sample[PLAYER_1]);
				
				/*stub for tests*/
				//adjusted_sample[PLAYER_2] = adjusted_sample[PLAYER_1]/2;
				
				/*make sure that values are within the range [0,1]*/
				if(adjusted_sample[PLAYER_1]>1){
					adjusted_sample[PLAYER_1] = 1;
				}else if(adjusted_sample[PLAYER_1]<0){
					adjusted_sample[PLAYER_1] = 0;
				}
				
				if(adjusted_sample[PLAYER_2]>1){
					adjusted_sample[PLAYER_2] = 1;
				}else if(adjusted_sample[PLAYER_2]<0){
					adjusted_sample[PLAYER_2] = 0;
				}
				
				/*integrated the difference*/
				integrated_diff += (adjusted_sample[PLAYER_1]-adjusted_sample[PLAYER_2])/75;
				
				/*report the current value*/
				printf("integrated_diff: %.3f\n",integrated_diff);
				
				/*update buzzer state*/
				set_buzzer_state(running_avg);
				set_player_rate(adjusted_sample[PLAYER_1],PLAYER_1);
				set_player_rate(adjusted_sample[PLAYER_2],PLAYER_2);
				set_explosion_location(integrated_diff);
				
			}else{
				
				set_player_rate(0.5,PLAYER_1);
				set_player_rate(0.5,PLAYER_2);
				set_explosion_location(integrated_diff);
			}
			
			/*get current time*/
			end = clock();
			cpu_time_used = ((double) (end - start)) / (double)CLOCKS_PER_SEC * 100;
			
			/*check if one of the stop conditions is met*/
			if(app_config->test_duration < cpu_time_used){
				task_running = 0x00;
			}else if(GAME_START_DELAY < cpu_time_used){
				game_started = 0x01;
			}
			
		}
		
		stop_cerebral_wars();
		sleep(1);
		
		printf("Finished\n");
		
	}

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
	printf("\nCerebral Wars - EEG 2 players competitive game\n\n");
	printf("Frederic Simard (fred.simard@atlantsembedded.com)\n");
	printf("Ron Brash (ron.brash@atlantsembedded.com)\n");
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





