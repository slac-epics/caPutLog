/**************************************************************/
/*                           IDCP                             */
/*            The Insertion Device Control Program            */
/*                 Author: Goetz Pfeiffer                     */
/**************************************************************/

/*____________________________________________________________*/
/* project information:                                       */
/*____________________________________________________________*/
/* project: IDCP                                              */
/* module: CSM-BASE                                           */
/* version-number of module: 1.0                              */
/* author: Goetz Pfeiffer                                     */
/* last modification date: 2004-04-15                         */
/* status: beta-test                                          */
/*____________________________________________________________*/

/*____________________________________________________________*/
/*                          comments                          */
/*____________________________________________________________*/

    /*@ETI------------------------------------------------*/
    /*     comments for the automatically generated       */
    /*  header-file, this does not concern csm.c but      */
    /*                      csmbase.h !                   */
    /*----------------------------------------------------*/

/*@EM("/@ This file was automatically generated, all manual changes\n") 
  @EM("   to this file will be lost, edit csm.c instead of csm.h !@/\n\n") */

    /*----------------------------------------------------*/
    /*  	   RCS-properties of this file            */
    /*----------------------------------------------------*/

/*@EM("    /@   RCS-properties of the underlying source csmbase.c   @/\n")@IT*/   
    
/* Author:              $Author: pfeiffer $
   check-in date:       $Date: 2004/04/27 10:38:08 $
   locker of this file: $Locker:  $
   Revision:            $Revision: 1.9 $
   State:               $State: Exp $
*/
   
    /*@ETI------------------------------------------------*/
    /*			    History                       */
    /*----------------------------------------------------*/
    
/* 
Version 0.9:
  Date: 6/16/97
  This is the first implementation csm. 

Version 0.91:
  Date: 8/22/97
  A mechanism to accelerate the search of x-values in the table has been
  implemented. Now the interval-borders from the previous search are
  tested (and if it makes sense taken as borders for the new search).
  This should speed up the process if table_x_lookup_ind is called with
  x-values that do not differ too much from each other (in terms of the
  table-values). This version has not yet been tested.

Version 0.92:
  Date: 10/16/97
  The program (csmbase) has been splitted from the original csm and now
  contains only generic routines for implementing linear and table-based
  conversions. All IDCP specific conversion funtions are now placed
  in csm.c. Note that for this file (csmbase.c) version 0.92 is the first
  version!

Version 0.93:
  Date: 5/5/99
  csmbase was extended in order to support 2-dimensional functions (z=f(x,y))
  These functions are specified in form of a table :
	 y1   y2   y3   .   .   .
     x1  z11  z12  z13  .   .   .
     x2  z21  z22  z23  .   .   .
     .    .    .    .
     .    .    .    .
     .    .    .    .

Version 0.94:
  Date: 6/16/99
  The functions csm_read_table and csm_read_xytable were changed in order to 
  work even if some lines of the data-file do not contain data (this is 
  especially the case with empty lines).

Version 0.95:
  Date: 5/26/00
  An error in coordinate_lookup_index() was removed. This caused the function
  to return wrong parameters when applied with x==2.

Version 0.96:
  Date: 10/13/00
  Small changes. If a function is not yet defined (type CSM_NOTHING), the
  function returns 0.
  
  Date: 2002-03-05
  Errors in the header-file generation were fixed, some 
  left-overs from previous debugging were removed
  
  Date: 2004-04-05
  in csm_read_table a superfluous parameter was removed
*/

    /*----------------------------------------------------*/
    /*		        General Comments                  */
    /*----------------------------------------------------*/
    
/*! \file csmbase.c
    \brief implement function table
    \author Goetz Pfeiffer (pfeiffer\@mail.bessy.de)
    \version 1.0
    
   This module implements one and two dimensional function tables.
   The tables can be specified by simple ascii files. The points 
   defined in the file don't have to be in the same distance.
*/


/*@ITI________________________________________________________*/
/*                      general Defines                       */
/*@ET_________________________________________________________*/

/*@EM("\n#ifndef __CSMBASE_H\n#define __CSMBASE_H\n") */

/*! \brief needed for POSIX compability */
#define _INCLUDE_POSIX_SOURCE

/*! \brief use DBG module ? */
#define USE_DBG 0

/*! \brief use epics printf ? */
#define USE_EPICS_DBG 1

/* the following macros affect the debug (dbg) module: */

#if USE_DBG

/*! \brief compile DBG assertions */
#define DBG_ASRT_COMP 0 

/*! \brief compile DBG debug messages */
#define DBG_MSG_COMP 0 

/*! \brief compile DBG trace messages */
#define DBG_TRC_COMP  0

/*! \brief use local switch-variables for the dbg module */
#define DBG_LOCAL_COMP 1

/*! \brief use async. I/O with message passing in DBG module*/
#define DBG_ASYNC_COMP 0

/* the following defines are for sci-debugging, they do not influence
   any header-files but are placed here since they are usually changed 
   together with the above macros.*/
   
/*! \brief for debug-outputs, "stdout" means console */
#define CSMBASE_OUT_FILE "stdout"

/*! \brief csm trace level

  \li level 6: dense traces for debugging
  \li level 5: enter/exit tracing
*/
#define CSMBASE_TRC_LEVEL 0

/*! \brief flushmode, 0, 1 and 2 are allowed */
#define CSMBASE_FLUSHMODE 1

#else

/*! \internal \brief compability macro, needed when DBG is not used */
#define DBG_MSG_PRINTF2(f,x) errlogPrintf(f,x)

/*! \internal \brief compability macro, needed when DBG is not used */
#define DBG_MSG_PRINTF3(f,x,y) errlogPrintf(f,x,y)

/*! \internal \brief compability macro, needed when DBG is not used */
#define DBG_MSG_PRINTF4(f,x,y,z) errlogPrintf(f,x,y,z)

#endif

/*! \brief for type csm_bool */
#define CSM_TRUE 1

/*! \brief for type csm_bool */
#define CSM_FALSE 0

/*____________________________________________________________*/
/*			 Include-Files			      */
/*____________________________________________________________*/

/* #include <basic.h> */



#include <vxWorks.h>
#include <semaphore.h> 


#if USE_DBG
#include <dbg.h>     
#else
/* #include <logLib.h> */
#include <errlog.h> /* epics error printf */    
#endif

#include <stdlib.h>
#include <stdio.h>    /*@IL*/
#include <string.h>

/*@ITI________________________________________________________*/
/*                          Types                             */
/*@ET_________________________________________________________*/

      /*................................................*/
      /*@IL        the csm_bool type (public)           */
      /*................................................*/

/*! \brief typedef-struct: the csm_bool data type */

typedef int csm_bool; /*@IL*/

      /*................................................*/
      /*          the coordinate type (private)         */
      /*................................................*/

/*! \internal \brief typedef-struct: a single coordinate value */ 
typedef struct
  { 
    double value; /*!< the floating point value of the coordinate */
    int    index; /*!< the index-number within this list of coordinates */
  } csm_coordinate;
  
/*! \internal \brief typedef-struct: a list of coordinates */ 
typedef struct
  { csm_coordinate *coordinate;  /*!< pointer to coordinate array */
    int            no_of_elements;
                                 /*!< number of elements in the coordinate 
				      array */
    int            a_last;       /*!< last left index */
    int            b_last;       /*!< last right index */
    int		   ret_last;     /*!< last return-code of lookup function */
    double         x_last;       /*!< last x that was looked up */
  } csm_coordinates;   

/*! \endif */


      /*................................................*/
      /*        the linear function type (private)      */
      /*................................................*/

/*! \internal \brief typedef-struct: a linear function y = a + b*x */
typedef struct
  { double a;  
    double b; 
  } csm_linear_function;

      /*................................................*/
      /*           the 1d table type (private)          */
      /*................................................*/

/*! \internal \brief typedef-struct: one-dimenstional function table 

  This is a list of x-y pairs that define a one-dimensional 
  function. The function can be inverted, if it is monotone.
*/ 
typedef struct
  { csm_coordinates x;    /*!< list of x-coordinates */ 
    csm_coordinates y;    /*!< list of y-coordinates */ 
  } csm_1d_functiontable; 
  
      /*................................................*/
      /*          the 2d table type (private)           */
      /*................................................*/

/*! \internal \brief typedef-struct: two-dimenstional function table 

  This is a list of x-y pairs and corresponding z-values that 
  define a two-dimensional function table. The function cannot be inverted.
*/ 
typedef struct
  { csm_coordinates x;   /*!< list of x-coordinates */ 
    csm_coordinates y;   /*!< list of y-coordinates */ 
    double          *z;  /*!< list of z-values (function values) */ 
  } csm_2d_functiontable; 
  
      /*................................................*/
      /*           the function-type (private)          */
      /*................................................*/

/*! \internal \brief typedef-enum: types of functions known in this module */
typedef enum { CSM_NOTHING,  /*!< kind of NULL value */
               CSM_LINEAR,   /*!< linear y=a+b*x */
               CSM_1D_TABLE, /*!< one-dimensional function table */
	       CSM_2D_TABLE  /*!< two-dimensional function table */
	     } csm_func_type;        

/*! \internal \brief typedef-struct: compound type for functions */
typedef struct 
  { SEM_ID   semaphore;   /*!< semaphore to lock access to csm_Function */
    double   last;        /*!< last value that was calculated */
    csm_bool on_hold;     /*!< when TRUE, just return last */
    csm_func_type type;   /*!< the type of the function */
    
    union { csm_linear_function    lf;   /*!< linear function */
            csm_1d_functiontable   tf_1; /*!< 1d function table */
            csm_2d_functiontable   tf_2; /*!< 2d function table */
	  } f;            /*!< union that holds the actual data */
  } csm_Function;

      /*................................................*/
      /*@IL       the function-object (public)          */
      /*................................................*/

/*! \brief typedef-struct: the abstract csm function object */
/*@IT*/
typedef struct {                        
                 char dummy; 
               } csm_function;

/*@ET*/

/*____________________________________________________________*/
/*			  Variables			      */
/*____________________________________________________________*/
  
      /*................................................*/
      /*           initialization (private)             */
      /*................................................*/

static csm_bool initialized= CSM_FALSE;

/*____________________________________________________________*/
/*                        Functions                           */
/*@ET_________________________________________________________*/


    /*----------------------------------------------------*/
    /*             debug - managment (private)            */
    /*----------------------------------------------------*/

#if USE_DBG
/*! \internal \brief initialize the DBG module [static]

  This internal function is called by csmbase_init
*/

static void csm_dbg_init(void)
  { static csm_bool csmbase_init= CSM_FALSE;

    if (csmbase_init)
      return;
    csmbase_init= CSM_TRUE;
    /* initialize the debug (dbg) module: use stdout for error-output */
    DBG_SET_OUT(CSMBASE_OUT_FILE);
    DBG_TRC_LEVEL= CSMBASE_TRC_LEVEL;
                           /* level 6: dense traces for debugging
                              level 5: enter/exit tracing
                              level 2: output less severe errors and warnings
                              level 1: output of severe errors
                              level 0: no output */
    DBG_FLUSHMODE(CSMBASE_FLUSHMODE);
  }
  
#endif

    /*----------------------------------------------------*/
    /*        utilities for csm-coordinates (private)     */
    /*----------------------------------------------------*/

        /*              initialization                */

/*! \internal \brief initialize a csm_coordinates structure [static]

  This function initializes a csm_coordinates structure. This function 
  should be called immediately after a variable of the type 
  csm_coordinates was created.
  \param c pointer to the csm_coordinates array
*/
static void init_coordinates(csm_coordinates *c)
  { 
    c->a_last=-1;
    c->b_last=-1;
    c->x_last=-1;
    c->ret_last=-1;
    c->coordinate= NULL;
    c->no_of_elements=0;
  }

/*! \internal \brief initialize a csm_coordinates structure [static]

  This function allocates memory for a 
  csm_coordinates structure. 
  \param c pointer to the csm_coordinates structure
  \param elements number of elements in the structure
  \return returns CSM_FALSE in case of an error, CSM_TRUE otherwise
*/
static csm_bool alloc_coordinates(csm_coordinates *c, int elements)
  { int i;
    csm_coordinate *co;
  
    if (c->no_of_elements=0) /* 1st time memory allocation */
      c->coordinate= malloc(sizeof(csm_coordinate)*elements);
    else  
      c->coordinate= realloc(c->coordinate,
                             sizeof(csm_coordinate)*elements);
			     
    if (c->coordinate==NULL) /* allocation error */
      { DBG_MSG_PRINTF2("error in IDCP:csm:alloc_coordinates line %d,\n"
                	"allocation failed!\n", __LINE__);
	init_coordinates(c);
	return(CSM_FALSE);
      };

    for(i=c->no_of_elements, co= (c->coordinate + c->no_of_elements); 
        i<elements; 
	i++, co++)
      { co->value=0;
	co->index=i;
      };
	  
    c->no_of_elements= elements;
    return(CSM_TRUE);
  }    

/*! \internal \brief resize a csm_coordinates structure [static]

  This function resizes a csm_coordinates structure. This means that
  the size of allocated memory is adapted to a new, different number
  of elements. Note that the existing data is not changed. If additional
  memory is allocated, it is initialized with zeroes. 
  \param c pointer to the csm_coordinates structure
  \param elements new number of elements in the structure
  \return returns CSM_FALSE in case of an error, CSM_TRUE otherwise
*/
static csm_bool resize_coordinates(csm_coordinates *c, int elements)
  { if (elements==c->no_of_elements)
      return(CSM_TRUE);
    
    c->no_of_elements= elements;
    if (NULL== (c->coordinate= realloc(c->coordinate,
                                       sizeof(csm_coordinate)*elements)))
      { DBG_MSG_PRINTF2("error in IDCP:csm:resize_coordinates line %d,\n" \
                        "realloc failed!\n", __LINE__);
        return(CSM_FALSE);
      };
    return(CSM_TRUE);
  }    

/*! \internal 
    \brief re-initialize a csm_coordinates structure [static]

  This re-initializes a csm_coordinates structure. This function
  is similar to init_coordinates, but already allocated memory is
  freed before the structure is filled with it's default values.   
  \param c pointer to the csm_coordinates array
*/
static void reinit_coordinates(csm_coordinates *c)
  { if (c->no_of_elements==0)
      return;
    
    free(c->coordinate);
    init_coordinates(c);
  }    

/*! \internal \brief initialize a matrix of double values [static]

  This function allocates an array of doubles in a way that it
  has enough room to hold all values of a rows*columns matrix.
  \param z address of pointer to array of double-numbers (output)
  \param rows number of rows 
  \param columns number of columns 
  \return returns CSM_FALSE in case of an error, CSM_TRUE otherwise
*/
static csm_bool init_matrix(double **z, int rows, int columns)
  { int i;
    double *zp;
    
    if (NULL== (zp= malloc((sizeof(double))*rows*columns)))
      { DBG_MSG_PRINTF2("error in IDCP:csm:init_matrix line %d,\n" \
                        "malloc failed!\n", __LINE__);
        return(CSM_FALSE);
      };
    *z = zp;
    for (i= rows*columns; i>0; i--, *(zp++)=0);
    return(CSM_TRUE);
  }  

        /*            x-compare for qsort             */

/*! \internal 
    \brief compare two values within a csm_coordinate structure [static]

  This function compares two values within a cms_coordinate structure.
  It is needed in order to apply quicksort.
  \param t1 this is the pointer to the first value. It should be of the
            type csm_coordinate*.
  \param t2 this is the pointer to the second value. It should be of the
            type csm_coordinate*.
  \return returns -1 if *t1 < *t2, 0 if *t1 = *t2, 1 if *t1 > *t2
*/
static int coordinate_cmp(const void *t1, const void *t2)
  { double x1= ((csm_coordinate *)(t1))->value;
    double x2= ((csm_coordinate *)(t2))->value;
    
    if (x1 < x2)
      return(-1);
    if (x1 > x2)
      return( 1);
    return(0);
  }

        /*              coordinate-sort               */

static void coordinate_sort( csm_coordinates *coords)
  {
    qsort( coords->coordinate, coords->no_of_elements, 
           sizeof(csm_coordinate), coordinate_cmp);
  }

static void coordinate_update_backlinks( csm_coordinates *coords1,
				         csm_coordinates *coords2)
  { int i,l;
    csm_coordinate *c,*d;
    
    l= coords1->no_of_elements; 
    for (i=0, c= coords1->coordinate, d= coords2->coordinate; i<l; i++, c++)
      { d[c->index].index = i; }; 
  }

        /*      lookup an index in coordinates        */

static int coordinate_lookup_index(csm_coordinates *coords,double x, 
				   int *a, int *b)
  /* 2: x was found, it is returned in a
     1: x was not found, but a and b define the closest interval around x
     0: x was not found at it is outside the closest interval a and b
    -1: error
  */
  /* the table should be x-sorted */
  { int i, ia, ib;
    double xt;
    csm_coordinate *c= coords->coordinate;
    
    if (coords->a_last==-1)
      { coords->a_last=0;
        coords->b_last=coords->no_of_elements-1;
      }	
    else
      { if (x==coords->x_last)
          { *a= coords->a_last; *b= coords->b_last; return(coords->ret_last); };
      };
	 	  
    *a= 0; *b=0;
    if (coords->no_of_elements<2)
      { if (coords->no_of_elements<1)
          { 
            DBG_MSG_PRINTF2("error in IDCP:csm:coordinate_lookup_index %d,\n" \
                            "table has less than 1 element!\n", __LINE__);
            return(-1);
           };
        coords->a_last= 0; coords->b_last= 0; coords->x_last= x; 
	if (x== c[0].value)
	  return( coords->ret_last= 2 ); 
	return( coords->ret_last= 0 );
      };
    ia= 0;
    ib= coords->no_of_elements-1;
    
    if (c[ia].value>=x) /* left of area */
      { if (c[ia].value==x) /* exact match */
          { coords->a_last= *a= ia; 
	    coords->b_last= *b= ia; 
	    coords->x_last= x;
	    return( coords->ret_last=2 );
	  };      
        coords->a_last= *a= ia; 
        coords->b_last= *b= (ia+1); 
	coords->x_last= x;
	return( coords->ret_last=0 );
      };
    if (c[ib].value<=x) /* right of area */
      { if (c[ib].value==x) /* exact match */
          { coords->a_last= *a= ib; 
	    coords->b_last= *b= ib; 
	    coords->x_last= x;
	    return( coords->ret_last=2 ); 
	  };      
        coords->a_last= *a= (ib-1); 
        coords->b_last= *b= ib; 
        coords->x_last= x;
        return( coords->ret_last=0 ); 
      };

    /* search acceleration for consecutive values of x: */
    i= (coords->a_last)-1;
    i= (i<ia) ? ia : i;
    xt= c[i].value;
    if (xt==x)
      { coords->a_last= *a= i; 
        coords->b_last= *b= i; 
        coords->x_last= x;
	return( coords->ret_last=2 ); 
      };
    if (xt<x)
      ia= i;
    else
      ib= i;
    i= (coords->b_last)+1;
    i= (i>ib) ? ib : i;
    xt= c[i].value;
    if (xt==x)
      { coords->a_last= *a= i; 
        coords->b_last= *b= i; 
        coords->x_last= x;
	return( coords->ret_last=2 ); 
      };
    if (xt<x)
      ia= i;
    else
      ib= i;
    
    for (; (ib>ia+1); )
      { i= (ib+ia)/2;
        xt= c[i].value;
        if (xt<x)
          ia= i;
        else
          if (xt>x)
            ib= i;
          else  /* exact match */
            { coords->a_last= *a= i; 
              coords->b_last= *b= i; 
              coords->x_last= x;
              return( coords->ret_last=2 ); 
            };
      };
    coords->a_last= *a= ia; 
    coords->b_last= *b= ib; 
    coords->x_last= x;
    return( coords->ret_last=1 );
  }       
         
    /*----------------------------------------------------*/
    /*     utilities for csm_linear_function (private)    */
    /*----------------------------------------------------*/

        /*               calculation                  */

/*! \internal \brief compute y from a given x [static]

  This function computes y when x is given for a linear function.
  A linear function is a 
  \param p pointer to the linear function structure
  \param x the value of x 
*/
static double linear_get_y(csm_linear_function *p, double x)
  { return( (p->a) + (p->b)*x ); }

/*! \internal \brief compute x from a given y [static]

  This function computes x when y is given for a linear function
  \param p pointer to the linear function structure
  \param y the value of y 
*/
static double linear_get_x(csm_linear_function *p, double y)
  { return( (y - (p->a))/(p->b) ); }

/*! \internal \brief compute delta-y from a given delta-x [static]

  This function computes delta-y when delta-x is given for a linear function
  \param p pointer to the linear function structure
  \param x the value of x 
*/
static double linear_delta_get_y(csm_linear_function *p, double x)
  { return( (p->b)*x ); }

/*! \internal \brief compute delta-x from a given delta-y [static]

  This function computes delta-x when delta-y is given for a linear function
  \param p pointer to the linear function structure
  \param y the value of y 
*/
static double linear_delta_get_x(csm_linear_function *p, double y)
  { return( y/(p->b) ); }

    /*----------------------------------------------------*/
    /*    utilities for csm_1d_functiontable (private)    */
    /*----------------------------------------------------*/

        /*              initialization                */

static void init_1d_functiontable(csm_1d_functiontable *ft)
  { init_coordinates(&(ft->x));
    init_coordinates(&(ft->y));
  }

static void reinit_1d_functiontable(csm_1d_functiontable *ft)
  { reinit_coordinates(&(ft->x));
    reinit_coordinates(&(ft->y));
  }
  
        /*        lookup a value in the table         */

static double lookup_1d_functiontable(csm_1d_functiontable *ft, double x,
				     csm_bool invert)
  { int res,a,b;
    double xa,xb,ya,yb;
    csm_coordinate c;
    csm_coordinates *xcoords;				       
    csm_coordinates *ycoords;

    if (!invert)
      { xcoords= &(ft->x);				       
        ycoords= &(ft->y);
      }
    else
      { xcoords= &(ft->y);				       
        ycoords= &(ft->x);
      };
      
    res= coordinate_lookup_index(xcoords, x, &a, &b);


    if (res==-1) /* error */
      return(0); 
    if (res== 2) /* exact match */
      { c= (xcoords->coordinate)[a];
        return( (ycoords->coordinate)[c.index].value );
      };    
    
    c= (xcoords->coordinate)[a];
    xa= c.value;
    ya= (ycoords->coordinate)[c.index].value;
    
    c= (xcoords->coordinate)[b];
    xb= c.value;
    yb= (ycoords->coordinate)[c.index].value;
    
    /* y= ya + (x-xa)/(xb-xa)*(yb-ya) */
    return( ya + (x-xa)/(xb-xa) * (yb-ya) );
  }

    /*----------------------------------------------------*/
    /*    utilities for csm_2d_functiontable (private)    */
    /*----------------------------------------------------*/

        /*              initialization                */

static void init_2d_functiontable(csm_2d_functiontable *ft)
  { init_coordinates(&(ft->x));
    init_coordinates(&(ft->y));
  }

static void reinit_2d_functiontable(csm_2d_functiontable *ft)
  { reinit_coordinates(&(ft->x));
    reinit_coordinates(&(ft->y));
    if (ft->z!=NULL)
      free(ft->z);
    ft->z= NULL;
  }

        /*       lookup a value in the xy-table       */

static double lookup_2d_functiontable(csm_2d_functiontable *ft, 
				      double x, double y)
  { int res,xia,xib,yia,yib;
    double xa,xb,ya,yb;
    double zaa,zab,zba,zbb;
    double *z= ft->z;
    int no_of_columns;

    csm_coordinate cxa,cxb,cya,cyb;
    csm_coordinates *xcoords= &(ft->x);				       
    csm_coordinates *ycoords= &(ft->y);

    no_of_columns= ycoords->no_of_elements;

    res= coordinate_lookup_index(xcoords, x, &xia, &xib);
    if (res==-1) /* error */
      return(0); 

    res= coordinate_lookup_index(ycoords, y, &yia, &yib);
    if (res==-1) /* error */
      return(0); 

    /* res==2 with exact match is currently not treated separately */

    cxa= (xcoords->coordinate)[xia];
    cxb= (xcoords->coordinate)[xib];
    cya= (ycoords->coordinate)[yia];
    cyb= (ycoords->coordinate)[yib];
    
    xa= cxa.value; /* with exact match, xa==xb or ya==yb is possible */
    xb= cxb.value;
    ya= cya.value;
    yb= cyb.value;
    
    zaa= z[ cxa.index * no_of_columns + cya.index ];
    zab= z[ cxa.index * no_of_columns + cyb.index ];
    zba= z[ cxb.index * no_of_columns + cya.index ];
    zbb= z[ cxb.index * no_of_columns + cyb.index ];

#if 0
printf("x:%f y:%f xa:%f xb:%f ya:%f yb:%f\n",x,y,xa,xb,ya,yb);
printf("zaa:%f zab:%f zba:%f zbb:%f\n",zaa,zab,zba,zbb);
#endif
#if 1
    { double alpha = (xb==xa) ? 0 : (x-xa)/(xb-xa);
      double beta  = (yb==ya) ? 0 : (y-ya)/(yb-ya);
      if (alpha==0)               /* xb==xa ? */
        { if (beta==0)            /* yb==ya ? */   
	    return(zaa);
	  return( zaa+ beta *(zab-zaa) );
	};
      if (beta==0)                /* yb==ya ? */
          return( zaa+ alpha*(zba-zaa) );
    
#if 0
printf(" ret: %f\n",
       zaa + beta*(zab-zaa)+alpha*(zba-zaa+beta*(zbb-zba-zab+zaa))); 
#endif
      return( zaa + beta*(zab-zaa)+alpha*(zba-zaa+beta*(zbb-zba-zab+zaa)) );
    };
#endif    

#if 0
    { /* alternative according to formula given by J.Bahrdt */
      /* doesn't work when yb==ya or xb==xa */
      double delta= (yb-ya)*(xb-xa);
      double yb_  = yb - y;
      double xb_  = xb - x;
      double y_a  = y  - ya;
      double x_a  = x  - xa;
      return((zaa*yb_*xb_ + zab*y_a*xb_ + zba*yb_*x_a + zbb*y_a*x_a)/delta);
#endif

 }
  
    /*----------------------------------------------------*/
    /*@IL           generic functions (public)            */
    /*----------------------------------------------------*/

/*! \brief compute x from a given y 

  This function computes x when y is given. Note that is doesn't
  work for two-dimensional function tables. For these, the 
  function returns 0
  \param f pointer to the function object
  \param y the value of y 
  \return x
*/

/*@EX(1)*/
double csm_x(csm_function *f, double y)
  { csm_Function *func= (csm_Function *)f;
    
    if (func->on_hold)
      return(func->last);

    semTake(func->semaphore, WAIT_FOREVER);

    if (func->on_hold)
      { semGive(func->semaphore);
        return(func->last); 
      };

    switch (func->type)
      { case CSM_NOTHING:
               func->last= 0; 
	       break;
	case CSM_LINEAR:
	       func->last= linear_get_x(&(func->f.lf), y);
	       break;	
	case CSM_1D_TABLE:
	       func->last= lookup_1d_functiontable(&(func->f.tf_1),y, 
	       					   CSM_TRUE);
	       break;	
        default:
	       func->last=0;
	       break;
      };
    semGive(func->semaphore);	  	       
    return(func->last);
  }

/*! \brief compute y from a given x 

  This function computes y when x is given. Note that is doesn't
  work for two-dimensional function tables. For these, the 
  function returns 0
  \param f pointer to the function object
  \param x the value of x 
  \return y
*/

/*@EX(1)*/
double csm_y(csm_function *f, double x)
  { csm_Function *func= (csm_Function *)f;

    if (func->on_hold)
      return(func->last);

    semTake(func->semaphore, WAIT_FOREVER);

    if (func->on_hold)
      { semGive(func->semaphore);
        return(func->last); 
      };

    switch (func->type)
      { case CSM_NOTHING:
               func->last= 0; 
	       break;
	case CSM_LINEAR:
	       func->last= linear_get_y(&(func->f.lf), x);
	       break;	
	case CSM_1D_TABLE:
	       func->last= lookup_1d_functiontable(&(func->f.tf_1),x, 
	       					   CSM_FALSE);
	       break;	
        default:
	       func->last=0;
	       break;
      };  
    semGive(func->semaphore);	  	       
    return(func->last);
  }
  
/*! \brief compute delta-x from a given delta-y 

  This function computes a delta-x when a delta-y is given.
  Note that this function only works with linear functions.
  For all other types, this function
  returns 0.
  \param f pointer to the function object
  \param y the given delta-y
  \return delta-x
*/

/*@EX(1)*/
double csm_dx(csm_function *f, double y)
  { csm_Function *func= (csm_Function *)f;

    if (func->on_hold)
      return(func->last);

    semTake(func->semaphore, WAIT_FOREVER);

    if (func->on_hold)
      { semGive(func->semaphore);
        return(func->last); 
      };

    if (func->type==CSM_LINEAR)
      func->last= linear_delta_get_x(&(func->f.lf), y);
    else
      func->last=0;
      
    semGive(func->semaphore);
    return(func->last);
  }

/*! \brief compute delta-y from a given delta-x 

  This function computes a delta-y when a delta-x is given.
  Note that this function only works with linear functions.
  For all other types, this function
  returns 0.
  \param f pointer to the function object
  \param x the given delta-x
  \return delta-y
*/

/*@EX(1)*/
double csm_dy(csm_function *f, double x)
  { csm_Function *func= (csm_Function *)f;

    if (func->on_hold)
      return(func->last);

    semTake(func->semaphore, WAIT_FOREVER);

    if (func->on_hold)
      { semGive(func->semaphore);
        return(func->last); 
      };

    if (func->type==CSM_LINEAR)
      func->last= linear_delta_get_y(&(func->f.lf), x);
    else
      func->last=0;
      
    semGive(func->semaphore);
    return(func->last);
  }

/*! \brief compute z from a given x and y 

  This function computes z for two-dimensional function table 
  when x and y are given. For all other function types, it returns
  0.
  \param f pointer to the function object
  \param x the value of x
  \param y the value of y 
  \return z
*/

/*@EX(1)*/
double csm_z(csm_function *f, double x, double y)
  { csm_Function *func= (csm_Function *)f;

    if (func->on_hold)
      return(func->last);

    semTake(func->semaphore, WAIT_FOREVER);
  
    if (func->on_hold)
      { semGive(func->semaphore);
        return(func->last); 
      };

    if (func->type==CSM_2D_TABLE)
      func->last= lookup_2d_functiontable(&(func->f.tf_2), x, y); 
    else
      func->last=0;

    semGive(func->semaphore);
    return(func->last);
  }
  
    /*----------------------------------------------------*/
    /*@IL                initialization                   */
    /*----------------------------------------------------*/

      /*   initialize a csm_Function object (private)   */

static void reinit_function(csm_function *f)
  { csm_Function *func= (csm_Function *)f;

    if (func->type==CSM_NOTHING)
      { return; };
    if (func->type==CSM_1D_TABLE)
      reinit_1d_functiontable( &(func->f.tf_1) );
    if (func->type==CSM_2D_TABLE)
      reinit_2d_functiontable( &(func->f.tf_2) );
    
    /* @@@@ CAUTION: currently there is no locking,
       no test wether another process is currently 
       accessing the structure */
    func->type= CSM_NOTHING;
  }
     
      /*  scan a string for a list of doubles (private) */

static int strdoublescan(char *st, double *d, int no_of_cols)
  /* returns the number of numbers found in the line, d may be null,
     in this case, the numbers are only counted,
     if a line contains not enough numbers, (unequal no_of_cols) the missing
     ones are set to 0 */
  /* reads over leading whitespaces */
  /* uses strtok ! */
  { int i;
    char *p= st;      /* anything different from NULL */
    char buf[1024];
    char *str= buf;
    double *dptr;
    
    if ((no_of_cols<=0)||(no_of_cols>1000))
      return(0);
    for( ;NULL!=strchr( " \t\r\n", *st); st++); 
    if (*st==0)
      return(0);
    strncpy(buf, st, 1023); buf[1023]=0; 
    if (d!=NULL)
      for(dptr= d, i=0; i<no_of_cols; i++, *(dptr++)=0 ); 
    for(p= str, dptr= d, i=0; i<no_of_cols; i++, dptr++)
      { 
        if (NULL== (p= strtok(str," \t\r\n")))
	  return(i); 
        str= NULL; 
	if (d!=NULL)
          { if (1!=sscanf(p,"%lf",dptr))
	      return(0); /* a kind of fatal error */
          };
      };
    return(no_of_cols);  /* everything that was expected was found */
  }  
    
      /*          define a function (public)            */

/*! \brief define a linear function

  This function initializes a \ref csm_function structure as
  a linear function (y= a+b*x). 
  \param f pointer to the function object
  \param a offset of y= a+b*x
  \param b multiplier of y= a+b*x
*/

/*@EX(1)*/
void csm_def_linear(csm_function *f, double a, double b)
  /* y= a+b*x */
  { 
    csm_Function *func= (csm_Function *)f;

    semTake(func->semaphore, WAIT_FOREVER);
    func->on_hold= CSM_TRUE;
    semGive(func->semaphore);

    reinit_function(f);
  
    func->type= CSM_LINEAR;
    (func->f.lf).a= a;
    (func->f.lf).b= b;

    func->on_hold= CSM_FALSE;
  }
 
/*! \brief re-define the offset of a linear function

  This function re-defines the offset-factor of 
  a linear function (y= a+b*x). 
  \param f pointer to the function object
  \param a offset of y= a+b*x
  \return returns CSM_FALSE in case of an error, CSM_TRUE otherwise
*/

/*@EX(1)*/
csm_bool csm_def_linear_offset(csm_function *f, double a)
  { csm_Function *func= (csm_Function *)f;

    if (func->type!=CSM_LINEAR)
      { DBG_MSG_PRINTF2("error in IDCP:csm_def_linear_offset line %d,\n" \
                        "not a linear function!\n", __LINE__);
        return(CSM_FALSE);
      };

    semTake(func->semaphore, WAIT_FOREVER);
    func->f.lf.a= a;  
    semGive(func->semaphore);

    return(CSM_TRUE);
  } 
   
/*! \brief read parameters of one-dimensional function table

  This function reads a one-dimensional function-table from a 
  file. 
  \param filename the name of the file
  \param fu pointer to the function object
  \return returns CSM_FALSE in case of an error, CSM_TRUE otherwise
*/

/*@EX(1)*/
csm_bool csm_read_1d_table(char *filename, csm_function *fu) 
/* if len==0, the table length is determined by counting the number of lines
   in the file from the current position to it's end */
  { char line[128];
    long pos;
    long i;
    csm_Function *func= (csm_Function *)fu;
    csm_1d_functiontable *ft= &(func->f.tf_1);
    csm_coordinate *xc, *yc;
    int len;
    FILE *f;
    char dummy;
    
    if (NULL==(f=fopen(filename,"r"))) /* vxworks doesn't accept "rt" */
      { DBG_MSG_PRINTF2("error in IDCP:csm_read_xytable line %d,\n" \
                        "file open error!\n", __LINE__);
        return(CSM_FALSE);
      };

    pos= ftell(f);
    for(len=0; NULL!=fgets(line, 127, f); len++);
    if (-1==fseek(f, pos, SEEK_SET))
      { fclose(f);                
        return(CSM_FALSE); 
      };
    
    semTake(func->semaphore, WAIT_FOREVER);
    func->on_hold= CSM_TRUE;
    semGive(func->semaphore);
    
    reinit_function(fu);

    init_1d_functiontable(ft);
    
    if (!alloc_coordinates(&(ft->x), len))
      { fclose(f);                
	reinit_function(fu);
        func->on_hold= CSM_FALSE;
	return(CSM_FALSE); 
      };

    if (!alloc_coordinates(&(ft->y), len))
      { fclose(f);                
	reinit_function(fu);
        func->on_hold= CSM_FALSE;
	return(CSM_FALSE); 
      };

    xc= (ft->x).coordinate;
    yc= (ft->y).coordinate;
    
    for(i=0;(len>0) && (NULL!=fgets(line, 127, f)); len--)
      { if (2!=sscanf(line, " %lf %lf %c", 
                      &(xc->value), &(yc->value), &dummy))
          { DBG_MSG_PRINTF4("warning[%s:%d]: the following line of the "
	           "data-file was not understood:\n%s\n", 
		   __FILE__,__LINE__,line);
	    continue;
          };
	xc->index= i; 
	yc->index= i;  
        i++, xc++,yc++;
      };
    if (!resize_coordinates(&(ft->x), i))
      { fclose(f);                
	reinit_function(fu);
        func->on_hold= CSM_FALSE;
        return(CSM_FALSE); 
      };
    if (!resize_coordinates(&(ft->y), i))
      { fclose(f);                
	reinit_function(fu);
        func->on_hold= CSM_FALSE;
        return(CSM_FALSE); 
      };
    fclose(f);
      
    func->type= CSM_1D_TABLE;

    coordinate_sort(&(ft->x));
    coordinate_update_backlinks(&(ft->x), &(ft->y));
    coordinate_sort(&(ft->y));
    coordinate_update_backlinks(&(ft->y), &(ft->x));
    func->on_hold= CSM_FALSE;
    return(CSM_TRUE);
  } 

/*! \brief read parameters of two-dimensional function table

  This function reads a two-dimensional function-table from a 
  file. 
  \param filename the name of the file
  \param fu pointer to the function object
  \return returns CSM_FALSE in case of an error, CSM_TRUE otherwise
*/

/*@EX(1)*/
csm_bool csm_read_2d_table(char *filename, csm_function *fu) 
/* file format:
    y1   y2   y3  ..
x1  z11  z12  z13 ...
x2  z21  z22  z23 ...
.    .    .    .
.    .    .    .
.    .    .    .
*/
  { char line[1024];
    long pos;
    long i,j,lines;
    double *buffer;
    double *zptr;
    csm_Function *func= (csm_Function *)fu;
    csm_2d_functiontable *ft= &(func->f.tf_2);
    csm_coordinate *xc, *yc;
    FILE *f;
    int columns,rows;

    if (NULL==(f=fopen(filename,"r"))) /* vxworks doesn't accept "rt" */
      { DBG_MSG_PRINTF2("error in IDCP:csm_read_xytable line %d,\n" \
                        "file open error!\n", __LINE__);
        return(CSM_FALSE);
      };
    
    if (NULL==fgets(line, 1024, f))
      { fclose(f);                
        return(CSM_FALSE); 
      };
       
    columns= strdoublescan(line, NULL, 512);
    if (columns<1)
      { fclose(f);  
        return(CSM_FALSE);
      };	
    if (NULL==(buffer= malloc(sizeof(double)*(columns+1))))
      { DBG_MSG_PRINTF2("MALLOC FAILED in file %s\n",__FILE__);
        fclose(f); 
	return(CSM_FALSE);
      };
    if (columns!= (i= strdoublescan(line, buffer, columns)))
      { DBG_MSG_PRINTF3("unexpected err in line %d in file %s\n",
                        __LINE__,__FILE__);
        free(buffer);
	fclose(f); 
	return(CSM_FALSE);
      };
      
    semTake(func->semaphore, WAIT_FOREVER);
    func->on_hold= CSM_TRUE;
    semGive(func->semaphore);
    
    reinit_function(fu);
    init_2d_functiontable(ft);

    if (!alloc_coordinates(&(ft->y), columns))
      { free(buffer);
        reinit_function(fu);
	fclose(f); 
        func->on_hold= CSM_FALSE;
	return(CSM_FALSE);
      };

    yc= (ft->y).coordinate;
    
    /* copy y-values into the csm_table_function - structure */
    { double *r= buffer;
      csm_coordinate *c= yc;
      for(i=0; i< columns; i++, c++, r++ )
        { c->value= *r;
	  c->index= i;
	};
    };  	  

    pos= ftell(f);
    for(lines=0; NULL!=fgets(line, 1024, f); lines++);
    if (-1==fseek(f, pos, SEEK_SET))
      { free(buffer);
        reinit_function(fu);
	fclose(f); 
        func->on_hold= CSM_FALSE;
	return(CSM_FALSE);
      };
    rows= lines;  
    
    if (!alloc_coordinates(&(ft->x), rows))
      { free(buffer);
        reinit_function(fu);
	fclose(f); 
        func->on_hold= CSM_FALSE;
	return(CSM_FALSE);
      };

    xc= (ft->x).coordinate;
    
    if (!init_matrix(&(ft->z), rows, columns))
      { free(buffer);
        reinit_function(fu);
	fclose(f); 
        func->on_hold= CSM_FALSE;
	return(CSM_FALSE);
      };

    zptr= ft->z;

    for(i=0; (lines>0) && (NULL!=fgets(line, 1024, f)); lines--)
      { 
        if (columns+1 != strdoublescan(line, buffer, columns+1))
          { DBG_MSG_PRINTF4("warning[%s:%d]: the following line of the "
	                    "data-file was not understood:\n%s\n", 
			    __FILE__,__LINE__,line);
	    continue;
	  };  	   

        xc->value= buffer[0];
	xc->index= i;

	zptr= &((ft->z)[i*columns]);
	for (j=0; j<columns; j++)  
	  zptr[j]= buffer[j+1]; 

	i++, xc++;  
      };
    
    rows= i;
    if (!resize_coordinates(&(ft->x), rows))
      { free(buffer);
        reinit_function(fu);
	fclose(f); 
        func->on_hold= CSM_FALSE;
	return(CSM_FALSE);
      };

    fclose(f); 

    func->type= CSM_2D_TABLE;

    coordinate_sort(&(ft->x));
    coordinate_sort(&(ft->y));
    func->on_hold= CSM_FALSE;
    return(CSM_TRUE);
  } 

  
      /*         initialize the module (public)         */

/*! \brief initialize the module

  This function should be called once to initialize the
  module.
*/

/*@EX(1)*/
void csm_init(void)
  { 
    if (initialized)
      return;
    initialized= CSM_TRUE;
#if USE_DBG
    csm_dbg_init(); 
#endif  
  }
  
      /*          create a new function object          */


/*! \brief create a new function object

  This function returns a pointer to a new function object. The object
  is initialized as an empty function
  \return returns NULL in case of an error. Otherwise the
          pointer to the csm_function is returned
*/

/*@EX(1)*/
csm_function *csm_new_function(void)
  { csm_Function *f= malloc(sizeof(csm_Function));
  
    if (f==NULL)
      return(NULL);
      
    f->type= CSM_NOTHING;
    f->last= 0;
    f->on_hold= CSM_FALSE;
    f->semaphore= semMCreate(SEM_Q_FIFO);
    if (f->semaphore==NULL)
      { free(f);
        return(NULL);
      };
     
    return((csm_function *)f);
  }
  
    /*----------------------------------------------------*/
    /*@IL              debugging (public)                 */
    /*----------------------------------------------------*/

void csm_pr_coordinates(csm_coordinates *c)
  { int i;
    printf("no . of elements: %d\n", c->no_of_elements);
    if (c->a_last==-1)
      printf("(still in initial-state)\n"); 
    else
      { printf("a_last:%d  b_last:%d ret_last:%d  x_last:%f\n", 
        c->a_last, c->b_last, c->ret_last, c->x_last); 
      };
    for(i=0; i< c->no_of_elements; i++)
      printf(" [%03d]: %12f --> %3d\n", 
             i,(c->coordinate)[i].value,(c->coordinate)[i].index); 
  }

void csm_pr_1d_table_elm(csm_1d_functiontable *t, int index)
  { printf("x: %15f   y: %15f\n", 
           ((t->x).coordinate)[index].value, 
           ((t->y).coordinate)[index].value); 
  }

void csm_pr_linear(csm_linear_function *lf)
  { printf("a:%f  b:%f  (y=a+b*x)\n", lf->a, lf->b); } 
  
void csm_pr_1d_table(csm_1d_functiontable *tf)
  { int i;
    int l= (tf->x).no_of_elements;
  
    if (tf->x.a_last==-1)
      printf("x-coord is still in initial-state\n"); 
    else
      printf("x-a_last:%d  x-b_last:%d\n", tf->x.a_last, tf->x.b_last); 
    if (tf->y.a_last==-1)
      printf("x-coord is still in initial-state\n"); 
    else
      printf("y-a_last:%d  y-b_last:%d\n", tf->y.a_last, tf->y.b_last); 
    printf("number of elements: %d\n", l);
    for(i=0; i<l; i++)
      csm_pr_1d_table_elm( tf, i);
  }

void csm_pr_2d_table(csm_2d_functiontable *ft)
  { int i,j;
    int rows   = ft->x.no_of_elements;
    int columns= ft->y.no_of_elements;
    double *z= ft->z;
    csm_coordinate *xc= ft->x.coordinate;
    csm_coordinate *yc= ft->y.coordinate;

    printf("rows:%d  columns:%d\n", 
           rows, columns);
    printf("%18s"," ");
    for (j=0; j<columns; j++)
      printf("%15f | ", yc[j].value); 
    printf("\n");
    printf("------------------");
    for (j=0; j<columns; j++)
      { printf("---------------"); };
    for(i=0; i<rows; i++)
      { printf("%15f | ", xc[i].value); 
        for (j=0; j<columns; j++)
          printf("%15f | ", z[columns*i+j]); 
	printf("\n");
      };   	   
  }
  
/*@EX(1)*/
void csm_pr_func(csm_function *f)
  { csm_Function *func= (csm_Function *)f;

    if (func->on_hold)
      printf("function is on hold!\n");
    else
      printf("function is operational\n");
    
    printf("Last calculated value: %f\n", func->last);
    
    printf("function type: \n");
    switch( func->type)
      { case CSM_NOTHING: 
             printf("CSM_NOTHING\n"); 
             break;
        case CSM_LINEAR:  
             printf("CSM_LINEAR\n"); 
             csm_pr_linear(&(func->f.lf));
             break;
        case CSM_1D_TABLE:   
             printf("CSM_1D TABLE\n"); 
             printf("normal function:\n");
             csm_pr_1d_table(&(func->f.tf_1));
             break;
	case CSM_2D_TABLE:
             printf("CSM_2D_TABLE\n");
	     csm_pr_2d_table(&(func->f.tf_2));
             break;
        default: 
             printf("%d (unknown)\n", func->type);
             break;
      };
  }
        

/*@EM("#endif\n") */

#if 0
csm_free implementieren.

strukturen cachen (anhand des Filenamens)

Filenamen übergeben

evtl die csm-struktur direkt allozieren

reload von Files implementieren
  im Fehlerfall die alten Daten behalten


#endif
