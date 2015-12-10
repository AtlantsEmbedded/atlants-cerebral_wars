/**
 * @file main.c
 * @author Frederic Simard (fred.simard@atlantsembedded.com)
 * @date 6/11/2015
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
#include "app_signal.h"
#include "feature_processing.h"
#include "ipc_status_comm.h"
#include "feature_input.h"
#include "xml.h"

#define NB_PLAYERS 2
#define PLAYER_1 0
#define PLAYER_2 0

static inline void print_banner();
inline char *which_config(int argc, char **argv);
char task_running = 0x01;


/*threads that trains and obtain a sample*/
void* train_player(void* param);
void* get_sample(void* param);

#define CONFIG_NAME "config/cerebralwars_app_config.xml"

/**
 * main(int argc, char *argv[])
 * @brief main function
 * @param argc
 * @param argv
 * @return 0 for success, -1 for error
 */
int main(int argc, char *argv[])
{	
	/*freq index*/
	double cpu_time_used;
	double running_avg = 0;
	double adjusted_sample = 0;
	clock_t start, end;
	feature_input_t feature_input[NB_PLAYERS];
	ipc_comm_t ipc_comm[NB_PLAYERS];
	feat_proc_t feature_proc[NB_PLAYERS];
	
	appconfig_t* app_config;
	
	/*Set up ctrl c signal handler*/
	(void)signal(SIGINT, ctrl_c_handler);

	/*Show program banner on stdout*/
	print_banner();
	
	/*read the xml*/
	app_config = xml_initialize(which_config(argc, argv));
	
	/*setup the cerebral wars*/
	/*
	 * 
	 * set in pairing mode
	 */
	
	/*For each player*/
	/*configure the feature input*/
	/*configure the inter-process communication channel*/
	feature_input[PLAYER_1].shm_key=7804;
	feature_input[PLAYER_1].sem_key=1234;
	init_feature_input(app_config->feature_source, &(feature_input[PLAYER_1]));
	ipc_comm[PLAYER_1].sem_key=1234;
	ipc_comm_init(&(ipc_comm[PLAYER_1]));
	
	feature_input[PLAYER_2].shm_key=4786; //to be completed
	feature_input[PLAYER_2].sem_key=1728; //to be completed
	init_feature_input(app_config->feature_source, &(feature_input[PLAYER_2]));
	ipc_comm[PLAYER_2].sem_key=1728; //to be completed
	ipc_comm_init(&(ipc_comm[PLAYER_2]));
	
	/*if required, wait for eeg hardware to be present*/
	if(app_config->eeg_hardware_required){
		if(!ipc_wait_for_harware(&(ipc_comm[PLAYER_1]))){
			exit(0);
		}
		if(!ipc_wait_for_harware(&(ipc_comm[PLAYER_2]))){
			exit(0);
		}
	}
	
	/*
	 * 
	 * reset pairing mode
	 */
	
	feature_proc[PLAYER_1].nb_train_samples = app_config->training_set_size;
	feature_proc[PLAYER_1].feature_input = &(feature_input[PLAYER_1]);
	init_feat_processing(&(feature_proc[PLAYER_1]));
	
	feature_proc[PLAYER_2].nb_train_samples = app_config->training_set_size;
	feature_proc[PLAYER_2].feature_input = &(feature_input[PLAYER_2]);
	init_feat_processing(&(feature_proc[PLAYER_2]));
	
	/*
	 * 
	 * training 
	 */
	pthread_create(&(thread[PLAYER_1]), &attr, train_player, &(feature_proc[PLAYER_1])); 
	pthread_create(&(thread[PLAYER_2]), &attr, train_player, &(feature_proc[PLAYER_2])); 
	pthread_join(&(thread[PLAYER_1]),NULL);
	pthread_join(&(thread[PLAYER_2]),NULL);
		
	printf("About to start task\n");
	fflush(stdout);	
	sleep(3);	
		
	start = clock();
		
	/*run the test*/
	while(task_running){
	
		/*get a normalized sample*/
		get_normalized_sample(&(feature_proc[PLAYER_1]));
		
		/*get a normalized sample*/
		pthread_create(&(thread[PLAYER_1]), &attr, get_sample, &(feature_proc[PLAYER_1]));
		pthread_create(&(thread[PLAYER_2]), &attr, get_sample, &(feature_proc[PLAYER_2])); 
		pthread_join(&(thread[PLAYER_1]),NULL);
		pthread_join(&(thread[PLAYER_2]),NULL);
		
		adjusted_sample[PLAYER_1] = ((float)feature_proc[PLAYER_1].sample*100/3.5);
		running_avg[PLAYER_1] += (adjusted_sample[PLAYER_1]-running_avg[PLAYER_1])/app_config->avg_kernel;
		
		adjusted_sample[PLAYER_2] = ((float)feature_proc[PLAYER_2].sample*100/3.5);
		running_avg[PLAYER_2] += (adjusted_sample[PLAYER_2]-running_avg[PLAYER_2])/app_config->avg_kernel;
		
		printf("sample value[PLAYER_1]: %i\n",(int)running_avg[PLAYER_1]);
		printf("sample value[PLAYER_2]: %i\n",(int)running_avg[PLAYER_2]);
		
		/*get current time*/
		end = clock();
		cpu_time_used = ((double) (end - start)) / (double)CLOCKS_PER_SEC * 100;
		
		/*check if one of the stop conditions is met*/
		if(app_config->test_duration < cpu_time_used){
			task_running = 0x00;
		}
		
	}
	
	printf("Finished\n");
		
	ipc_comm_cleanup(&(ipc_comm[PLAYER_1]));
	clean_up_feat_processing(&(feature_proc[PLAYER_1]));
	
	exit(0);
}


/**
 * print_banner()
 * @brief Prints app banner
 */
static inline void print_banner()
{
	printf("\nCerebral Wars - EEG tug-of-war\n\n");
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
inline char *which_config(int argc, char **argv)
{

	if (argc == 2) {
		return argv[1];
	} else {
		return CONFIG_NAME;
	}
}

/**
 * void* train_player(void* param)
 * @brief thread that trains the feature processor received as input
 * @param param, pointer to feature processor
 * @return NULL
 */
void* train_player(void* param){
	train_feat_processing((feat_proc_t*)param);	
	return NULL;
}

/**
 * void* get_sample(void* param)
 * @brief thread that gets a processed sample
 * @param param, pointer to feature processor
 * @return NULL
 */
void* get_sample(void* param){
	get_normalized_sample((feat_proc_t*)param);
}

