/*******************************************************************************
 *
 * Copyright (c) 2003 Cypress Semiconductor
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef CY7C67200_300_DEBUG_H
#define CY7C67200_300_DEBUG_H

#define CONFIG_USB_DEBUG
#ifdef CONFIG_USB_DEBUG

/* defined in cy7c67200_30_lcd.c */
extern int cy_dbg_on;

#define cy_dbg(format, arg...) \
    if( cy_dbg_on != 0 ) \
        printk(KERN_DEBUG __FILE__ ":"  "%d: " format ,__LINE__, ## arg)
     /* printk(KERN_DEBUG __FILE__ ":"  "%d: " format "\n" ,__LINE__, ## arg)*/

#else

#define cy_dbg(format, arg...) do {} while (0)

#endif


#define cy_dbg_sri(format, arg...) \
          printk(format, ## arg)


#define cy_err(format, arg...) \
  printk(KERN_ERR __FILE__ ":"  "%d: " format "\n" ,__LINE__, ## arg)

#define cy_info(format, arg...) \
  printk(KERN_INFO __FILE__ ":"  "%d: " format "\n" ,__LINE__, ## arg)

#define cy_warn(format, arg...) \
  printk(KERN_WARNING __FILE__ ":"  "%d: " format "\n" ,__LINE__, ## arg)

#endif  /* CY7C67200_300_DEBUG_H */
