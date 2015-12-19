/***********Finger.cpp**************************************************************

  By:       Timothy Miskell         
            Virinchi Balabhadrapatruni
            16.499 Capstone Proposal
            ECE Department                               
            Umass Lowell                                                                        
 
  PURPOSE:  The Set() and Show() functions are defined in this module.

  CHANGES:  08/09/2015
                                                                                 
************************************************************************************/

#include <climits>
#include "Finger.h"

/*----------Finger::Set( )-----------------------------------------------------------

  PURPOSE:  Given a pair of sensors, store their values as a "Finger".

  INPUT  PARAMETERS:  flexVal    -- the flex sensor value.
                      contactVal -- the contact sensor value.

-----------------------------------------------------------------------------------*/

void Finger::Set( double flexVal, bool contactVal ) {

        flex    = flexVal ;
        contact = contactVal ;
	defined = true ;
	
	return ;

}

/*----------Finger::Show( )----------------------------------------------------------

  PURPOSE:  Display a "Finger" on the screen.

  INPUT PARAMETERS:  os -- an output stream.

  OUTPUT PARAMETERS: os -- the modified output stream.
    
-----------------------------------------------------------------------------------*/

void Finger::Show( ostream &os ) const {

  if( defined ){
    os << "\t\t" << "Flex Sensor:\t" << Flex() << "\n" ;
    os << "\t\t" << "Contact Sensor:\t" << Contact() << "\n" ;
  }

  return ;

}
