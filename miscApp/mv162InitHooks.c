/******************************************************************************
 *
 * $RCSfile: mv162InitHooks.c,v $ (This file is best viewed with a tabwidth of 4)
 *
 ******************************************************************************
 *
 * Author: Lange & Franksen
 *
 *		Standard mv162 init hook
 *
 * $Revision: 1.1 $
 *
 * $Date: 1998/02/04 15:49:05 $
 *
 * $Author: bergl $
 *
 * $Log: mv162InitHooks.c,v $
 * Revision 1.1  1998/02/04 15:49:05  bergl
 * New InitHooks
 *
 *
 * This software is copyrighted by the BERLINER SPEICHERRING 
 * GESELLSCHAFT FUER SYNCHROTRONSTRAHLUNG M.B.H., BERLIN, GERMANY. 
 * The following terms apply to all files associated with the 
 * software. 
 *
 * BESSY hereby grants permission to use, copy, and modify this 
 * software and its documentation for non-commercial educational or 
 * research purposes, provided that existing copyright notices are 
 * retained in all copies. 
 *
 * The receiver of the software provides BESSY with all enhancements, 
 * including complete translations, made by the receiver.
 *
 * IN NO EVENT SHALL BESSY BE LIABLE TO ANY PARTY FOR DIRECT, 
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING 
 * OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY 
 * DERIVATIVES THEREOF, EVEN IF BESSY HAS BEEN ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 *
 * BESSY SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR 
 * A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS 
 * PROVIDED ON AN "AS IS" BASIS, AND BESSY HAS NO OBLIGATION TO 
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR 
 * MODIFICATIONS. 
 *
 ******************************************************************************
 *
 * Notes:
 *
 */


#include	<vxWorks.h>
#include	<stdio.h>
#include	<initHooks.h>

#ifdef mv162
#include	<errMdef.h>
#include	<devLib.h>
#endif

void mv162Hooks(int callNumber)
{
#ifdef mv162
	if (INITHOOKatBeginning == callNumber)
	{  /* Register the MV162's VME2chip addresses that are
	      always mapped onto the VME (for mp support) */
	   void* mv162_gcsr = (void*) 0xc200;
	   void* dummy = NULL;
	   devRegisterAddress(
	      "MV162 VME2chip GCSR",
	      atVMEA16,
	      mv162_gcsr,
	      0x100,
	      &dummy);
	}
#endif
}
