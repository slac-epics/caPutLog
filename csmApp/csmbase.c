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
/* version-number of module: 0.96                             */
/* author: Goetz Pfeiffer                                     */
/* last modification date: 2002-03-05                         */
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
    
/* Author:              $Author: franksen $
   check-in date:       $Date: 2004/03/19 13:32:42 $
   locker of this file: $Locker:  $
   Revision:            $Revision: 1.2 $
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
*/

    /*----------------------------------------------------*/
    /*		        General Comments                  */
    /*----------------------------------------------------*/
    
/* This is the calculation-support module. It supports the conversion from
   position-increments to gap distances and from gap-distances to 
   energy-values. The calculation-support module is a part of IDCP. */

/*@ITI________________________________________________________*/
/*                      general Defines                       */
/*@ET_________________________________________________________*/

/*@EM("\n#ifndef __CSMBASE_H\n#define __CSMBASE_H\n") */

#define _INCLUDE_POSIX_SOURCE

/* the following macros affect the debug (dbg) module: */
#define DBG_ASRT_COMP 0 
/* 1: compile assertions */

#define DBG_MSG_COMP 0 
/* 1: compile debug_messages */

#define DBG_TRC_COMP  0
/* 1: compile trace-messages */

#define DBG_LOCAL_COMP 1
/* use local switch-variables for the dbg module */

#define DBG_ASYNC_COMP 0
/* use async. I/O via message passing */

/* the following defines are for sci-debugging, they do not influence
   any header-files but are placed here since they are usually changed 
   together with the above macros.*/
   
#define CSMBASE_OUT_FILE "stdout"
/* file for debug-outputs */
#define CSMBASE_TRC_LEVEL 0
/* standard trace-level */ 
/* level 6: dense traces for debugging
   level 5: enter/exit tracing */
#define CSMBASE_FLUSHMODE 1
/* 0, 1 and 2 are allowed */


/*____________________________________________________________*/
/*			 Include-Files			      */
/*____________________________________________________________*/

#include <basic.h> /*@IL*/


#ifdef B_VXWORKS
#include <vxWorks.h>
#endif

/* include files for epics subroutine-support: */
#include <dbDefs.h>
#include <subRecord.h>
#include <dbCommon.h>
#include <recSup.h>
/* ... until here ! */


#include <dbg.h>     

#include <stdlib.h>
#include <stdio.h>    /*@IL*/
#include <string.h>

/*@ITI________________________________________________________*/
/*                          Types                             */
/*@ET_________________________________________________________*/

      /*................................................*/
      /*@IL           the coordinate type               */
      /*................................................*/

        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        /*                  public                    */
        /*@IT\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

typedef struct
  { double value;
    int    index;
  } csm_coordinate;
  
typedef struct
  { csm_coordinate *coordinate;
    int            no_of_elements;
    int            a_last;       /* last left index */
    int            b_last;       /* last right index */
    int		   ret_last;     /* last return-val. of lookup function */
    double         x_last;       /* last x that was looked up */
    boolean        initial;      /* TRUE after initialization */
  } csm_coordinates;   

      /*@ETI............................................*/
      /*@IL         the table-function type             */
      /*................................................*/

        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        /*                  public                    */
        /*@IT\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        
typedef struct
  { csm_coordinates x;
    csm_coordinates y;
  } csm_1d_functiontable; 
  
typedef struct
  { csm_coordinates x;
    csm_coordinates y;
    double          *z;
  } csm_2d_functiontable; 
  
      /*@ETI............................................*/
      /*@IL       the linconv-parameter type            */
      /*................................................*/

        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        /*                  public                    */
        /*@IT\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

typedef struct
  { double a;
    double b; /* y = a + b*x */
  } csm_linear_function;
  
      /*@ETI............................................*/
      /*@IL            the function-type                */
      /*................................................*/

        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        /*                  public                    */
        /*@IT\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

typedef enum { CSM_NOTHING, CSM_LINEAR, 
              CSM_1D_TABLE, CSM_2D_TABLE} csm_func_type;        

typedef struct 
  { csm_func_type type;
    union { csm_linear_function    lf; 
            csm_1d_functiontable   tf_1; /* forward function y(x) */
            csm_2d_functiontable   tf_2; /* xy */
	  } f;  
  } csm_function;

/*____________________________________________________________*/
/*                        Functions                           */
/*@ET_________________________________________________________*/


    /*----------------------------------------------------*/
    /*                 debug - managment                  */
    /*----------------------------------------------------*/

        /*/////////////////////////////////////////// */
        /*                 private                    */
        /*/////////////////////////////////////////// */

static void csmbase_dbg_init(void)
  { static boolean csmbase_init= FALSE;

    if (csmbase_init)
      return;
    csmbase_init= TRUE;
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

    /*----------------------------------------------------*/
    /*                 linear conversion                  */
    /*----------------------------------------------------*/

        /*/////////////////////////////////////////// */
        /*                  private                   */
        /*/////////////////////////////////////////// */

static double linear_get_y(csm_linear_function *p, double x)
  { return( (p->a) + (p->b)*x ); }

static double linear_get_x(csm_linear_function *p, double y)
  { return( (y - (p->a))/(p->b) ); }

static double linear_delta_get_y(csm_linear_function *p, double x)
  { return( (p->b)*x ); }

static double linear_delta_get_x(csm_linear_function *p, double y)
  { return( y/(p->b) ); }
 
    /*----------------------------------------------------*/
    /*                  table-handling                    */
    /*----------------------------------------------------*/

        /*::::::::::::::::::::::::::::::::::::::::::::*/
        /*              initialization                */
        /*::::::::::::::::::::::::::::::::::::::::::::*/

static boolean init_coordinates(csm_coordinates *c, int elements)
  { int i;
    csm_coordinate *co;
  
    c->no_of_elements= elements;
    c->initial= TRUE;
    if (NULL== (c->coordinate= malloc(sizeof(csm_coordinate)*elements)))
      { DBG_MSG_PRINTF2("error in IDCP:csm:init_coordinates line %d,\n" \
                        "malloc failed!\n", __LINE__)
        return(FALSE);
      };
    for(i=elements, co= c->coordinate; i>0; i--, (*(co++)).value= 0);
    return(TRUE);
  }    

static boolean resize_coordinates(csm_coordinates *c, int elements)
  { if (elements==c->no_of_elements)
      return(TRUE);
    
    c->no_of_elements= elements;
    if (NULL== (c->coordinate= realloc(c->coordinate,
                                       sizeof(csm_coordinate)*elements)))
      { DBG_MSG_PRINTF2("error in IDCP:csm:resize_coordinates line %d,\n" \
                        "realloc failed!\n", __LINE__)
        return(FALSE);
      };
    return(TRUE);
  }    

static boolean init_matrix(double **z, int rows, int columns)
  { int i;
    double *zp;
    
    if (NULL== (zp= malloc((sizeof(double))*rows*columns)))
      { DBG_MSG_PRINTF2("error in IDCP:csm:init_matrix line %d,\n" \
                        "malloc failed!\n", __LINE__)
        return(FALSE);
      };
    *z = zp;
    for (i= rows*columns; i>0; i--, *(zp++)=0);
    return(TRUE);
  }  

        /*::::::::::::::::::::::::::::::::::::::::::::*/
        /*            x-compare for qsort             */
        /*::::::::::::::::::::::::::::::::::::::::::::*/

        /*/////////////////////////////////////////// */
        /*                  private                   */
        /*/////////////////////////////////////////// */

static int coordinate_cmp(const void *t1, const void *t2)
  { double x1= ((csm_coordinate *)(t1))->value;
    double x2= ((csm_coordinate *)(t2))->value;
    
    if (x1 < x2)
      return(-1);
    if (x1 > x2)
      return( 1);
    return(0);
  }

        /*::::::::::::::::::::::::::::::::::::::::::::*/
        /*              coordinate-sort               */
        /*::::::::::::::::::::::::::::::::::::::::::::*/

        /*/////////////////////////////////////////// */
        /*                  private                   */
        /*/////////////////////////////////////////// */

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

        /*::::::::::::::::::::::::::::::::::::::::::::*/
        /*      lookup an index in coordinates        */
        /*::::::::::::::::::::::::::::::::::::::::::::*/

        /*/////////////////////////////////////////// */
        /*                  private                   */
        /*/////////////////////////////////////////// */

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
    
    if (coords->initial)
      { coords->initial= FALSE;
        coords->a_last=0;
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
         
        /*::::::::::::::::::::::::::::::::::::::::::::*/
        /*       lookup an y-value in the table       */
        /*::::::::::::::::::::::::::::::::::::::::::::*/

        /*/////////////////////////////////////////// */
        /*                  private                   */
        /*/////////////////////////////////////////// */

static double lookup_1d_functiontable(csm_1d_functiontable *ft, double x,
				     boolean invert)
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

        /*::::::::::::::::::::::::::::::::::::::::::::*/
        /*       lookup a value in the xy-table       */
        /*::::::::::::::::::::::::::::::::::::::::::::*/

        /*/////////////////////////////////////////// */
        /*                  private                   */
        /*/////////////////////////////////////////// */

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
    /*@IL               generic functions                 */
    /*----------------------------------------------------*/

        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        /*                  public                    */
        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*@EX(1)*/
double csm_x(csm_function *f, double y)
  { if (f->type==CSM_NOTHING)
      { return(0); };
    if (f->type==CSM_LINEAR)
      { return(linear_get_x(&(f->f.lf), y)); };
    if (f->type==CSM_1D_TABLE)
      { return( lookup_1d_functiontable(&(f->f.tf_1), y, TRUE) ); };
    return(0);
  }

/*@EX(1)*/
double csm_y(csm_function *f, double x)
  { if (f->type==CSM_NOTHING)
      { return(0); };
    if (f->type==CSM_LINEAR)
      { return(linear_get_y(&(f->f.lf), x)); };
    if (f->type==CSM_1D_TABLE)
      { return( lookup_1d_functiontable(&(f->f.tf_1), x, FALSE) ); };
    return(0);
  }
  
/*@EX(1)*/
double csm_dx(csm_function *f, double y)
  { if (f->type==CSM_LINEAR)
      { return(linear_delta_get_x(&(f->f.lf), y)); };
    return(0);
  }

/*@EX(1)*/
double csm_dy(csm_function *f, double x)
  { if (f->type==CSM_LINEAR)
      { return(linear_delta_get_y(&(f->f.lf), x)); };
    return(0);
  }

/*@EX(1)*/
double csm_z(csm_function *f, double x, double y)
  { if (f->type==CSM_2D_TABLE)
      { return( lookup_2d_functiontable(&(f->f.tf_2), x, y)); };
    return(0);
  }
  
    /*----------------------------------------------------*/
    /*@IL                initialization                   */
    /*----------------------------------------------------*/

        /*/////////////////////////////////////////// */
        /*                  private                   */
        /*/////////////////////////////////////////// */

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
    
        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        /*                  public                    */
        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*@EX(1)*/
void csm_def_linear(csm_function *f, double a, double b)
  /* y= a+b*x */
  { f->type= CSM_LINEAR;
    (f->f.lf).a= a;
    (f->f.lf).b= b;
  }
  
/*@EX(1)*/
boolean csm_read_linear(FILE *f, csm_function *func)
  { char line[128];
  
    if (NULL==fgets(line, 127, f))
      return(FALSE);
    if (2!=sscanf(line, " %lf %lf", &((func->f.lf).a), &((func->f.lf).b)))
      return(FALSE);
    return(TRUE);
  }
  
/*@EX(1)*/
boolean csm_read_table(FILE *f, csm_function *func, long len, double x_start)
/* if len==0, the table length is determined by counting the number of lines
   in the file from the current position to it's end */
  { char line[128];
    long pos;
    long i;
    csm_1d_functiontable *ft= &(func->f.tf_1);
    csm_coordinate *xc, *yc;
    
    func->type= CSM_1D_TABLE;
    if (len<=0)
      { pos= ftell(f);
        for(len=0; NULL!=fgets(line, 127, f); len++);
        if (-1==fseek(f, pos, SEEK_SET))
          return(FALSE);
      };
    
    if (!init_coordinates(&(ft->x), len))
      return(FALSE);
    
    if (!init_coordinates(&(ft->y), len))
      return(FALSE);
    
    xc= (ft->x).coordinate;
    yc= (ft->y).coordinate;
    
    for(i=0;(len>0) && (NULL!=fgets(line, 127, f)); len--)
      { if (2!=sscanf(line, " %lf %lf", &(xc->value), &(yc->value)))
          { printf("warning[%s:%d]: the following line of the data-file "
	           "was not understood:\n%s\n", __FILE__,__LINE__,line);
	    continue;
          };
	xc->index= i; 
	yc->index= i;  
        i++, xc++,yc++;
      };
    if (!resize_coordinates(&(ft->x), i))
      return(FALSE);
    if (!resize_coordinates(&(ft->y), i))
      return(FALSE);
      
    coordinate_sort(&(ft->x));
    coordinate_update_backlinks(&(ft->x), &(ft->y));
    coordinate_sort(&(ft->y));
    coordinate_update_backlinks(&(ft->y), &(ft->x));
    return(TRUE);
  } 

/*@EX(2)*/
boolean csm_read_xytable(FILE *f, csm_function *func, 
                         long rows, long columns) 
/* if rows==0, the table length is determined by counting the number of lines
   in the file from the current position to it's end */
/* if columns==0, the table length is determined by counting the items in the
   first line of the file*/
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
    csm_2d_functiontable *ft= &(func->f.tf_2);
    csm_coordinate *xc, *yc;

    func->type= CSM_2D_TABLE;
    if (NULL==fgets(line, 1024, f))
      return(FALSE); 
       
    if (columns<=0)
      columns= strdoublescan(line, NULL, 512);
    if (columns<1)
      return(FALSE);
    if (NULL==(buffer= malloc(sizeof(double)*(columns+1))))
      { DBG_MSG_PRINTF2("MALLOC FAILED in file %s\n",__FILE__);
        return(FALSE);
      };
    if (columns!= (i= strdoublescan(line, buffer, columns)))
      { printf("unexpected err in line %d in file %s\n",__LINE__,__FILE__);
        return(FALSE);
      };
      
    if (!init_coordinates(&(ft->y), columns))
      return(FALSE);

    yc= (ft->y).coordinate;
    
    /* copy y-values into the csm_table_function - structure */
    { double *r= buffer;
      csm_coordinate *c= yc;
      for(i=0; i< columns; i++, c++, r++ )
        { c->value= *r;
	  c->index= i;
	};
    };  	  

    if (rows<=0)
      { pos= ftell(f);
        for(lines=0; NULL!=fgets(line, 1024, f); lines++);
        if (-1==fseek(f, pos, SEEK_SET))
          return(FALSE);
	rows= lines;  
      }
    else
      lines= rows;

    if (!init_coordinates(&(ft->x), rows))
      return(FALSE);

    xc= (ft->x).coordinate;
    
    if (!init_matrix(&(ft->z), rows, columns))
      return(FALSE);

    zptr= ft->z;

    for(i=0; (lines>0) && (NULL!=fgets(line, 1024, f)); lines--)
      { 
        if (columns+1 != strdoublescan(line, buffer, columns+1))
          { printf("warning[%s:%d]: the following line of the data-file "
	           "was not understood:\n%s\n", __FILE__,__LINE__,line);
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
      return(FALSE);

    coordinate_sort(&(ft->x));
    coordinate_sort(&(ft->y));
    return(TRUE);
  } 

/*@EX(1)*/
void csmbase_init(void)
  { csmbase_dbg_init(); }
  
/*@EX(1)*/
void csmbase_func_init(csm_function *f)
  { f->type= CSM_NOTHING; }

    /*----------------------------------------------------*/
    /*@IL                  debugging                      */
    /*----------------------------------------------------*/

        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        /*                  public                    */
        /*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
        
void csm_pr_coordinates(csm_coordinates *c)
  { int i;
    printf("no . of elements: %d\n", c->no_of_elements);
    if (c->initial)
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
  
    if (tf->x.initial)
      printf("x-coord is still in initial-state\n"); 
    else
      printf("x-a_last:%d  x-b_last:%d\n", tf->x.a_last, tf->x.b_last); 
    if (tf->y.initial)
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
  
void csm_pr_func(csm_function *f)
  { printf("function type: \n");
    switch( f->type)
      { case CSM_NOTHING: 
             printf("CSM_NOTHING\n"); 
             break;
        case CSM_LINEAR:  
             printf("CSM_LINEAR\n"); 
             csm_pr_linear(&(f->f.lf));
             break;
        case CSM_1D_TABLE:   
             printf("CSM_1D TABLE\n"); 
             printf("normal function:\n");
             csm_pr_1d_table(&(f->f.tf_1));
             break;
	case CSM_2D_TABLE:
             printf("CSM_2D_TABLE\n");
	     csm_pr_2d_table(&(f->f.tf_2));
             break;
        default: 
             printf("%d (unknown)\n", f->type);
             break;
      };
  }
        

/*@EM("#endif\n") */
