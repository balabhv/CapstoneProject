/* Includes */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
/* Define constants. */
#define MAX_CHAR 1023                           /* Number of characters in buffer. */
#define ATMEGA_ADDR 0x04                        /* Atmega I2C address. */
#define TOTAL_NUM_CONTACTS 13                   /* Total number of contact sensors. */
#define NUM_FINGER_CONTACTS 2                   /* Number of contact sensors per finger. */
#define NUM_HANDS 2                             /* Number of hands. */
#define NUM_FINGERS 5                           /* Number of fingers per hand. */
#define NUM_FOLDS 4                             /* Number of inter-digital folds per hand. */
#define MAX_ADC 1023.0                          /* The maximum 10-bit ADC value. */
#define NUM_303 2                               /* Number of connected LSM303 accelerometers. */
#define NUM_303_VALS 6                          /* Number of values per LSM303 accelerometer reading. */
#define TOTAL_NUM_303 NUM_303 * NUM_303_VALS    /* Total number of LSM303 values. */
#define SEP_NUM_303 TOTAL_NUM_303 / 2           /* Number of LSM303 values for a given type (accelerometer or magnetometer). */
#define NUM_9DOF 2                              /* Number of connected LSM9DOF accelerometers. */
#define NUM_9DOF_VALS 9                         /* Number of values per LSM9DOF accelerometer reading. */
#define TOTAL_NUM_9DOF NUM_9DOF * NUM_9DOF_VALS /* Total bumber of LSM9DOF values. */
#define SEP_NUM_9DOF TOTAL_NUM_9DOF / 3         /* Number of LSM9DOF values for a given type (accelerometer, magnetometer, or gyrometer). */
/* Custom type definitions. */
typedef enum{ false, true } bool ; /* Used to define boolean values. */
struct Finger{  /* Structure to store finger related data. */
  unsigned int flex ;
  bool contact[NUM_FINGER_CONTACTS] ;
} ;
struct Fold{    /* Structure to store inter-digital fold related data. */
  bool contact ;
} ;
struct LSM303{  /* Structure to store LSM303 accelerometer related data. */
  double accel_x ;
  double accel_y ;
  double accel_z ;
  double mag_x ;
  double mag_y ;
  double mag_z ;
} ;
struct LSM9DOF{ /* Structure to store LSM9DOF accelerometer related data. */
  double accel_x ;
  double accel_y ;
  double accel_z ;
  double mag_x ;
  double mag_y ;
  double mag_z ;
  double gyro_x ;
  double gyro_y ;
  double gyro_z ;
} ;
struct Hand{    /* Structure to store hand related data. */
  struct Finger fingers[NUM_FINGERS] ;
  struct Fold fold[NUM_FOLDS] ;
  struct LSM303 lsm303[NUM_303] ;  
  struct LSM9DOF lsm9dof[NUM_9DOF] ;  
} ;
/* Global variables. */
volatile sig_atomic_t kb_flag = 0 ; /* Keyboard interrupt flag. */
/* Function declarations. */
bool i2c_read( const char* f_name, char buffer[MAX_CHAR], unsigned int num_bytes, const unsigned int addr,
	       int* fd, bool open_file, bool close_file, int oflags, mode_t mode ) ;
bool i2c_write( const char* f_name, char buffer[MAX_CHAR], unsigned int num_bytes, const unsigned int addr, 
		int* fd, bool open_file, bool close_file, int oflags, mode_t mode ) ;
void buffer_init( char buffer[MAX_CHAR] ) ;
void data_init( struct Hand hands[NUM_HANDS], unsigned int left_flex[NUM_FINGERS], unsigned int right_flex[NUM_FINGERS], 
                bool left_contact[TOTAL_NUM_CONTACTS], bool right_contact[TOTAL_NUM_CONTACTS], 
                double left_303_accel[SEP_NUM_303], double left_303_mag[SEP_NUM_303],
                double right_303_accel[SEP_NUM_303], double right_303_mag[SEP_NUM_303],
                double left_9dof_accel[SEP_NUM_9DOF], double left_9dof_mag[SEP_NUM_9DOF], double left_9dof_gyro[SEP_NUM_9DOF],
                double right_9dof_accel[SEP_NUM_9DOF], double right_9dof_mag[SEP_NUM_9DOF], double right_9dof_gyro[SEP_NUM_9DOF] ) ;
bool write_file( char* f_name, struct Hand hands[NUM_HANDS], char status[MAX_CHAR] ) ;
void store_data( struct Hand hands[NUM_HANDS], unsigned int flex[NUM_FINGERS], bool contact[TOTAL_NUM_CONTACTS], 
                 double accel303[SEP_NUM_303], double mag303[SEP_NUM_303], double accel9dof[SEP_NUM_9DOF], double mag9dof[SEP_NUM_9DOF],
                 double gyro9dof[SEP_NUM_9DOF], unsigned int i ) ;
void group_data( struct Hand hands[NUM_HANDS], unsigned int left_flex[NUM_FINGERS], unsigned int right_flex[NUM_FINGERS], 
                 bool left_contact[TOTAL_NUM_CONTACTS], bool right_contact[TOTAL_NUM_CONTACTS], 
                 double left_303_accel[SEP_NUM_303], double left_303_mag[SEP_NUM_303],
                 double right_303_accel[SEP_NUM_303], double right_303_mag[SEP_NUM_303],
                 double left_9dof_accel[SEP_NUM_9DOF], double left_9dof_mag[SEP_NUM_9DOF], double left_9dof_gyro[SEP_NUM_9DOF],
                 double right_9dof_accel[SEP_NUM_9DOF], double right_9dof_mag[SEP_NUM_9DOF], double right_9dof_gyro[SEP_NUM_9DOF] ) ;
void print_values( struct Hand hands[NUM_HANDS] ) ;
bool reset_sensor( char* f_name ) ;
unsigned int round_flex( unsigned int x, unsigned int lb, unsigned int ub ) ;
void signal_handler( int sig ) ;

int main( int argc, char* argv[] ){
  /* Main function. */

  char* I2C_FILE = "/dev/i2c-1" ;           /* Location of I2C Device. */ 
                                            /* Number after "i2c-" is device number, which can be assigned dynamically. */
  unsigned int num_bytes ;                  /* Number of bytes to write. */
  char buffer[MAX_CHAR] ;                   /* Buffer to store current data read from I2C device. */
  char cmd[MAX_CHAR] ;                      /* The next command to send to the microcontroller. */
  unsigned int i ;                          /* An iterator. */
  bool open_file = true ;                   /* An indicator if a file should be opened. */
  bool close_file = true ;                  /* An indicator if a file should be closed. */
  int oflags = O_RDWR ;                     /* Flags to use when opening a file. */
  mode_t mode = S_IWUSR | S_IRUSR ;         /* Permissions to use when opening a file. */
  int fd ;                                  /* File handle. */
  int result = EXIT_SUCCESS ;               /* Indicator if the program exited successfully. */
  unsigned int left_flex[NUM_FINGERS] ;     /* Array to store the left hand flex sensor data. */
  unsigned int right_flex[NUM_FINGERS] ;    /* Array to store the right hand flex sensor data. */
  bool left_contact[TOTAL_NUM_CONTACTS] ;   /* Array to store the left hand contact sensor data. */
  bool right_contact[TOTAL_NUM_CONTACTS] ;  /* Array to store the right hand contact sensor data. */
  struct timespec t1 ;                      /* Amount of time to wait. */
  struct timespec t2 ;                      /* Remaining time if delay is interrupted. */
  struct Hand hands[NUM_HANDS] ;            /* Array to store data for both hands. */
  char status[MAX_CHAR] ;                   /* Status of sensor, either connected or disconnected. */
  double left_303_accel[SEP_NUM_303] ;      /* LSM303 accelerometer data for the left hand. */
  double left_303_mag[SEP_NUM_303] ;        /* LSM303 magnetometer data for the left hand. */
  double right_303_accel[SEP_NUM_303] ;     /* LSM303 accelerometer data for the right hand. */
  double right_303_mag[SEP_NUM_303] ;       /* LSM303 magnetometer data for the right hand. */
  double left_9dof_accel[SEP_NUM_9DOF] ;    /* LSM9DOF accelerometer data for the left hand. */
  double left_9dof_mag[SEP_NUM_9DOF] ;      /* LSM9DOF magnetometer data for the left hand. */
  double left_9dof_gyro[SEP_NUM_9DOF] ;     /* LSM9DOF gyrometer data for the left hand. */
  double right_9dof_accel[SEP_NUM_9DOF] ;   /* LSM9DOF accelerometer data for the right hand. */
  double right_9dof_mag[SEP_NUM_9DOF] ;     /* LSM9DOF magnetometer data for the right hand. */
  double right_9dof_gyro[SEP_NUM_9DOF] ;    /* LSM9DOF gyrometer data for the right hand. */
  char* f_name = "/home/pi/CapstoneProject/gesture_data/gesture_data_init.xml" ; /* File to store data. */
  char* gpio_f_name = "/sys/class/gpio/gpio27/value" ; /* File handle used to reset microcontroller. */

  fprintf( stdout, "Initializing\n" ) ;
  /* Initialize the screen. Register keyboard interrupt handler. */
  signal( SIGINT, signal_handler ) ;
  /* Initialize status and command. */
  buffer_init( status ) ;
  buffer_init( cmd ) ;
  /* Initialize data. */
  data_init( hands, left_flex, right_flex, left_contact, right_contact, 
             left_303_accel, left_303_mag, right_303_accel, right_303_mag,
             left_9dof_accel, left_9dof_mag, left_9dof_gyro, right_9dof_accel, right_9dof_mag, right_9dof_gyro ) ;
  t1.tv_sec = 0 ;                  
  /* Reset microcontroller. */
  fprintf( stdout, "Reseting microcontroller.\n" ) ;
  if( !reset_sensor(gpio_f_name) ){
    perror( "*** Unable to reset sensor " ) ;
  }
  /* Continually read current sensor data. */
  while( true ){
    buffer_init( buffer ) ;
    strcpy( status, "connected" ) ;
    /* Reset microcontroller internal pointer. */
    cmd[0] = 0 ;
    num_bytes = 1 ;
    if( !i2c_write(I2C_FILE, cmd, num_bytes, ATMEGA_ADDR, &fd, open_file, close_file, oflags, mode) ){
      /* I2C bus write error. */
      strcpy( status, "disconnected" ) ;
    }
    /* Allow microcontroller sufficient time to update values. */
    t1.tv_nsec = 62500000L ;   
    nanosleep( &t1, &t2 ) ;
    /* Read flex sensors. */
    fprintf( stdout, "Reading flex sensors\n" ) ;
    t1.tv_nsec = 31250000L ;   
    num_bytes = 4 ;
    for( i = 0 ; i < (NUM_FINGERS - 1) ; i++ ){ /* Note the thumb currently doesn't have a flex sensor. */
      if( !i2c_read(I2C_FILE, buffer, num_bytes, ATMEGA_ADDR, &fd, open_file, close_file, oflags, mode) ){
        /* I2C bus read error. */
        strcpy( status, "disconnected" ) ;
        break ;
      }
      nanosleep( &t1, &t2 ) ;
      right_flex[i] = (unsigned int)atoi( buffer ) ; /* Currently there is only a right handed glove. */
    }
    /* Read contact sensors. */
    fprintf( stdout, "Reading contact sensors\n" ) ;
    buffer_init( buffer ) ;
    num_bytes = 1 ;
    for( i = 0 ; i < TOTAL_NUM_CONTACTS ; i++ ){
      if( !i2c_read(I2C_FILE, buffer, num_bytes, ATMEGA_ADDR, &fd, open_file, close_file, oflags, mode) ){
        /* I2C bus read error. */
        strcpy( status, "disconnected" ) ;
        break ;
      }
      nanosleep( &t1, &t2 ) ;
      right_contact[i] = (bool)atoi( buffer ) ;
      /* Flip the value so that contact is true, and no contact is false. */
      if( right_contact[i] == true ){
        right_contact[i] = false ;
      }
      else{
	right_contact[i] = true ;
      }
    }
    /* Read accelerometers. */
    fprintf( stdout, "Reading LSM303 accelerometers\n" ) ; 
    num_bytes = 4 ;
    for( i = 0 ; i < SEP_NUM_303 ; i++ ){
      if( !i2c_read(I2C_FILE, buffer, num_bytes, ATMEGA_ADDR, &fd, open_file, close_file, oflags, mode) ){
        /* I2C bus read error. */
        strcpy( status, "disconnected" ) ;
        break ;
      }
      nanosleep( &t1, &t2 ) ;
      right_303_accel[i] = (double)atoi( buffer ) ; /* Currently there is only a right handed glove. */
    }
    for( i = 0 ; i < SEP_NUM_303 ; i++ ){
      if( !i2c_read(I2C_FILE, buffer, num_bytes, ATMEGA_ADDR, &fd, open_file, close_file, oflags, mode) ){
        /* I2C bus read error. */
        strcpy( status, "disconnected" ) ;
        break ;
      }
      nanosleep( &t1, &t2 ) ;
      right_303_mag[i] = (double)atoi( buffer ) ; /* Currently there is only a right handed glove. */
    }
    fprintf( stdout, "Reading LSM9DOF accelerometers\n" ) ; 
    num_bytes = 6 ;
    for( i = 0 ; i < SEP_NUM_9DOF ; i++ ){
      if( !i2c_read(I2C_FILE, buffer, num_bytes, ATMEGA_ADDR, &fd, open_file, close_file, oflags, mode) ){
        /* I2C bus read error. */
        strcpy( status, "disconnected" ) ;
        break ;
      }
      nanosleep( &t1, &t2 ) ;
      right_9dof_accel[i] = (double)atoi( buffer ) ; /* Currently there is only a right handed glove. */
    }
    for( i = 0 ; i < SEP_NUM_9DOF ; i++ ){
      if( !i2c_read(I2C_FILE, buffer, num_bytes, ATMEGA_ADDR, &fd, open_file, close_file, oflags, mode) ){
        /* I2C bus read error. */
        strcpy( status, "disconnected" ) ;
        break ;
      }
      nanosleep( &t1, &t2 ) ;
      right_9dof_mag[i] = (double)atoi( buffer ) ; /* Currently there is only a right handed glove. */
    }
    for( i = 0 ; i < SEP_NUM_9DOF ; i++ ){
      if( !i2c_read(I2C_FILE, buffer, num_bytes, ATMEGA_ADDR, &fd, open_file, close_file, oflags, mode) ){
        /* I2C bus read error. */
        strcpy( status, "disconnected" ) ;
        break ;
      }
      nanosleep( &t1, &t2 ) ;
      right_9dof_gyro[i] = (double)atoi( buffer ) ; /* Currently there is only a right handed glove. */
    }
    /* Group the data. */
    group_data( hands, left_flex, right_flex, left_contact, right_contact, 
                left_303_accel, left_303_mag, right_303_accel, right_303_mag,
                left_9dof_accel, left_9dof_mag, left_9dof_gyro, right_9dof_accel, right_9dof_mag, right_9dof_gyro ) ;
    print_values( hands ) ;
    /* Output current sensor data to file. */
    fprintf( stdout, "Writing sensor data to:\t%s\n", f_name ) ;
    if( !write_file(f_name, hands, status) ){
      perror( "*** Error writing sensor data " ) ;
    }
    if( kb_flag ){
      /* Keyboard interrupt pressed. Perform clean up. */
      break ;
    }
  }
  fprintf( stdout, "\nExiting\n" ) ;

  return result ;

}

bool i2c_read( const char* f_name, char buffer[MAX_CHAR], unsigned int num_bytes, const unsigned int addr,
                      int* fd, bool open_file, bool close_file, int oflags, mode_t mode ){
  /* Function to read from an I2C device. */

  if( open_file ){
    *fd = open( f_name, oflags, mode ) ; /* File pointer. O_RDONLY, S_IRUSR */
    /* Verify file opened successfully. */
    if( *fd == -1 ){
      /* File did not open successfully.  */
      perror( "*** Unable to open I2C connection " ) ;
      return false ;
    }
  }
  /* Address device. */
  if( ioctl(*fd, I2C_SLAVE, addr) < 0 ){
    perror( "*** Unable to address I2C device " ) ;
    return false ;
  }
  /* Read byte(s) from register. */
  if( read(*fd, buffer, num_bytes) != num_bytes  ){
    perror( "*** Unable to read from I2C bus " ) ;
    return false ;
  }
  if( close_file ){
    /* Close connection to device. */
    if( close(*fd) == -1 ){
      /* File did not close successfully.  */
      perror( "*** Unable to close I2C connection " ) ;
      return false ;
    }
  }
  /*fprintf( stdout, "I2C Received %s\n", buffer ) ;*/

  return true ;
  
}

bool i2c_write( const char* f_name, char buffer[MAX_CHAR], unsigned int num_bytes, const unsigned int addr, 
		int* fd, bool open_file, bool close_file, int oflags, mode_t mode ){
  /* Function to write to an I2C device. */

  if( open_file ){
    *fd = open( f_name, oflags, mode ) ; /* File pointer. O_WRONLY, S_IWUSR */
    /* Verify file opened successfully. */
    if( *fd == -1 ){
      /* File did not open successfully.  */
      perror( "*** Unable to open I2C connection " ) ;
      return false ;
    }
  }
  /* Address device. */
  if( ioctl(*fd, I2C_SLAVE, addr) < 0 ){
    perror( "*** Unable to address I2C device " ) ;
    return false ;
  }
  /* Write to device. */
  if( write(*fd, buffer, num_bytes) != num_bytes ){
    perror( "*** Unable to write to I2C bus " ) ;
    return false ;
  }
  if( close_file ){
    /* Close connection to device. */
    if( close(*fd) == -1 ){
      /* File did not close successfully.  */
      perror( "*** Unable to close I2C connection " ) ;
      return false ;
    }
  }
  /*fprintf( stdout, "I2C Sent %s\n", buffer ) ;*/

  return true ;
  
}

void data_init( struct Hand hands[NUM_HANDS], unsigned int left_flex[NUM_FINGERS], unsigned int right_flex[NUM_FINGERS], 
                bool left_contact[TOTAL_NUM_CONTACTS], bool right_contact[TOTAL_NUM_CONTACTS], 
                double left_303_accel[SEP_NUM_303], double left_303_mag[SEP_NUM_303],
                double right_303_accel[SEP_NUM_303], double right_303_mag[SEP_NUM_303],
                double left_9dof_accel[SEP_NUM_9DOF], double left_9dof_mag[SEP_NUM_9DOF], double left_9dof_gyro[SEP_NUM_9DOF],
                double right_9dof_accel[SEP_NUM_9DOF], double right_9dof_mag[SEP_NUM_9DOF], double right_9dof_gyro[SEP_NUM_9DOF] ){
  /* Function to initialize the sensor data and data structure. */

  unsigned int i ; /* An iterator. */
  unsigned int j ; /* An iterator. */
  unsigned int k ; /* An iterator. */

  for( i = 0 ; i < NUM_FINGERS ; i++ ){
    left_flex[i] = 0 ;
    right_flex[i] = 0 ;
  }
  for( i = 0 ; i < TOTAL_NUM_CONTACTS ; i++ ){
    left_contact[i] = 0 ;
    right_contact[i] = 0 ;
  }
  for( i = 0 ; i < SEP_NUM_303 ; i++ ){
    left_303_accel[i] = 0 ;
    left_303_mag[i] = 0 ;
    right_303_accel[i] = 0 ;
    right_303_mag[i] = 0 ;
  }
  for( i = 0 ; i < SEP_NUM_9DOF ; i++ ){
    left_9dof_accel[i] = 0 ;
    left_9dof_mag[i] = 0 ;
    left_9dof_gyro[i] = 0 ;
    right_9dof_accel[i] = 0 ;
    right_9dof_mag[i] = 0 ;
    right_9dof_gyro[i] = 0 ;
  }
  for( i = 0 ; i < NUM_HANDS ; i++ ){
    for( j = 0 ; j < NUM_FINGERS ; j++ ){
      hands[i].fingers[j].flex = 0 ;
      for( k = 0 ; k < NUM_FINGER_CONTACTS ; k++ ){
        hands[i].fingers[j].contact[k] = 0 ;
      }
    }
    for( j = 0 ; j < NUM_303 ; j++ ){
      hands[i].lsm303[j].accel_x = 0.0 ;
      hands[i].lsm303[j].accel_y = 0.0 ;
      hands[i].lsm303[j].accel_z = 0.0 ;
      hands[i].lsm303[j].mag_x = 0.0 ;
      hands[i].lsm303[j].mag_y = 0.0 ;
      hands[i].lsm303[j].mag_z = 0.0 ;
    }
    for( j = 0 ; j < NUM_9DOF ; j++ ){
      hands[i].lsm9dof[j].accel_x = 0.0 ;
      hands[i].lsm9dof[j].accel_y = 0.0 ;
      hands[i].lsm9dof[j].accel_z = 0.0 ;
      hands[i].lsm9dof[j].mag_x = 0.0 ;
      hands[i].lsm9dof[j].mag_y = 0.0 ;
      hands[i].lsm9dof[j].mag_z = 0.0 ;
      hands[i].lsm9dof[j].gyro_x = 0.0 ;
      hands[i].lsm9dof[j].gyro_y = 0.0 ;
      hands[i].lsm9dof[j].gyro_z = 0.0 ;
    }
  }

  return ;

}

void store_data( struct Hand hands[NUM_HANDS], unsigned int flex[NUM_FINGERS], bool contact[TOTAL_NUM_CONTACTS], 
                 double accel303[SEP_NUM_303], double mag303[SEP_NUM_303], double accel9dof[SEP_NUM_9DOF], double mag9dof[SEP_NUM_9DOF],
                 double gyro9dof[SEP_NUM_9DOF], unsigned int i ){
  /* Function to store the data into the appropriate fields within the data structure. */

  unsigned int j ; /* An iterator. */
  unsigned int k ; /* An iterator. */
  unsigned int m = 0 ; /* An iterator. */

  for( j = 0 ; j < NUM_FINGERS ; j++ ){
    hands[i].fingers[j].flex = flex[j] ;
    for( k = 0 ; k < NUM_FINGER_CONTACTS ; k++ ){
      hands[i].fingers[j].contact[k] = contact[m] ; 
      m++ ;
      if( j == (NUM_FINGERS - 1) ){
        /* Currently the thumb has only one contact sensor. */
	break ;
      }
    }
  }
  for( j = 0 ; j < NUM_FOLDS ; j++ ){
    hands[i].fold[j].contact = contact[m] ;
    m++ ;
  }
  k = 0 ;
  for( j = 0 ; j < NUM_303 ; j++ ){
    hands[i].lsm303[j].accel_x = accel303[k] ;
    hands[i].lsm303[j].mag_x = mag303[k++] ;
    hands[i].lsm303[j].accel_y = accel303[k] ;
    hands[i].lsm303[j].mag_y = mag303[k++] ;
    hands[i].lsm303[j].accel_z = accel303[k] ;
    hands[i].lsm303[j].mag_z = mag303[k++] ;
  }
  k = 0 ;
  for( j = 0 ; j < NUM_9DOF ; j++ ){
    hands[i].lsm9dof[j].accel_x = accel9dof[k] ;
    hands[i].lsm9dof[j].mag_x = mag9dof[k] ;
    hands[i].lsm9dof[j].gyro_x = gyro9dof[k++] ;
    hands[i].lsm9dof[j].accel_y = accel9dof[k] ;
    hands[i].lsm9dof[j].mag_y = mag9dof[k] ;
    hands[i].lsm9dof[j].gyro_y = gyro9dof[k++] ;
    hands[i].lsm9dof[j].accel_z = accel9dof[k] ;
    hands[i].lsm9dof[j].mag_z = mag9dof[k] ;
    hands[i].lsm9dof[j].gyro_z = gyro9dof[k++] ;
  }

  return ;

}
 
void group_data( struct Hand hands[NUM_HANDS], unsigned int left_flex[NUM_FINGERS], unsigned int right_flex[NUM_FINGERS], 
                 bool left_contact[TOTAL_NUM_CONTACTS], bool right_contact[TOTAL_NUM_CONTACTS], 
                 double left_303_accel[SEP_NUM_303], double left_303_mag[SEP_NUM_303],
                 double right_303_accel[SEP_NUM_303], double right_303_mag[SEP_NUM_303],
                 double left_9dof_accel[SEP_NUM_9DOF], double left_9dof_mag[SEP_NUM_9DOF], double left_9dof_gyro[SEP_NUM_9DOF],
                 double right_9dof_accel[SEP_NUM_9DOF], double right_9dof_mag[SEP_NUM_9DOF], double right_9dof_gyro[SEP_NUM_9DOF] ){
  /* Function to group the sensor data into a data structure. */

  unsigned int i ;     /* An iterator. */

  for( i = 0 ; i < NUM_HANDS ; i++ ){
    if( i == 0 ){
      /* Left hand data. */
      store_data( hands, left_flex, left_contact, left_303_accel, left_303_mag, left_9dof_accel, left_9dof_mag, left_9dof_gyro, i ) ;
    }
    else{
      /* Right hand data. */
      store_data( hands, right_flex, right_contact, right_303_accel, right_303_mag, right_9dof_accel, right_9dof_mag, right_9dof_gyro, i ) ;     
    }
  }

  return ;

}

bool write_file( char* f_name, struct Hand hands[NUM_HANDS], char status[MAX_CHAR] ){
  /* Function to generate an output XML file. */

  FILE* fp ;                                   /* File handle. */
  char* hand_name[NUM_HANDS] = {"left",        /* A list of names for the hands. */
                                "right"
                               } ;
  char* finger_name[NUM_FINGERS] = {"index",   /* A list of names for the fingers. */
                                    "middle", 
                                    "ring", 
                                    "pinky",
                                    "thumb"
                                   } ;
  char* fold_name[NUM_FOLDS] = {"thumb-index",              /* A list of names for the folds between fingers. */
                                "index-middle", 
                                "middle-ring", 
                                "ring-pinky"
                               } ;      
  char* contact_name[NUM_FINGER_CONTACTS] = {"contact-tip", /* A list of the contact names. */
                                             "contact-mid"
                                            } ;       
  char* lsm303_side[NUM_303] = {"top",                      /* A list of the LSM303 positions. */
                                "bottom"
                               } ;       
  char* lsm9dof_side[NUM_9DOF] = {"top",                    /* A list of the LSM303 positions. */
                                  "bottom"
                                 } ;       
  unsigned int i ;  /* An iterator. */
  unsigned int j ;  /* An iterator. */
  unsigned int k ;  /* An iterator. */
  unsigned int flex_adjust ; /* The adjusted flex sensor value, in the range 0 - 100. */
  unsigned int flex_round ;  /* The flex sensor value rounded to one of three values: 0, 50, 100.*/
  const unsigned int lb = 10 ; /* The lower bound for rounding. */
  const unsigned int ub = 40 ; /* The upper bound for rounding. */

  fp = fopen( f_name, "w" ) ;
  if( fp == NULL ){
    return false ;
  }
  /* Write the header. */
  fprintf( fp,  "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>\n" ) ;
  fprintf( fp,  "<?xml-stylesheet type=\"text/xsl\" href=\"gesture_data.xsl\"?>\n" ) ;
  fprintf( fp,  "<!DOCTYPE gestures SYSTEM \"gesture_data.dtd\">\n" ) ;
  /* Write the next set of gestures. */
  fprintf( fp,  "<gestures>\n" ) ;
  /* Write the next gesture. */
  fprintf( fp,  "\t<gesture>\n" ) ;
  for( i = 0 ; i < NUM_HANDS ; i++ ){
    /* Write the next set of hand data. */
    fprintf( fp, "\t\t<hand side=\"%s\">\n", hand_name[i] ) ;
    for( j = 0 ; j < NUM_FINGERS ; j++ ){
      /* Write the next set of finger data. */
      fprintf( fp, "\t\t\t<%s>\n", finger_name[j] ) ;
      /* Express flex sensor values in range 0-100.*/
      flex_adjust = (unsigned int)(((float)hands[i].fingers[j].flex / MAX_ADC) * 100.0) ;
      flex_round = round_flex( flex_adjust, lb, ub ) ;
      fprintf( fp, "\t\t\t\t<flex>%u</flex>\n", flex_round ) ; 
      for( k = 0 ; k < NUM_FINGER_CONTACTS ; k++ ){
	if( (strcmp(finger_name[j], "thumb") == 0) && (k == (NUM_FINGER_CONTACTS - 1)) ){
	  /* Thumb only has a tip contact sensor. */
	  break ;
	}
	/* Convert binary values to boolean values. */
        if( hands[i].fingers[j].contact[k] ){
          fprintf( fp, "\t\t\t\t<%s>true</%s>\n", contact_name[k], contact_name[k] ) ; 
        }
        else{
          fprintf( fp, "\t\t\t\t<%s>false</%s>\n", contact_name[k], contact_name[k] ) ; 
        }
      }
      /* Close out the finger data. */
      fprintf( fp, "\t\t\t</%s>\n", finger_name[j] ) ;
    }
    for( j = 0 ; j < NUM_FOLDS ; j++ ){
      /* Write the next set of fold data. */
      fprintf( fp, "\t\t\t<%s>\n", fold_name[j] ) ;
      if( hands[i].fold[j].contact ){
        fprintf( fp, "\t\t\t\t<%s>true</%s>\n", contact_name[0], contact_name[0] ) ;
      }
      else{
        fprintf( fp, "\t\t\t\t<%s>false</%s>\n", contact_name[0], contact_name[0] ) ;
      }
      /* Close out the fold data. */
      fprintf( fp, "\t\t\t</%s>\n", fold_name[j] ) ;
    }
    for( j = 0 ; j < NUM_303 ; j++ ){ /* Only print out one accelerometer for now. */
      /* Write the next set of LSM303 accelerometer data. */
      fprintf( fp, "\t\t\t<lsm303 side=\"%s\">\n", lsm303_side[j] ) ;
      fprintf( fp, "\t\t\t\t<accel-x>%f</accel-x>\n", hands[i].lsm303[j].accel_x ) ;
      fprintf( fp, "\t\t\t\t<accel-y>%f</accel-y>\n", hands[i].lsm303[j].accel_y ) ;
      fprintf( fp, "\t\t\t\t<accel-z>%f</accel-z>\n", hands[i].lsm303[j].accel_z ) ;
      fprintf( fp, "\t\t\t\t<mag-x>%f</mag-x>\n", hands[i].lsm303[j].mag_x ) ;
      fprintf( fp, "\t\t\t\t<mag-y>%f</mag-y>\n", hands[i].lsm303[j].mag_y ) ;
      fprintf( fp, "\t\t\t\t<mag-z>%f</mag-z>\n", hands[i].lsm303[j].mag_z ) ;
      fprintf( fp, "\t\t\t</lsm303>\n" ) ;
    }
    for( j = 0 ; j < NUM_9DOF ; j++ ){ /* Only print out one accelerometer for now. */
      /* Write the next set of LSM9DOF accelerometer data. */
      fprintf( fp, "\t\t\t<lsm9dof side=\"%s\">\n", lsm9dof_side[j] ) ;
      fprintf( fp, "\t\t\t\t<accel-x>%f</accel-x>\n", hands[i].lsm9dof[j].accel_x ) ;
      fprintf( fp, "\t\t\t\t<accel-y>%f</accel-y>\n", hands[i].lsm9dof[j].accel_y ) ;
      fprintf( fp, "\t\t\t\t<accel-z>%f</accel-z>\n", hands[i].lsm9dof[j].accel_z ) ;
      fprintf( fp, "\t\t\t\t<mag-x>%f</mag-x>\n", hands[i].lsm9dof[j].mag_x ) ;
      fprintf( fp, "\t\t\t\t<mag-y>%f</mag-y>\n", hands[i].lsm9dof[j].mag_y ) ;
      fprintf( fp, "\t\t\t\t<mag-z>%f</mag-z>\n", hands[i].lsm9dof[j].mag_z ) ;
      fprintf( fp, "\t\t\t\t<gyro-x>%f</gyro-x>\n", hands[i].lsm9dof[j].gyro_x ) ;
      fprintf( fp, "\t\t\t\t<gyro-y>%f</gyro-y>\n", hands[i].lsm9dof[j].gyro_y ) ;
      fprintf( fp, "\t\t\t\t<gyro-z>%f</gyro-z>\n", hands[i].lsm9dof[j].gyro_z ) ;
      fprintf( fp, "\t\t\t</lsm9dof>\n" ) ;
    }
    /* Close out the hand data. */
    fprintf( fp, "\t\t</hand>\n" ) ;
  } 
  /* Close out the gesture data. */
  fprintf( fp, "\t</gesture>\n" ) ;
  /* Write the converted text. */
  fprintf( fp, "\t<converted-text></converted-text>\n" ) ;
  /* Write the sensor status. */
  fprintf( fp, "\t<status>%s</status>\n", status ) ;
  /* Write the conversion status. */
  fprintf( fp, "\t<convert>false</convert>\n" ) ;
  /* Write the XML version. */
  fprintf( fp, "\t<version>1.0</version>\n" ) ;
  /* Close out the set of gestures. */
  fprintf( fp, "</gestures>\n" ) ;

  fclose( fp ) ;

  return true ;

}

void buffer_init( char buffer[MAX_CHAR] ){
  /* Function to initialize a buffer of characters. */

  unsigned int i ; /* An iterator. */

  for( i = 0 ; i < MAX_CHAR ; i++ ){
    buffer[i] = '\0' ;
  }

  return ;

}

bool reset_sensor( char* f_name ){
  /* Function to reset the microcontroller and attached sensors. */

  FILE* fp ;                                /* File handle. */
  struct timespec t1 ;                      /* Amount of time to wait. */
  struct timespec t2 ;                      /* Remaining time if delay is interrupted. */
  const unsigned int NUM_REPS = 3 ;         /* The number of iterations to perform. */
  unsigned int i ;                          /* An iterator. */

  t1.tv_sec = 0.0 ;
  t1.tv_nsec = 250000000L ;   

  /* Open a handle to the GPIO pin. Create a distinct pulse. */
  for( i = 0 ; i < NUM_REPS ; i++ ){
    fp = fopen( f_name, "w" ) ;
    if( fp == NULL ){
      return false ;
    }
    if( (i % 2) != 0 ){
      fprintf( fp, "0" ) ;  
    }
    else{
      fprintf( fp, "1" ) ;  
    }
    fclose( fp ) ;
    nanosleep( &t1, &t2 ) ;
  }
  /* Wait sufficient time for the microcontroller to reset.  */
  t1.tv_sec = 2.0 ;
  t1.tv_nsec = 0 ;   
  fprintf( stdout, "Waiting for microcontroller to reset..." ) ;
  nanosleep( &t1, &t2 ) ;
  fprintf( stdout, "Done.\n" ) ;

  return true ;
}

void print_values( struct Hand hands[NUM_HANDS] ){
  /* Function to print the values read from the sensors to the screen. */

  char* hand_name[NUM_HANDS] = {"left",         /* A list of names for the hands. */
                                "right"
                               } ;
  char* finger_name[NUM_FINGERS] = {"index",    /* A list of names for the flex sensors. */
                                    "middle", 
                                    "ring", 
                                    "pinky",
                                    "thumb"
                                   } ;
  char* fold_name[NUM_FOLDS] = {"thumb-Index",  /* A list of names for the folds between fingers. */
                                "index-Middle", 
                                "middle-Ring", 
                                "ring-Pinky"
                               } ;             
  char* lsm303_side[NUM_303] = {"top",          /* A list of the LSM303 positions. */
                                "bottom"
                               } ;       
  char* lsm9dof_side[NUM_9DOF] = {"top",        /* A list of the LSM303 positions. */
                                  "bottom"
                                 } ;       
  char* border = "***\n" ;                      /* The border to print. */
  unsigned int i ;
  unsigned int j ;

  /* Print out the flex sensor values. */
  for( i = 0 ; i < NUM_HANDS ; i++ ){
    fprintf( stdout, "%s Hand:\n", hand_name[i] ) ;
    fprintf( stdout, border ) ;
    for( j = 0 ; j < NUM_FINGERS ; j++ ){
      if( strcmp(finger_name[j], "thumb") == 0 )
	/* Currently the thumb does not have a flex sensor. */
	continue ;
      fprintf( stdout, "%s Flex: %u\t\t", finger_name[j], hands[i].fingers[j].flex ) ;
    }
    fprintf( stdout, "\n" ) ;
    fprintf( stdout, border ) ;
    /* Print out the contact sensor values. */
    for( j = 0 ; j < NUM_FINGERS ; j++ ){
      fprintf( stdout, "%s Tip Contact: %u\t\t", finger_name[j], hands[i].fingers[j].contact[0] ) ;
      if( strcmp(finger_name[j], "thumb") == 0 ){
        /* Currently the thumb has only one contact sensor. */
	fprintf( stdout, "\n" ) ;
        continue ;
      }
      fprintf( stdout, "%s Mid Contact: %u\n", finger_name[j], hands[i].fingers[j].contact[1] ) ;
    }
    fprintf( stdout, border ) ;
    for( j = 0 ; j < NUM_FOLDS ; j++ ){
      fprintf( stdout, "%s Contact: %u\t\t", fold_name[j], hands[i].fold[j].contact ) ;
      if( (j % 2) != 0 ){
	fprintf( stdout, "\n" ) ;
      }
    }
    fprintf( stdout, border ) ;
    for( j = 0 ; j < NUM_303 ; j++ ){
      fprintf( stdout, "LSM303 Side: %s\n", lsm303_side[j] ) ;
      fprintf( stdout, "LSM303 Accel: (%f, %f, %f)\t", hands[i].lsm303[j].accel_x, hands[i].lsm303[j].accel_y, hands[i].lsm303[j].accel_z ) ;
      fprintf( stdout, "LSM303 Mag: (%f, %f, %f)\n", hands[i].lsm303[j].mag_y, hands[i].lsm303[j].mag_x, hands[i].lsm303[j].mag_z ) ;
    }
    fprintf( stdout, border ) ;
    for( j = 0 ; j < NUM_9DOF ; j++ ){
      fprintf( stdout, "LSM9DOF Side: %s\n", lsm9dof_side[j] ) ;
      fprintf( stdout, "LSM9DOF Accel: (%f, %f, %f)\t", hands[i].lsm9dof[j].accel_x, hands[i].lsm9dof[j].accel_y, hands[i].lsm9dof[j].accel_z ) ;
      fprintf( stdout, "LSM9DOF Mag: (%f, %f, %f)\t", hands[i].lsm9dof[j].mag_x, hands[i].lsm9dof[j].mag_y, hands[i].lsm9dof[j].mag_z ) ;
      fprintf( stdout, "LSM9DOF Gyro: (%f, %f, %f)\n", hands[i].lsm9dof[j].gyro_x, hands[i].lsm9dof[j].gyro_y, hands[i].lsm9dof[j].gyro_z ) ;
    }
    fprintf( stdout, border ) ;
  }

  return ;

}

unsigned int round_flex( unsigned int x, unsigned int lb, unsigned int ub ){
  /* Function to round input flex sensor values. */

  const unsigned int MIN_VAL = 0 ;
  const unsigned int MID_VAL = 50 ;
  const unsigned int MAX_VAL = 100 ;

  if( x < lb ){
    return MIN_VAL ;
  }
  else if( x > ub ){
    return MAX_VAL ;
  } 
  else{
    return MID_VAL ;
  }

}

void signal_handler( int sig ){ 

    kb_flag = 1 ; 

    return ;

}