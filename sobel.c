#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#define MAX_DIGITS 	6

int core[3][3] = {	{ -1, -2, -1 },
					{  0,  0,  0 },
					{  1,  2,  1 }};

float intensity[3]	=	{0.2145, 0.7132, 0.0765};	

u_char** pic = NULL;
int** res = NULL;

u_char brightness(unsigned char r, unsigned char g, unsigned char b) {
	return r * intensity[0] + g * intensity[1] + b * intensity[2];
}

void clear(int fd_in, int fd_out, int w) {
    close(fd_in);
    close(fd_out);
    
    if (pic != NULL) {
        for (int i = 0; i < w; i++)
            free(pic[w]);
        free(pic);
        pic = NULL;
    }

    if (res != NULL){
        for (int i = 0; i < w; i++)
            free(res[w]);
        free(res);
        res = NULL;
    }
    
}

int convolution(unsigned char** arr, char direction, int x, int y) {
	int result = 0;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			if (direction == 0)
				result += core[i][j] * arr[x+i-1][y+i-1];
			else
				result += core[j][i] * arr[x+i-1][y+i-1];
	return result;
}

//Args: lehght, width, size strip and number thread
void* Sobel_filter(void* args) {
	int max = 0;
	int w = ((int*)args)[0];
	int h = ((int*)args)[1];
	int strip = ((int*)args)[2];
	int thread_n = ((int*)args)[3];
	for (int i = thread_n * strip; i < (thread_n + 1) * strip; i++) {
		int sobel = 0;		
		int x = i % w;
		int y = i / w;	
		if (x == 0 || x == w-1 || y == 0 || y == h-1)
			sobel = 0;
		else {
			int gy = convolution(pic, 0, x, y);
			int gx = convolution(pic, 1, x, y);
			sobel = sqrt(gx*gx + gy*gy);
			if (sobel > max)
				max = sobel;
		}
		res[x][y] = sobel;
	}
	int* res = malloc(sizeof(int));
		*res = max;
	pthread_exit(res);
}

int main(int argc, char* argv[]) {
    int w = 0, h = 0;
    int thread_counter = 1;//default threads 1
    int in_file, out_file;//src and res image
    
    if (argc < 3) {
        printf("Not enough arguments.\n");
        exit(1);
    }

    if (argc > 3) {
        thread_counter = atoi(argv[3]);
        if (thread_counter < 1){
            printf("Slishkom malo threads.\n");
            exit(1);
        }
    }

    if ((in_file = open(argv[1], O_RDONLY)) < 0) {
        printf("ERROR open src image.\n");
        exit(1);
    }

    if ((out_file = open(argv[2], O_CREAT | O_WRONLY, 0666)) < 0) {
        printf("ERROR create result file.\n");
        exit(1);
    }

    char buf[MAX_DIGITS];
	char temp;
	buf[0] = '\0';
	buf[1] = '\0';
	if (read(in_file, buf, 3) != 3 || buf[0] != 'P' || buf[1] != '6') {
		printf("Work only PNM!\n");
		clear(in_file, out_file, w);
		exit(1);
	}

	if (write(out_file, "P6\n", 3) != 3) {
		clear(in_file, out_file, w);
		printf("Write error.\n");
        exit(1);
	}

	int i;
	for (i = 0; i < MAX_DIGITS; i++) {
		if (read(in_file, &temp, 1) != 1) {
			clear(in_file, out_file, w);
			printf("Read error.\n");
        	exit(1);
		}
		if (i == 0 && temp == '#') {
			while (temp != '\n') {
				if (read(in_file, &temp, 1) != 1) {
		            clear(in_file, out_file, w);
		            printf("Read error.\n");
                    exit(1);
				}
			}
			i = -1;
			continue;
		}
		if (temp < '0' || temp > '9'){
			buf[i] = '\0';
			break;
		}
		else 
			buf[i] = temp;
	}
	if (i == MAX_DIGITS) {
		clear(in_file, out_file, w);
		printf("File is too big. Change MAX_DIGITS to work.\n");
		exit(1);
	}

	if (write(out_file, &buf, strlen(buf)) != strlen(buf)) {
		clear(in_file, out_file, w);
		printf("Write error.\n");
        exit(1);
	}

	if (write(out_file, " ", 1) != 1) {
	    clear(in_file, out_file, w);
		printf("Write error.\n");
        exit(1);
	}

	w = atoi(buf);
	if (w <= 0) {
		printf("Incorrect file width value.\n");
		clear(in_file, out_file, w);
		exit(1);
	}
	for (i = 0; i < MAX_DIGITS; i++) {
		if (read(in_file, &temp, 1) != 1) {
	        clear(in_file, out_file, w);
		    printf("Read error.\n");
            exit(1);
		}
		if (temp < '0' || temp > '9') {
			buf[i] = '\0';
			break;
		}
		else 
			buf[i] = temp;
	}

	if (i == MAX_DIGITS) {
	    clear(in_file, out_file, w);
		printf("File is too big. Change MAX_DIGITS to work.\n");
		exit(1);
	}

	if (write(out_file, &buf, strlen(buf)) != strlen(buf)) {
	    clear(in_file, out_file, w);
		printf("Write error.\n");
        exit(1);
	}

	if (write(out_file, "\n", 1) != 1) {
	    clear(in_file, out_file, w);
		printf("Write error.\n");
        exit(1);
	}

    h = atoi(buf);
    if (h <= 0) {
        clear(in_file, out_file, w);
        printf("Bad height of file.\n");
        exit(1);
    }

    if ((pic = malloc(w * sizeof(char*)))== NULL || (res = malloc(w * sizeof(int*)))== NULL) {
        clear(in_file, out_file, w);
        printf("ERROR selection memory.\n");
        exit(1);
    }
    
    for (int i = 0; i < w; i++) {
        if ((pic = malloc(w * sizeof(char*))) == NULL || (res = malloc(w * sizeof(int*))) == NULL) {
            clear(in_file, out_file, w);
            printf("ERROR selection memory.\n");
            exit(1);
        }
    }

    for (int i = 0; i < w; i++) {
        if ((pic[i] = malloc(h * sizeof(char))) == NULL || (res[i] = malloc(h * sizeof(int))) == NULL) {
            for (int j = 0; j < i; j++)	{
				free(pic[j]);
				free(res[j]);
			}
			(pic[i] != NULL)? free(pic[i]) : free(res[i]);
			free(res);
			free(pic);
            printf("ERROR selection memory.\n");
			exit(1);
        }
    }

    printf("%ix%i\n", w, h);//resolution image
	if (read(in_file, buf, 4) != 4){
		clear(in_file, out_file, w);
		printf("Read error\n");
        exit(1);
	}

	buf[3] = '\0';
	if (atoi(buf) != 255){
	    clear(in_file, out_file, w);
		printf("File error.\n");
        exit(1);
	}

	if (write(out_file, "255\n", 4) != 4) {
	    clear(in_file, out_file, w);
		printf("Write error\n");
        exit(1);
	}

	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			unsigned char r, g, b;
			r = 0;
			g = 0;
			b = 0;
			if (read(in_file, &r, 1) != 1 || read(in_file, &g, 1) != 1 || read(in_file, &b, 1) != 1) {
	            clear(in_file, out_file, w);
		        printf("Read error.\n");
                exit(1);			
            }
			pic[j][i] = brightness(r, g, b);	//convert to black-white
		}
	}

 	printf("Number of threads: %i\n", thread_counter);
	pthread_t* threads;
	int** args;
	int strip = 0;
	int max = 0;
	if (thread_counter > 1) {//prepare for threads
		
		strip = w * h / thread_counter;
				
		if ((threads = malloc((thread_counter - 1) * sizeof(pthread_t))) == NULL || 
			(args 	 = malloc((thread_counter - 1) * sizeof(int*))) 	 == NULL) {
	        clear(in_file, out_file, w);
			(threads == NULL)? free(args) : free(threads);
			printf("ERROR selection memory.\n");
			exit(1);
		}
		for (int i = 0; i < (thread_counter - 1); i++) {
			if ((args[i] = malloc (4 * sizeof(int))) == NULL) {
				free(args);
				free(threads);
	        	clear(in_file, out_file, w);
				printf("ERROR selection memory.\n");
				exit(1);
			}
			args[i][0] = w;
			args[i][1] = h;
			args[i][2] = strip;
			args[i][3] = i;
		}
	}
	
	clockid_t timer = CLOCK_REALTIME; //create and start timer
	struct timespec time;
	struct timespec st_time;
	clock_getres(timer, &time);
	time.tv_nsec = 0;
	time.tv_sec = 0;	
	clock_gettime(timer, &st_time);	
	
	int* return_max; 
	if (thread_counter > 1){//start threads
		for (int i = 0; i < (thread_counter - 1); i++) {
			if (pthread_create(threads + i, NULL, Sobel_filter, (void*)args[i]) != 0) {
	        	clear(in_file, out_file, w);
					for (int j = 0; j < i; j++) {
						pthread_cancel(threads[j]);
						free(args[j]);
					}
				free(threads);
				free(args);
				printf("ERROR create threads.\n");
				exit(1);
			}
		}
	}

	for (int i = (thread_counter - 1) * strip; i < w * h; i++) {
		int sobel = 0;		
		int x = i % w;
		int y = i / w;	
		if (x == 0 || x == w-1 || y == 0 || y == h-1)
			sobel = 0;
		else {
			int gy = convolution(pic, 0, x, y);
			int gx = convolution(pic, 1, x, y);
			long catets = pow(gx, 2) + pow(gy, 2);
			sobel = sqrt(catets);
			if (sobel > max)
				max = sobel;
		}	
		res[x][y] = sobel;
	}

	if (thread_counter > 1) { //join threads
		for (int i = 0; i < (thread_counter - 1); i++) {
			if (pthread_join(threads[i], (void*) &return_max) != 0) {
	        	clear(in_file, out_file, w);
				for (int j = 0; j < (thread_counter - 1); j++) {
					if (j > i)
						pthread_cancel(threads[j]);
					free(args[j]);
				}
				free(threads);			
				free(args);
				printf("ERROR join threads.\n");
				exit(1);
			}
			if (*return_max > max)
				max = *return_max;
		}
	}
	
	clock_gettime(timer, &time);
	double nano = time.tv_sec * 1000000000 - st_time.tv_sec * 1000000000 + time.tv_nsec - st_time.tv_nsec;
	printf("Passed %g ns.\n", nano);
	
	if (thread_counter > 1) {
		for (int i = 0; i < (thread_counter - 1); i++)
			free(args[i]);
		free(args);
		free(threads);
	}
	
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++){
			u_char normal = ((double) res[x][y] / (double) max) * 255;
			for (int k = 0; k < 3; k++){
				if (write(out_file, &normal, 1) != 1) {
	            	clear(in_file, out_file, w);
		        	printf("Write error.\n");
                	exit(1);			
				}
			}		
		}
	}
	
	clear(in_file, out_file, w);
    return 0;
}