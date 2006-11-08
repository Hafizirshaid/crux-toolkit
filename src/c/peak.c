/*****************************************************************************
 * \file peak.c
 * AUTHOR: William Stafford Noble
 * CREATE DATE: 6/14/04
 * VERSION: $Revision: 1.11 $
 * DESCRIPTION: Object for representing one peak in a spectrum.
 *****************************************************************************/
#include "peak.h"
#include "utils.h"
#include <string.h>
#include "objects.h"
#include <stdlib.h>
#include <stdio.h>
#include "carp.h"

/**
 * \struct peak
 * \brief A spectrum peak.
 */
struct peak {
  float intensity;  ///< The intensity of the peak.
  float location;   ///< The location of the peak.
};    

/**
 * \returns A PEAK_T object, 
 * heap allocated, must be freed using free_peak method
 */
PEAK_T* new_peak (
  float intensity, ///< intensity for the new peak -in 
  float location ///< location for the new peak -in
  )
{
  PEAK_T* fresh_peak =(PEAK_T*)mymalloc(sizeof(PEAK_T));
  fresh_peak->intensity = intensity;
  fresh_peak->location = location;
  return fresh_peak;
}
/**
 * frees A PEAK_T object
 */
void free_peak (
  PEAK_T* garbage_peak ///< the peak to free -in
  )
{
  free(garbage_peak);
}

/**
 * \returns the intensity of PEAK_T object
 */
float get_peak_intensity(
  PEAK_T* working_peak///< return the intensity of this peak -in
)
{
  return working_peak->intensity;
}

/**
 * \returns the location of PEAK_T object
 */
float get_peak_location(
  PEAK_T* working_peak///< return the location of this peak -in 
  )
{
  return working_peak->location;
}


/**
 * sets the intensity of PEAK_T object
 */
void set_peak_intensity(
  PEAK_T* working_peak, ///<set the intensity of this peak -out
  float intensity///< the intensity -in
 )
{
  working_peak->intensity = intensity;
}

/**
 * sets the location of PEAK_T object
 */
void set_peak_location(
  PEAK_T* working_peak, ///<set the location of this peak -out
  float location ///< the location -in
  )
{
  working_peak->location = location;
}

/**
 * prints the intensity and location of PEAK_T object to stdout
 */
void print_peak(
  PEAK_T* working_peak ///< print this peak -in
  )
{
  printf("%.1f %.1f\n", 
         working_peak->location,
         working_peak->intensity);
}

/**
 * \returns A heap allocated PEAK_T object array
 */
PEAK_T* allocate_peak_array(
  int num_peaks///< number of peaks to allocate -in
  )
{
  PEAK_T* peak_array;
  peak_array = (PEAK_T*)mycalloc(1,sizeof(PEAK_T) * num_peaks);
  return peak_array;
}

/**
 * frees A PEAK_T object array
 */
void free_peak_array(
  PEAK_T* garbage_peak ///<the peak array to free -in
  ) 
{
  free(garbage_peak);
}

/**
 *\returns a pointer to the peak in the peak_array
 */
PEAK_T* find_peak(
  PEAK_T* peak_array,///< peak_array to search -in
  int index ///< the index of peak to fine -in
  )
{
  return &peak_array[index];
}


/***********************************************
 * Sort peaks
 * also functions for lib. function qsort(),
 * although maybe used for other purposes
 ************************************************/

/**
 * Written for the use of lib. function, qsort()
 * compare the intensity of peaks
 *\returns -1 if peak_1 is larger, 1 if peak_2, 0 if equal
 */
int compare_peaks_by_intensity(
  const void* peak_1, ///< peak one to compare -in
  const void* peak_2  ///< peak two to compare -in
  )
{
  PEAK_T* peak_one = (PEAK_T*)peak_1;
  PEAK_T* peak_two = (PEAK_T*)peak_2;
  
  if(peak_one->intensity > peak_two->intensity){
    return -1;
  }
  else if(peak_one->intensity < peak_two->intensity){
    return 1;
  }
  //peak_one == peak_two
  return 0;
}


/**
 * Written for the use of lib. function, qsort()
 * compare the mz(location) of peaks
 *\returns 1 if peak_1 is larger, -1 if peak_2, 0 if equal
 */
int compare_peaks_by_mz(
  const void* peak_1, ///< peak one to compare -in
  const void* peak_2  ///< peak two to compare -in
  )
{
  PEAK_T* peak_one = (PEAK_T*)peak_1;
  PEAK_T* peak_two = (PEAK_T*)peak_2;
  
  if(peak_one->location > peak_two->location){
    return 1;
  }
  else if(peak_one->location < peak_two->location){
    return -1;
  }
  //peak_one == peak_two
  return 0;
}

/**
 * sort peaks by their intensity or location
 * use the lib. function, qsort()
 * sorts intensity in descending order
 */
void sort_peaks(
  PEAK_T* peak_array, ///< peak array to sort -in/out
  int num_peaks,  ///< number of total peaks -in
  PEAK_SORT_TYPE_T sort_type ///< the sort type(location or intensity)
  )
{
  if(sort_type == _PEAK_INTENSITY){
    //sort the peaks by intensity
    qsort((void*)peak_array, num_peaks, sizeof(PEAK_T), compare_peaks_by_intensity);
  }
  else if(sort_type == _PEAK_LOCATION){
    //sort the peaks by location
    qsort((void*)peak_array, num_peaks, sizeof(PEAK_T), compare_peaks_by_mz);
  }
  else{
    carp(CARP_ERROR, "no matching peak sort type");
    exit(1);
  }
}
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
